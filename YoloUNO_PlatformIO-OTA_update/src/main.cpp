#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <OTA_Firmware_Update.h>
#include <ThingsBoard.h>
#include <Shared_Attribute_Update.h>
#include <Attribute_Request.h>
#include <Espressif_Updater.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Esp32Servo.h>
#include <HTTPClient.h>
#include <Manage.h>


// RFID reader pins
#define SS_PIN    GPIO_NUM_5   // GPIO5 - SDA
#define RST_PIN   GPIO_NUM_4   // GPIO4 - RST
#define SCK_PIN   GPIO_NUM_18
#define MOSI_PIN  GPIO_NUM_23
#define MISO_PIN  GPIO_NUM_19

// Servo pin & angles
#define SERVO_PIN GPIO_NUM_25
#define SERVO_OPEN_ANGLE 90
#define SERVO_CLOSE_ANGLE 0



Servo doorServo;
MFRC522 rfid(SS_PIN, RST_PIN);

String activeUID = "";
unsigned long startTime = 0;

// Task handles
TaskHandle_t RFIDTaskHandle = NULL;
TaskHandle_t OTATaskHandle = NULL;

// WiFi & ThingsBoard config
constexpr char WIFI_SSID[] = "HCMUT36";
constexpr char WIFI_PASSWORD[] = "12345679";
constexpr char TOKEN[] = "h4bifhszk7kt6qa75y5b";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;
constexpr uint16_t TELEMETRY_INTERVAL = 5000U;

// OTA config
constexpr char CURRENT_FIRMWARE_TITLE[] = "OTA_RFID";
constexpr char CURRENT_FIRMWARE_VERSION[] = "1.1";
constexpr uint8_t FIRMWARE_FAILURE_RETRIES = 12U;
constexpr uint16_t FIRMWARE_PACKET_SIZE = 4096U;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
OTA_Firmware_Update<> ota;
Shared_Attribute_Update<1U, 2U> shared_update;
Attribute_Request<2U, 2U> attr_request;
Espressif_Updater<> updater;

const std::array<IAPI_Implementation*, 3U> apis = {
  &shared_update,
  &attr_request,
  &ota
};

ThingsBoard tb(mqttClient, 512U, 512U, Default_Max_Stack_Size, apis);

bool shared_update_subscribed = false;
bool currentFWSent = false;
bool updateRequestSent = false;
bool requestedShared = false;
uint32_t previousTelemetrySend = 0;

constexpr std::array<const char*, 1U> SHARED_ATTRIBUTES = {"doorState"};

void update_starting_callback() {}
void finished_callback(const bool & success) {
  Serial.println(success ? "Update done. Rebooting..." : "Firmware update failed");
  if (success) esp_restart();
}
void progress_callback(const size_t & current, const size_t & total) {
  Serial.printf("OTA Progress: %.2f%%\n", static_cast<float>(current * 100U) / total);
}
void requestTimedOut() {
  Serial.printf("Attribute request timed out after %llu microseconds.\n", 10000000ULL);
}
void processSharedAttributeUpdate(const JsonObjectConst &data) {
  if (data.containsKey("doorState")) {
    String state = data["doorState"].as<String>();
    state.toUpperCase();

    if (state == "OPEN") {
      doorServo.write(SERVO_OPEN_ANGLE);
      Serial.println("🔓 Mở khóa từ xa");
    } else if (state == "CLOSE") {
      doorServo.write(SERVO_CLOSE_ANGLE);
      Serial.println("🔒 Đóng khóa từ xa");
    }
  }

  const size_t jsonSize = Helper::Measure_Json(data);
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);
  Serial.println(buffer);
}
void processSharedAttributeRequest(const JsonObjectConst &data) {
  const size_t jsonSize = Helper::Measure_Json(data);
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);
  Serial.println(buffer);
}

// Initialize WiFi
void InitWiFi() {
  Serial.println("Connecting to AP...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected.");
}
bool reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    InitWiFi();
  }
  return true;
}

// OTA task
void OTATask(void *pvParameters) {
  while (1) {
    if (!reconnect()) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (!tb.connected()) {
      Serial.printf("Connecting to: %s with token: %s\n", THINGSBOARD_SERVER, TOKEN);
      if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
        Serial.println("Failed to connect");
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }

      if (!requestedShared) {
        Serial.println("Requesting shared attributes...");
        const Attribute_Request_Callback<2U> sharedCallback(
          &processSharedAttributeRequest,
          10000000ULL,
          &requestTimedOut,
          SHARED_ATTRIBUTES
        );
        requestedShared = attr_request.Shared_Attributes_Request(sharedCallback);
      }

      if (!shared_update_subscribed) {
        const Shared_Attribute_Callback<2U> callback(&processSharedAttributeUpdate, SHARED_ATTRIBUTES);
        shared_update_subscribed = shared_update.Shared_Attributes_Subscribe(callback);
        Serial.println(shared_update_subscribed ? "Shared attributes subscribed." : "Failed to subscribe.");
      }
    }

    if (!currentFWSent) {
      currentFWSent = ota.Firmware_Send_Info(CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION);
    }

    if (!updateRequestSent) {
      Serial.println("Checking firmware update...");
      const OTA_Update_Callback callback(
        CURRENT_FIRMWARE_TITLE, CURRENT_FIRMWARE_VERSION,
        &updater,
        &finished_callback,
        &progress_callback,
        &update_starting_callback,
        FIRMWARE_FAILURE_RETRIES,
        FIRMWARE_PACKET_SIZE
      );

      updateRequestSent = ota.Start_Firmware_Update(callback);
      if (updateRequestSent) {
        Serial.println("Subscribing to OTA updates...");
        updateRequestSent = ota.Subscribe_Firmware_Update(callback);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// RFID task
void RFIDTask(void *pvParameters) {
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();
  doorServo.attach(SERVO_PIN);
  doorServo.write(SERVO_CLOSE_ANGLE);
  
  manageSystem.begin();  // Initialize the management system
  Serial.println("RFID task started. Waiting for cards...");

  while (1) {
    manageSystem.checkButton();  // Check in/out button state
    
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      uid += String(rfid.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();

    Serial.println("UID: " + uid);
    
    // Handle RFID card with management system
    manageSystem.handleRFIDCard(uid);

    // Send telemetry data to ThingsBoard
    tb.sendTelemetryData("rfid_uid", uid);
    tb.sendAttributeData("rfid_uid", uid);

    if (activeUID == "") {
      activeUID = uid;
      startTime = millis();
      doorServo.write(SERVO_OPEN_ANGLE);
      Serial.println("🔓 Khóa đã mở.");
      tb.sendAttributeData("doorState", "OPEN");
      tb.sendTelemetryData("doorState", "OPEN");
    } 
    else if (uid == activeUID) {
      unsigned long usedTime = millis() - startTime;
      float minutesUsed = usedTime / 60000.0;
      Serial.println("🔒 Đóng khóa.");
      Serial.printf("⏱ Đã sử dụng: %.2f phút\n", minutesUsed);
      doorServo.write(SERVO_CLOSE_ANGLE);

      tb.sendAttributeData("doorState", "CLOSE");
      tb.sendTelemetryData("doorState", "CLOSE");

      activeUID = "";
      startTime = 0;
    } 

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  delay(1000);
  InitWiFi();
  xTaskCreate(OTATask, "OTATask", 10000, NULL, 1, &OTATaskHandle);
  xTaskCreate(RFIDTask, "RFIDTask", 10000, NULL, 1, &RFIDTaskHandle);
}

void loop() {
  // Nothing here
}
