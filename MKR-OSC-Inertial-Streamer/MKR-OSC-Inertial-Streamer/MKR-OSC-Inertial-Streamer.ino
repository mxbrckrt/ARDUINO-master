/*
 * ===============================================================================
 * PROJECT     : MKR-OSC-Inertial-Streamer (Final Stable Version)
 * AUTHOR      : Max Bruckert
 * DATE        : February 20, 2026
 * LICENSE     : MIT
 * HARDWARE    : Arduino MKR WiFi 1010 + Analog Accelerometer
 * * LED BEHAVIOR:
 * - Rapid blinking : Connecting to WiFi (SSID: bruckertbox)
 * - Slow Breathing : WiFi OK, waiting for OSC "Ping" on Port 8000
 * - Solid ON       : Master found, streaming data to Port 9000
 * - 3 Sharp Flashes: Device is shutting down
 * ===============================================================================
 */

#include <WiFiNINA.h>          
#include <WiFiUdp.h>           
#include <OSCMessage.h>        
#include <OSCBundle.h>         
#include <FlashStorage.h>      
#include <ArduinoLowPower.h>   
#include <math.h>              

// --- 1. NETWORK CONFIGURATION ---
char ssid[] = "airportexpressbruckert";     
char pass[] = "bruckertbox";
const int localPort = 8000;    
const int destPort = 9000;     

// --- 2. SYSTEM VARIABLES ---
IPAddress currentMasterIP;     
bool hasMaster = false;        
const uint8_t PIN_BUTTON = 7;  
const uint8_t PIN_X = A1, PIN_Y = A2, PIN_Z = A3;

typedef struct { float offset[3]; bool valid; } CalibrationData;
FlashStorage(my_flash_store, CalibrationData);

WiFiUDP udp;
float offset[3] = {0.0f, 0.0f, 0.0f}; 
float prevEnergy = 0;
float batteryLevel = 0;
unsigned long lastMicros = 0;
const unsigned long intervalMicros = 5000; // 200 Hz
unsigned long lastWifiCheck = 0;
unsigned long lastBatteryCheck = 0;
unsigned long buttonPressTime = 0;
bool isShuttingDown = false;

// --- 3. HELPER FUNCTIONS ---

float readBattery() {
  int rawBat = analogRead(ADC_BATTERY);
  // ADC_BATTERY on MKR1010 has an internal divider. 
  // 4.3 is a common factor for LiPo voltage calculation on this board.
  float voltage = rawBat * (4.3 / 4095.0); 
  return voltage;
}

void goToSleep() {
  digitalWrite(LED_BUILTIN, LOW);
  WiFi.end();
  LowPower.attachInterruptWakeup(PIN_BUTTON, [](){}, FALLING);
  LowPower.deepSleep(); 
}

float readOversampled(int pin) {
  long sum = 0;
  for(int i=0; i<8; i++) sum += analogRead(pin);
  return sum / 8.0f;
}

void sendInfo() {
    if (!hasMaster) return;
    OSCMessage m("/info");
    IPAddress ip = WiFi.localIP();
    char ipBuf[16];
    sprintf(ipBuf, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    m.add("IP");       m.add(ipBuf);
    m.add("author");   m.add("Max Bruckert");
    m.add("battery");  m.add(readBattery());
    udp.beginPacket(currentMasterIP, destPort);
    m.send(udp);
    udp.endPacket();
}

void calibrate() {
  digitalWrite(LED_BUILTIN, HIGH);
  float sum[3] = {0,0,0};
  for(int i=0; i<100; i++) {
    sum[0] += readOversampled(PIN_X); 
    sum[1] += readOversampled(PIN_Y); 
    sum[2] += readOversampled(PIN_Z);
    delay(2);
  }
  CalibrationData cd;
  for(int j=0; j<3; j++) { 
    offset[j] = (sum[j] / 100.0f) - 2048.0f; 
    cd.offset[j] = offset[j]; 
  }
  cd.valid = true; 
  my_flash_store.write(cd);
  for(int k=0; k<10; k++) { digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); delay(30); }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  analogReadResolution(12);
  
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH); delay(100); digitalWrite(LED_BUILTIN, LOW); delay(100); 
  }
  udp.begin(localPort);

  CalibrationData cd = my_flash_store.read();
  if(cd.valid) { for(int i=0; i<3; i++) offset[i] = cd.offset[i]; }
  batteryLevel = readBattery();
}

void loop() {
  // A. POWER CONTROL
  bool buttonState = !digitalRead(PIN_BUTTON);
  if (buttonState) {
    if (buttonPressTime == 0) buttonPressTime = millis();
    if (millis() - buttonPressTime > 4000 && !isShuttingDown) {
      isShuttingDown = true;
      for(int i=0; i<3; i++) { digitalWrite(LED_BUILTIN, HIGH); delay(100); digitalWrite(LED_BUILTIN, LOW); delay(100); }
      goToSleep();
    }
  } else { buttonPressTime = 0; isShuttingDown = false; }

  // B. WATCHDOGS
  if (WiFi.status() != WL_CONNECTED && (millis() - lastWifiCheck > 5000)) {
    lastWifiCheck = millis(); 
    WiFi.begin(ssid, pass);
    if (WiFi.status() == WL_CONNECTED) udp.begin(localPort);
  }
  
  if (millis() - lastBatteryCheck > 10000) {
    lastBatteryCheck = millis();
    batteryLevel = readBattery();
  }

  // C. HANDSHAKE
  int size = udp.parsePacket();
  if (size > 0) {
    currentMasterIP = udp.remoteIP();
    hasMaster = true; 
    OSCMessage msgIn;
    while (size--) msgIn.fill(udp.read());
    if (msgIn.fullMatch("/calibrate")) calibrate();
    if (msgIn.fullMatch("/get_info"))  sendInfo();
  }

  // D. STREAMING
  if (hasMaster) {
    unsigned long currentMicros = micros();
    if (currentMicros - lastMicros >= intervalMicros) {
      lastMicros = currentMicros;
      OSCBundle b;

      float rawX = readOversampled(PIN_X);
      float rawY = readOversampled(PIN_Y);
      float rawZ = readOversampled(PIN_Z);

      // G-Force Math
      float gx = (rawX - offset[0] - 2048.0f) / 1024.0f; 
      float gy = (rawY - offset[1] - 2048.0f) / 1024.0f;
      float gz = (rawZ - offset[2] - 2048.0f) / 1024.0f;

      float energy = fabs(sqrt(gx*gx + gy*gy + gz*gz) - 1.0f);
      float jerk = fabs(energy - prevEnergy);
      prevEnergy = energy;

      // Pack Everything
      b.add("/accelx").add((rawX - offset[0]) / 4095.0f);
      b.add("/accely").add((rawY - offset[1]) / 4095.0f);
      b.add("/accelz").add((rawZ - offset[2]) / 4095.0f);
      b.add("/roll").add(atan2(gy, gz) * 57.29578f);
      b.add("/pitch").add(atan2(-gx, sqrt(gy*gy + gz*gz)) * 57.29578f);
      b.add("/energy").add(energy);
      b.add("/jerk").add(jerk);
      b.add("/battery").add(batteryLevel);
      b.add("/timestamp").add((float)millis());

      if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, HIGH); 
        udp.beginPacket(currentMasterIP, destPort);
        b.send(udp);
        udp.endPacket();
      } else {
        digitalWrite(LED_BUILTIN, (millis() % 500 < 250)); 
      }
    }
  } else {
    digitalWrite(LED_BUILTIN, (millis() % 2000 < 1000));
  }
}