/*
===============================================================================
 ULTRA-LOW-LATENCY OSC STREAMER — MKR WiFi 1010
 Includes clipping, smoothing and gamma correction
===============================================================================

DOCUMENTATION
-------------------------------------------------------------------------------
1) inrange(value)
   Clips analog values to a fixed numeric range before further processing.
   Default: 0..1023

2) smoothing(previous, current)
   Lightweight exponential smoothing to reduce jitter.
   Lower values = more smoothing, slower response.
   Higher values = less smoothing, faster response.
   Ultra-low-latency default: 0.15

3) gammaCorrection(value, gamma)
   Applies perceptual curve shaping.
   - gamma = 1.0 → no change (linear)
   - gamma > 1.0 → more sensitivity in lower range
   - gamma < 1.0 → more sensitivity in upper range

4) OSC Messages:
   /analog/Ax          (int)   raw ADC 0..1023
   /analog-one/Ax      (float) clipped → smoothed → gamma → 0..1 scale
   /analog-hundred/Ax  (float) same but scaled to -100..100

   Messages are only sent when value changes by more than EPSILON.

===============================================================================
*/

#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>

// ---------------------------------------------------------------------------
// CONFIGURATION
// ---------------------------------------------------------------------------
IPAddress DEST_IP(192,168,X,X); // Your computer IP address
const int DEST_PORT = XXX; // Your computer input port

const char* SSID     = "XXX"; // Wifi SSID the Arduino should connect to
const char* PASSWORD = "XXX"; // Wifi password

WiFiUDP udp;

const float EPS = 0.001f;       // change filter threshold
float GAMMA = 1.4f;             // editable: perceptual curve shaping

// ---------------------------------------------------------------------------
// ANALOG & DIGITAL PINS
// ---------------------------------------------------------------------------
const uint8_t A_PINS[] = {A0,A1,A2,A3,A4,A5,A6};
const uint8_t N_A = 7;

const uint8_t D_PINS[] = {2,3,4,5,6,7,8,9,10,11,12,13,14}; // Pins 0 and 1 are ignored as they also act as Rx Tx.
const uint8_t N_D = 13;

// ---------------------------------------------------------------------------
// STATE BUFFERS
// ---------------------------------------------------------------------------
uint16_t lastRaw[N_A]     = {0};
float    lastOne[N_A]     = {0};
float    lastHundred[N_A] = {0};
uint8_t  lastDig[N_D]     = {0};

// ---------------------------------------------------------------------------
// USER-EDITABLE PROCESSING FUNCTIONS
// ---------------------------------------------------------------------------

// 1) CLIPPING : change the mimum and maximum range to clip the raw values. 
// This will be "coocked" as /analog-one/Ax -> 0..1 scale AND /analog-hundred/Ax -> -100..100
float inrange(float v) {
    if (v < 0) return 0.0f; // minimum clip
    if (v > 1023) return 1023.0f; // maximum clip
    return v;
}

// 2) SMOOTHING (lightweight and ultra-fast)
float smoothing(float prev, float cur) {
    const float k = 0.15f; // editable smoothing coefficient
    return prev + (cur - prev) * k;
}

// 3) GAMMA CORRECTION
float gammaCorrection(float v, float g) {
    // v must be 0..1
    return powf(v, g);
}

// ---------------------------------------------------------------------------
// SEND OSC (minimal, no bundle overhead)
// ---------------------------------------------------------------------------
void sendOSC(const char* addr, float v) {
    OSCMessage m(addr);
    m.add(v);
    udp.beginPacket(DEST_IP, DEST_PORT);
    m.send(udp);
    udp.endPacket();
}

void sendOSC(const char* addr, int v) {
    OSCMessage m(addr);
    m.add((int32_t)v);
    udp.beginPacket(DEST_IP, DEST_PORT);
    m.send(udp);
    udp.endPacket();
}

// ---------------------------------------------------------------------------
// WIFI
// ---------------------------------------------------------------------------
void wifiConnect() {
    while (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(SSID, PASSWORD);
        delay(1200);
    }
}

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    wifiConnect();
    udp.begin(DEST_PORT);

    for (uint8_t i=0; i<N_D; i++)
        pinMode(D_PINS[i], INPUT_PULLUP);

    analogReadResolution(10);
}

// ---------------------------------------------------------------------------
// MAIN LOOP (no delay → ultra-low-latency)
// ---------------------------------------------------------------------------
void loop() {

    // ------------------
    // ANALOG
    // ------------------
    for (uint8_t i=0; i<N_A; i++) {

        analogRead(A_PINS[i]);              // discard first sample
        int raw = analogRead(A_PINS[i]);    // real ADC result

        // RAW
        if (raw != lastRaw[i]) {
            char addr[24];
            sprintf(addr, "/analog/A%d", i);
            sendOSC(addr, raw);
            lastRaw[i] = raw;
        }

        // PROCESSING
        float clipped = inrange((float)raw);

        static float smoothVal[7] = {0};
        float s = smoothing(smoothVal[i], clipped);
        smoothVal[i] = s;

        float vNorm = s * 0.0009775171f;    // 1/1023
        float vGamma = gammaCorrection(vNorm, GAMMA);

        // 0..1 OUTPUT
        if (fabs(vGamma - lastOne[i]) > EPS) {
            char addr[32];
            sprintf(addr, "/analog-one/A%d", i);
            sendOSC(addr, vGamma);
            lastOne[i] = vGamma;
        }

        // -100..100 OUTPUT
        float vHund = (vGamma * 200.0f) - 100.0f;

        if (fabs(vHund - lastHundred[i]) > EPS) {
            char addr[32];
            sprintf(addr, "/analog-hundred/A%d", i);
            sendOSC(addr, vHund);
            lastHundred[i] = vHund;
        }
    }

    // ------------------
    // DIGITAL
    // ------------------
    for (uint8_t i=0; i<N_D; i++) {
        uint8_t v = digitalRead(D_PINS[i]);
        if (v != lastDig[i]) {
            char addr[24];
            sprintf(addr, "/digital/D%d", D_PINS[i]);
            sendOSC(addr, v);
            lastDig[i] = v;
        }
    }

    // TIMESTAMP (always sent)
    sendOSC("/timestamp", (int)millis());
}
