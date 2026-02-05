#include <Arduino.h>
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    WiFi.mode(WIFI_STA);
    
    uint8_t mac[6];
    WiFi.macAddress(mac);
    
    Serial.println("\n=== DIRECCIÓN MAC ===");
    Serial.printf("String: %s\n", WiFi.macAddress().c_str());
    Serial.print("Array:  {");
    for (int i = 0; i < 6; i++) {
        Serial.printf("0x%02X", mac[i]);
        if (i < 5) Serial.print(", ");
    }
    Serial.println("}");
}

void loop() {
    delay(1000);
}
