/*
================================================================================
  OSC Sensor Streamer for Arduino MKR WiFi 1010
  Author: Max Bruckert

  DESCRIPTION
  ------------------------------------------------------------------------------
  - Reads multiple analog and digital inputs
  - Sends OSC messages over UDP using ONE bundle per cycle (highest efficiency)
  - Includes change filtering (values sent only when changed)
  - Adds timing messages:
        /timestamp    -> raw milliseconds since boot
        /rtimestamp   -> human-readable runtime (HH:MM:SS:MS)
        /clock        -> SAME as /rtimestamp (relative clock, no RTC required)

  OSC ADDRESS FORMAT
  ------------------------------------------------------------------------------
  /analog/A0 <value>
  /analog/A1 <value>
  ...
  /digital/D2 <value>
  /digital/D3 <value>
  ...

  The following messages are ALWAYS included:
  /timestamp <ms>
  /rtimestamp "HH:MM:SS:MS"
  /clock      "HH:MM:SS:MS"    <-- relative runtime, not real date/time

  Designed for MaxMSP, Pure Data, VVVV, Touch Designer, ar any other OSC compatible software


  PLEASE INSTALL WiFiNINA, WiFiUdp and OSCBundle from your Library manager in the Arduino IDE
================================================================================
*/

#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <OSCBundle.h>

// -----------------------------------------------------------------------------
// NETWORK CONFIGURATION
// -----------------------------------------------------------------------------
IPAddress DESTINATION_IP(192, 168, 1, 140);   // <-- CHANGE THIS: Target computer
const unsigned int DESTINATION_PORT = 3033;   // <-- CHANGE THIS IF NEEDED:incoming port on the target computer
const unsigned int SEND_INTERVAL = 5;       // ms

const char* WIFI_SSID     = "YOUR_WIFI_NETWORK";    // <-- CHANGE THIS: Wifi network it should connect to
const char* WIFI_PASSWORD = "YOUR_PASSWORD";   //<-- CHANGE THIS: Wifi password

// -----------------------------------------------------------------------------
// PIN SETUP
// -----------------------------------------------------------------------------
const int analogPins[]  = {A0, A1, A2, A3, A4, A5, A6};
const int numAnalogPins = sizeof(analogPins) / sizeof(analogPins[0]);

const int digitalPins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}; // IGNORE pins 0 and 1 as they are used for Tx and Rx
const int numDigitalPins = sizeof(digitalPins) / sizeof(digitalPins[0]);

int lastAnalogValues[7]   = {0};
int lastDigitalValues[13] = {0};

WiFiUDP Udp;

// -----------------------------------------------------------------------------
// CONNECT TO WIFI
// -----------------------------------------------------------------------------
void connectToWiFi() {
  WiFi.disconnect();
  delay(200);

  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(2000);
  }
}

// -----------------------------------------------------------------------------
// RELATIVE TIME FORMATTER (HH:MM:SS:MS)
// -----------------------------------------------------------------------------
void buildRelativeTime(char* buffer, size_t size) {
  unsigned long t  = millis();
  unsigned long ms = t % 1000;
  unsigned long s  = (t / 1000) % 60;
  unsigned long m  = (t / 60000) % 60;
  unsigned long h  = (t / 3600000);

  snprintf(buffer, size, "%02lu:%02lu:%02lu:%03lu", h, m, s, ms);
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  connectToWiFi();
  Udp.begin(DESTINATION_PORT);

  // Setup digital pins as inputs
  for (int i = 0; i < numDigitalPins; i++) {
    pinMode(digitalPins[i], INPUT_PULLUP);
  }
}

// -----------------------------------------------------------------------------
// MAIN LOOP
// -----------------------------------------------------------------------------
void loop() {

  OSCBundle bundle;

  // ------------------------
  // ANALOG INPUTS (change filter)
  // ------------------------
  for (int i = 0; i < numAnalogPins; i++) {
    int value = analogRead(analogPins[i]);
    if (value != lastAnalogValues[i]) {
      char address[32];
      snprintf(address, sizeof(address), "/analog/A%d", i);
      bundle.add(address).add((int32_t)value);
      lastAnalogValues[i] = value;
    }
  }

  // ------------------------
  // DIGITAL INPUTS (change filter)
  // ------------------------
  for (int i = 0; i < numDigitalPins; i++) {
    int pin = digitalPins[i];
    int value = digitalRead(pin);
    if (value != lastDigitalValues[i]) {
      char address[32];
      snprintf(address, sizeof(address), "/digital/D%d", pin);
      bundle.add(address).add((int32_t)value);
      lastDigitalValues[i] = value;
    }
  }

  // ------------------------
  // RAW TIMESTAMP (always)
  // ------------------------
  bundle.add("/timestamp").add((int32_t)millis());

  // ------------------------
  // HUMAN READABLE TIME (always)
  // ------------------------
  char rts[32];
  buildRelativeTime(rts, sizeof(rts));

  bundle.add("/rtimestamp").add(rts);
  bundle.add("/clock").add(rts);   // same content, different OSC name

  // ------------------------
  // SEND BUNDLE
  // ------------------------
  Udp.beginPacket(DESTINATION_IP, DESTINATION_PORT);
  bundle.send(Udp);
  Udp.endPacket();

  delay(SEND_INTERVAL);
}
