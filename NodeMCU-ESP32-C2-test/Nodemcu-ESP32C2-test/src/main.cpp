#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("--- ESP32-C2 OPERATIONNEL ---");
}

void loop() {
  Serial.println("Hello Mac M2 !");
  delay(2000);
}