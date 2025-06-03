#include "Manage.h"

ManageSystem manageSystem;

ManageSystem::ManageSystem() {
    studentCount = 0;
    inOutState = false;
    btnTimeDelay = 0;
    btnIOState = HIGH;
    webAppUrl = "https://script.google.com/macros/s/AKfycbxNEWLINK/exec"; 
}

void ManageSystem::begin() {
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BTN_IO_PIN, INPUT_PULLUP);
    pinMode(LED_IO_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_IO_PIN, LOW);
    
    if(!readDataSheet()) {
        Serial.println("Failed to read student data!");
    }
}

bool ManageSystem::readDataSheet() {
    if (WiFi.status() == WL_CONNECTED) {
        String Read_Data_URL = webAppUrl + "?sts=read";
        HTTPClient http;
        
        http.begin(Read_Data_URL.c_str());
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        int httpCode = http.GET();
        if (httpCode > 0) {
            String payload = http.getString();
            studentCount = 0;
            
            DynamicJsonDocument doc(1024);
            deserializeJson(doc, payload);
            
            JsonArray array = doc.as<JsonArray>();
            for(JsonVariant v : array) {
                if(studentCount >= MAX_STUDENTS) break;
                
                students[studentCount].id = v[0].as<String>();
                strlcpy(students[studentCount].code, v[1].as<const char*>(), sizeof(students[studentCount].code));
                strlcpy(students[studentCount].name, v[2].as<const char*>(), sizeof(students[studentCount].name));
                
                studentCount++;
            }
            
            http.end();
            return studentCount > 0;
        }
        http.end();
    }
    return false;
}

void ManageSystem::handleRFIDCard(const String& uid) {
    char uidChar[uid.length() + 1];
    uid.toCharArray(uidChar, uid.length() + 1);
    char* studentName = getStudentNameById(uidChar);
    
    if (studentName != nullptr) {
        beep(1, 200);
        writeLogSheet(uid, String(studentName));
    } else {
        beep(3, 500);
        Serial.println("Unknown UID: " + uid);
    }
}

void ManageSystem::checkButton() {
    if(digitalRead(BTN_IO_PIN) == LOW) {
        if(btnIOState == HIGH && (millis() - btnTimeDelay > 500)) {
            inOutState = !inOutState;
            digitalWrite(LED_IO_PIN, inOutState);
            btnTimeDelay = millis();
        }
        btnIOState = LOW;
    } else {
        btnIOState = HIGH;
    }
}

// Implement các phương thức còn lại...
String ManageSystem::urlencode(String str) {
    String encodedString = "";
    char c;
    char code0;
    char code1;
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') encodedString += '+';
        else if (isalnum(c)) encodedString += c;
        else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) code0 = c - 10 + 'A';
            encodedString += '%';
            encodedString += code0;
            encodedString += code1;
        }
        yield();
    }
    return encodedString;
}

char* ManageSystem::getStudentNameById(const char* uid) {
    for (int i = 0; i < studentCount; i++) {
        if (strcmp(students[i].code, uid) == 0) {
            return students[i].name;
        }
    }
    return nullptr;
}

void ManageSystem::beep(int n, int d) {
    for(int i = 0; i < n; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(d);
        digitalWrite(BUZZER_PIN, LOW);
        delay(d);
    }
}

bool ManageSystem::writeLogSheet(const String& uid, const String& studentName) {
    String Send_Data_URL = webAppUrl + "?sts=writelog";
    Send_Data_URL += "&uid=" + uid;
    Send_Data_URL += "&name=" + urlencode(studentName);
    Send_Data_URL += "&inout=" + urlencode(inOutState ? "RA" : "VÀO");
    
    HTTPClient http;
    http.begin(Send_Data_URL.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
        String payload = http.getString();
        Serial.println("Response: " + payload);
        http.end();
        return true;
    }
    http.end();
    return false;
}