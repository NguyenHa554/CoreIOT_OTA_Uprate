#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>

constexpr char WIFI_SSID[] = "T.V.H";
constexpr char WIFI_PASSWORD[] = "12345678";
constexpr char TOKEN[] = "h4bifhszk7kt6qa75y5b";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883;

constexpr uint32_t MAX_MESSAGE_SIZE = 1024;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200;
constexpr uint16_t SEND_INTERVAL = 5000;

WiFiClient espClient;
Arduino_MQTT_Client mqttClient(espClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

String validUIDs[] = { "04A1B2C3D4", "123456789" }; // UID hợp lệ

void InitWiFi() {
  Serial.println("🔌 Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi.");
}

bool reconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    InitWiFi();
  }

  if (!tb.connected()) {
    Serial.printf("🌐 Connecting to ThingsBoard (%s)...\n", THINGSBOARD_SERVER);
    return tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT);
  }
  return true;
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  InitWiFi();
  Serial.println("=== RFID Serial Simulation ===");
  Serial.println("Nhập UID RFID:");
}

void loop() {
  if (!reconnect()) return;

  if (tb.connected()) {
    tb.loop();
  }

  // Giả lập quét UID qua Serial
  if (Serial.available()) {
    String uid = Serial.readStringUntil('\n');
    uid.trim();
    Serial.println("📥 UID nhập: " + uid);

    // Kiểm tra UID hợp lệ
    bool authorized = false;
    for (String valid : validUIDs) {
      if (uid == valid) {
        authorized = true;
        break;
      }
    }

    if (authorized) {
      Serial.println("✅ UID hợp lệ. Gửi lên ThingsBoard...");
    } else {
      Serial.println("❌ UID không hợp lệ. Vẫn gửi lên để test.");
    }

    // Gửi dữ liệu lên ThingsBoard
    tb.sendTelemetryData("rfid_uid", uid.c_str());
    tb.sendTelemetryData("access_granted", authorized); // true/false
    tb.sendAttributeData("last_uid", uid.c_str());
  }
}
