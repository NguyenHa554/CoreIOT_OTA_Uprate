#ifndef MANAGE_H
#define MANAGE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#define MAX_STUDENTS 10
#define BUZZER_PIN GPIO_NUM_13
#define BTN_IO_PIN GPIO_NUM_14
#define LED_IO_PIN GPIO_NUM_12

struct Student {
    String id;
    char code[10];
    char name[30];
};

class ManageSystem {
public:
    ManageSystem();
    void begin();
    bool readDataSheet();
    bool writeLogSheet(const String& uid, const String& studentName);
    char* getStudentNameById(const char* uid);
    void handleRFIDCard(const String& uid);
    void checkButton();
    void beep(int n, int d);

private:
    Student students[MAX_STUDENTS];
    int studentCount;
    bool inOutState;  // false: vào, true: ra
    unsigned long btnTimeDelay;
    bool btnIOState;
    String webAppUrl;
    String urlencode(String str);
};

extern ManageSystem manageSystem;

#endif // MANAGE_H