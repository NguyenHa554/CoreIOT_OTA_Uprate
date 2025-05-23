#include <SPI.h>
#include <MFRC522.h>

// #define SS_PIN    5   // D5
// #define RST_PIN   4   // D4

// MFRC522 rfid(SS_PIN, RST_PIN);
// unsigned long lastUIDTime = 0;
// String lastUID = "";

// void setup() {
//   Serial.begin(115200);
//   SPI.begin();               // SCK, MISO, MOSI auto map
//   rfid.PCD_Init();           // Init RFID
//   Serial.println("Chạm thẻ RFID...");
// }

// void loop() {
//   // Kiểm tra thẻ mới
//   if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
//     return;
//   }

//   // Đọc UID
//   String uidStr = "";
//   for (byte i = 0; i < rfid.uid.size; i++) {
//     uidStr += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
//     uidStr += String(rfid.uid.uidByte[i], HEX);
//   }
//   uidStr.toUpperCase();

//   Serial.println("Thẻ quét: " + uidStr);

//   // Nếu là cùng UID với lần trước => tính thời gian sử dụng
//   if (uidStr == lastUID) {
//     unsigned long now = millis();
//     float durationMinutes = (now - lastUIDTime) / 60000.0;
//     Serial.printf("UID %s đã sử dụng: %.2f phút\n", uidStr.c_str(), durationMinutes);
//     // reset để lần sau test tiếp
//     lastUID = "";
//   } else {
//     // Lưu UID & thời gian quẹt vào
//     lastUID = uidStr;
//     lastUIDTime = millis();
//     Serial.println("Ghi nhận thời điểm vào...");
//   }

//   rfid.PICC_HaltA();         // Dừng đọc
//   rfid.PCD_StopCrypto1();    // Dừng mã hóa
// }

String rfidUID = "";

void setup() {
  Serial.begin(115200);
  Serial.println("Nhập UID để giả lập quét thẻ:");
}

void loop() {
  if (Serial.available()) {
    rfidUID = Serial.readStringUntil('\n');
    rfidUID.trim();

    Serial.println("Thẻ được quét có UID: " + rfidUID);

    // Giả lập xử lý
    if (rfidUID == "04A1B2C3D4") {
      Serial.println("Truy cập được cho phép.");
    } else {
      Serial.println("Truy cập bị từ chối.");
    }
  }
}
