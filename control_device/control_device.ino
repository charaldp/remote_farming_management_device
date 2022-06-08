#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h";

const char* ssid = "Maggouri"; //Enter SSID
const char* password = "petaloudes"; //Enter Password
int pressure_analog_read = 0;
float pressure = 0;
unsigned long now = 0;
unsigned long time_interval = 0;
unsigned long watering_time_interval = 0;
unsigned long restart_time_interval = 0;

bool is_on = false;
bool temp_is_on = false;
String watering_entry_id = "0";
int httpCode = 0;

void setup() {
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(PRESSURE_READ_PIN, INPUT);
    digitalWrite(RELAY_PIN, LOW);
    Serial.begin(115200);
    // Connect to wifi
    WiFi.begin(ssid, password);

    // Wait some time to connect to wifi
    for(int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
        Serial.print(".");
        delay(1000);
    }

    // Check if connected to wifi
    if(WiFi.status() != WL_CONNECTED) {
        Serial.println("No Wifi!");
        ESP.restart();
    }
    time_interval = millis();
    restart_time_interval = millis();
}

void loop() {
    delay(100);
    now = millis();
    if ((now - time_interval < 2000)
    //    && (
    //        is_on && (
    //            (now - watering_time_interval < 60000 && time_interval < 2000) ||
    //            (now - watering_time_interval < 300000 && time_interval < 10000)
    //        )
    //    )
    )
        return;
    time_interval = now;
    if (is_on && (now - watering_time_interval > 10800000))
        updateIsOn(false);
    if(WiFi.status() != WL_CONNECTED)
        return;
    checkIsOnStatus();
    // get response from the server for watering status
    if (is_on) {
        pressure_analog_read = analogRead(PRESSURE_READ_PIN);
        pressure = 6.875 * (float)pressure_analog_read / 4096.0;
        Serial.println("P="+String(pressure));
        WiFiClient client;
        HTTPClient http;
        String sensor_reading_store_path = String("/api/control_device/")+String(CONTROL_DEVICE_ID)+String("/watering_entry/")+watering_entry_id+String("/sensor_device/")+String(PRESSURE_SENSOR_DEVICE_ID)+String("/sensor_reading/store");
        if (http.begin(client, String(SERVER_HOST) + sensor_reading_store_path)) {
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            httpCode = http.POST(String("measurement_type=pressure")+String("&value=") + String(pressure));
            if (!(httpCode == HTTP_CODE_OK)) {
                Serial.println("Pressure Reading Store, Non regular response code: "+String(httpCode));
                String payload = http.getString();
                Serial.println(sensor_reading_store_path);
                Serial.println(payload);
                return;
            }
            String payload = http.getString();
        }
    }
}

void updateIsOn(bool new_is_on) {
    is_on = new_is_on;
    if (is_on) {
        digitalWrite(RELAY_PIN, HIGH);
        watering_time_interval = millis();
    } else {
        digitalWrite(RELAY_PIN, LOW);
        ESP.restart();
    }
}

void checkIsOnStatus() {
    WiFiClient client;
    HTTPClient http;
    String control_device_path = String("/control_device/")+String(CONTROL_DEVICE_ID)+String("/show");
    if (!http.begin(client, String(SERVER_HOST) + control_device_path))
        return;
    httpCode = http.GET();
    if (!(httpCode == HTTP_CODE_OK)) {
        Serial.println("Control Device Show, Non regular response code: "+String(httpCode));
        Serial.println(control_device_path);
        return;
    }
    const String& payload = http.getString();
    const size_t capacity = JSON_OBJECT_SIZE(6) + JSON_ARRAY_SIZE(2) + 256;
    DynamicJsonDocument doc(capacity);

    DeserializationError error = deserializeJson(doc, payload);
    if (!error) {
        Serial.println("Response:");
        Serial.println(payload);
        Serial.println((String)doc["is_on"].as<int>());
        Serial.println(doc["name"].as<char*>());
        Serial.println((String)doc["watering_entry_id"].as<int>());
                
        if (doc["is_on"].as<int>() == 1) {
            // csrf = (String)doc["csrf"].as<char*>();
            temp_is_on = true;
        } else {
            temp_is_on = false;
        }
        if (temp_is_on != is_on) {
            updateIsOn(temp_is_on);
            watering_entry_id = (String)doc["watering_entry_id"].as<int>();
        }
    } else {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      Serial.println(payload);
    }
}
