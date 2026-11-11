/*
===============================================================================
 ULTRA-LOW-LATENCY OSC STREAMER + 3-AXIS ACCELEROMETER — Arduino MKR WiFi 1010
===============================================================================
 Features:
 - Native 12-bit ADC resolution (0..4095) for 4x finer small-signal tracking
 - 3-Axis Accelerometer Engine (A0=X, A1=Y, A2=Z, e.g. SparkFun ADXL3xx):
     * Auto-zero calibration (3 quick clicks on D7, OSC /accel/calibrate, or Web UI)
     * Digital gain amplification (zoom around calibrated zero)
     * High-pass dynamic filter (isolates motion/impacts from static tilt)
     * Per-axis & 3D magnitude threshold detection with hysteresis & cooldown
 - Momentary (unlatch) command switch on D7 (wired between D7 and GND):
     * /switch/raw         (int 0/1, immediate)
     * /switch/short       (int 1, single click)
     * /switch/double      (int 1, double click)
     * /switch/triple      (int 1, triple click -> triggers zero calibration)
     * Short + Long (>3s)  -> toggles AP Mode ON / OFF
     * Direct Long (>5s)   -> enters Deep Sleep (press D7 to wake up)
 - Wi-Fi Manager & Flash Persistence (no plaintext passwords in source code):
     * Known Wi-Fi networks & processing config stored in FlashStorage
     * AP Mode ("Arduino AP mode" / pass "12345678", IP 192.168.1.128):
       broadcasts OSC on 192.168.1.255:3033 and serves the HTML editor
     * Client Mode (AP Mode OFF): scans and connects to known Wi-Fi networks
 - Onboard LED (D6 / LED_BUILTIN) hardware-PWM status indicator:
     * Solid ON   : Client mode connected to known Wi-Fi
     * Fast blink : Client mode disconnected / searching
     * Slow pulse : AP Mode active
     * OFF        : Deep Sleep
 - Bidirectional OSC over UDP (send on DEST_PORT 3033, receive on LOCAL_OSC_PORT 3034)

 HARDWARE NOTES (MKR WiFi 1010):
 - 3.3V logic ONLY (do not apply 5V to any analog or digital input).
 - Pins 13 and 14 map to hardware Serial1 (RX/TX) on MKR boards.
===============================================================================
*/

#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <ArduinoLowPower.h>
#include <FlashStorage.h>
#include <spi_drv.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// NETWORK & AP CONFIGURATION (NO STATION PASSWORDS HARDCODED IN SOURCE)
// ---------------------------------------------------------------------------
const char* AP_SSID        = "Arduino AP mode";
const char* AP_PASS        = "12345678"; // WPA2 requires >= 8 characters
const IPAddress AP_IP(192, 168, 1, 128);
const IPAddress AP_BCAST_IP(192, 168, 1, 255);
const IPAddress SUBNET_MASK(255, 255, 255, 0);

const int DEFAULT_DEST_PORT = 3033;     // Destination OSC port (Max [udpreceive 3033])
const int LOCAL_OSC_PORT    = 3034;     // Local UDP port for incoming OSC commands

// ---------------------------------------------------------------------------
// FLASH STORAGE SCHEMA (KNOWN WI-FI NETWORKS + SETTINGS)
// ---------------------------------------------------------------------------
#define CFG_MAGIC     0x4D4B5235UL      // "MKR5"
#define MAX_WIFI_NETS 5

struct WifiEntry {
    char    ssid[33];
    char    pass[65];
    uint8_t destIp[4];                  // OSC target computer IP (e.g. 192.168.1.6)
    uint8_t localIp[4];                 // Optional fixed MKR IP (0.0.0.0 = DHCP)
    uint8_t gatewayIp[4];               // Optional gateway (0.0.0.0 = auto .1)
};

struct ConfigStore {
    uint32_t  magic;
    uint8_t   netCount;
    WifiEntry nets[MAX_WIFI_NETS];
    float     accelGain;
    float     accelThresh[3];
    float     accelThreshMag;
    float     smoothK;
    float     gamma;
    uint16_t  streamHz;
    uint16_t  destPort;
    uint8_t   activeAnalogCount;        // 1..7 (default 3 = A0..A2 to avoid floating pin flood)
};

FlashStorage(flashStore, ConfigStore);
ConfigStore cfg;

// Runtime active OSC target
IPAddress DEST_IP(192, 168, 1, 255);
int       DEST_PORT = DEFAULT_DEST_PORT;
char      activeSsid[33] = "";

// ---------------------------------------------------------------------------
// ADC & SIGNAL PROCESSING CONSTANTS (12-bit = 0..4095)
// ---------------------------------------------------------------------------
const float ADC_MAX     = 4095.0f;
float CLIP_MIN          = 0.0f;
float CLIP_MAX          = 4095.0f;
const float EPS_ONE     = 0.003f;       // Change threshold for 0..1 output
const float EPS_HUNDRED = 0.5f;         // Change threshold for -100..100 output
const int   RAW_EPS     = 4;            // 12-bit ADC deadband to prevent noise flood

const uint32_t TIMESTAMP_MS     = 1000;
const uint32_t WEB_POLL_MS      = 15;
const uint32_t WIFI_RESCAN_MS   = 15000;
const float    ACCEL_HP_ALPHA   = 0.01f;
const float    HYST_RATIO       = 0.70f;
const uint32_t TRIG_COOLDOWN_MS = 60;
const float    DIR_DEADBAND     = 0.008f; // Slope deadband for ascending/descending (+1/0/-1)

// ---------------------------------------------------------------------------
// PIN DEFINITIONS
// ---------------------------------------------------------------------------
const uint8_t A_PINS[] = {A0, A1, A2, A3, A4, A5, A6};
const uint8_t N_A      = sizeof(A_PINS) / sizeof(A_PINS[0]);

// Pin 6 (LED_BUILTIN) and Pin 7 (CMD_SWITCH_PIN) are excluded from D_PINS
const uint8_t D_PINS[] = {2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14};
const uint8_t N_D      = sizeof(D_PINS) / sizeof(D_PINS[0]);

#define CMD_SWITCH_PIN 7
const uint32_t SW_DEBOUNCE_MS   = 25;
const uint32_t SW_MULTI_GAP_MS  = 700;  // Window to detect short+long or multi-click
const uint32_t SW_AP_HOLD_MS    = 3000; // Short + hold > 3s toggles AP Mode
const uint32_t SW_SLEEP_HOLD_MS = 5000; // Direct hold > 5s enters Deep Sleep

// ---------------------------------------------------------------------------
// INTERNAL STATE
// ---------------------------------------------------------------------------
WiFiUDP    udp;
WiFiServer webServer(80);

uint16_t lastRaw[N_A]     = {0};
float    smoothVal[N_A]   = {0};
float    lastOne[N_A]     = {-1.0f};
float    lastHundred[N_A] = {-999.0f};
uint8_t  lastDig[N_D]     = {0};

// Accelerometer state (A0=X, A1=Y, A2=Z)
float    accelZero[3]     = {2048.0f, 2048.0f, 2048.0f};
float    accelSlow[3]     = {2048.0f, 2048.0f, 2048.0f};
bool     accelBaseInit    = false;
float    accelZoom[3]     = {0};
float    accelDyn[3]      = {0};
float    accelMag         = 0.0f;
float    lastZoom[3]      = {-99.0f, -99.0f, -99.0f};
float    lastDyn[3]       = {-99.0f, -99.0f, -99.0f};
float    lastMagSent      = -99.0f;
float    prevDynVal[3]    = {0};
float    dynSlope[3]      = {0};
int8_t   accelDir[3]      = {0, 0, 0};  // +1 ascending, -1 descending, 0 steady
bool     trigArmed[4]     = {true, true, true, true}; // 0..2=XYZ, 3=Mag
uint32_t lastTrigMs[4]    = {0};

// Mode flags
bool     apModeActive     = false;
bool     wifiConnected    = false;
bool     streamingPaused  = false;
bool     sleepRequested   = false;
bool     apTogglePending  = false;
bool     reconnectPending = false;

// Switch state (D7)
bool     swStableState    = HIGH;
bool     swLastRaw        = HIGH;
uint32_t swDebounceAt     = 0;
uint32_t swPressedAt      = 0;
uint32_t swLastReleaseAt  = 0;
uint8_t  swClickCount     = 0;
bool     swHoldHandled    = false;

// Non-blocking timers
uint32_t lastSampleUs     = 0;
uint32_t lastTimestampMs  = 0;
uint32_t lastWebPollMs    = 0;
uint32_t lastWifiCheckMs  = 0;
uint32_t lastLedUpdateMs  = 0;
uint32_t udpBackoffUntil  = 0;
uint8_t  wifiMissCount    = 0;

// ---------------------------------------------------------------------------
// FLASH CONFIG PERSISTENCE
// ---------------------------------------------------------------------------
void loadConfig() {
    flashStore.read(&cfg);
    if (cfg.magic != CFG_MAGIC) {
        memset(&cfg, 0, sizeof(cfg));
        cfg.magic             = CFG_MAGIC;
        cfg.netCount          = 0;
        cfg.accelGain         = 5.0f;
        cfg.accelThresh[0]    = 0.60f;
        cfg.accelThresh[1]    = 0.60f;
        cfg.accelThresh[2]    = 0.60f;
        cfg.accelThreshMag    = 0.60f;
        cfg.smoothK           = 0.18f;
        cfg.gamma             = 1.0f;
        cfg.streamHz          = 125;    // 125 Hz default (8ms latency, rock-solid WiFiNINA)
        cfg.destPort          = DEFAULT_DEST_PORT;
        cfg.activeAnalogCount = 3;      // A0, A1, A2 by default (prevents floating A3..A6 flood)
        flashStore.write(cfg);
    }
    if (cfg.netCount > MAX_WIFI_NETS) cfg.netCount = MAX_WIFI_NETS;
    if (cfg.activeAnalogCount < 1 || cfg.activeAnalogCount > N_A) cfg.activeAnalogCount = 3;
    DEST_PORT = cfg.destPort > 0 ? cfg.destPort : DEFAULT_DEST_PORT;
}

void saveConfig() {
    cfg.magic = CFG_MAGIC;
    flashStore.write(cfg);
}

// ---------------------------------------------------------------------------
// SIGNAL PROCESSING HELPERS
// ---------------------------------------------------------------------------
float clipAndNormalize(float v) {
    float span = CLIP_MAX - CLIP_MIN;
    if (span <= 0.0f) return 0.0f;
    if (v <= CLIP_MIN) return 0.0f;
    if (v >= CLIP_MAX) return 1.0f;
    return (v - CLIP_MIN) / span;
}

float applySmoothing(float prev, float cur) {
    return prev + (cur - prev) * cfg.smoothK;
}

float applyGamma(float vNorm, float g) {
    if (vNorm <= 0.0f) return 0.0f;
    if (vNorm >= 1.0f) return 1.0f;
    return powf(vNorm, g);
}

void calibrateAccelZero() {
    for (uint8_t a = 0; a < 3; a++) {
        uint32_t acc = 0;
        for (uint8_t k = 0; k < 16; k++) {
            analogRead(A_PINS[a]);
            acc += analogRead(A_PINS[a]);
            delay(1);
        }
        float avg = acc / 16.0f;
        accelZero[a] = avg;
        accelSlow[a] = avg;
    }
    accelBaseInit = true;
}

// ---------------------------------------------------------------------------
// LED STATUS INDICATOR (PIN D6 / LED_BUILTIN, HARDWARE PWM)
// ---------------------------------------------------------------------------
void updateLedStatus(uint32_t nowMs) {
    if ((nowMs - lastLedUpdateMs) < 20) return;
    lastLedUpdateMs = nowMs;

    if (apModeActive) {
        // Slow smooth breathing pulse (2.4s period)
        float phase = (float)(nowMs % 2400) / 2400.0f * 6.2831853f;
        int pwm = (int)(127.5f - 127.5f * cosf(phase));
        analogWrite(LED_BUILTIN, pwm);
    } else if (wifiConnected) {
        // Solid full ON
        analogWrite(LED_BUILTIN, 255);
    } else {
        // Fast blink (120 ms on / 120 ms off)
        analogWrite(LED_BUILTIN, ((nowMs / 120) % 2) ? 255 : 0);
    }
}

// ---------------------------------------------------------------------------
// OSC SEND & RECEIVE
// ---------------------------------------------------------------------------
bool networkReady() {
    return apModeActive || wifiConnected;
}

// Zero-allocation OSC packet writer (avoids malloc/free heap fragmentation on SAMD21)
void sendOscPacket4(const char* addr, char typeTag, const uint8_t payloadBE[4]) {
    if (!networkReady()) return;
    if ((int32_t)(millis() - udpBackoffUntil) < 0) return;

    size_t addrLen = strlen(addr);
    size_t addrPad = (addrLen + 4) & ~((size_t)3); // Zero-terminated & 4-byte aligned
    if (addrPad + 8 > 64) return;

    uint8_t pkt[64];
    memset(pkt, 0, addrPad + 8);
    memcpy(pkt, addr, addrLen);
    pkt[addrPad]     = ',';
    pkt[addrPad + 1] = (uint8_t)typeTag;
    memcpy(pkt + addrPad + 4, payloadBE, 4);

    if (!udp.beginPacket(DEST_IP, DEST_PORT)) {
        udpBackoffUntil = millis() + 15;
        return;
    }
    udp.write(pkt, addrPad + 8);
    if (!udp.endPacket()) {
        udpBackoffUntil = millis() + 15; // Back off briefly if NINA TX queue is full
    }
}

void sendOSC(const char* addr, float v) {
    uint32_t raw;
    memcpy(&raw, &v, 4);
    uint8_t be[4] = {(uint8_t)(raw >> 24), (uint8_t)(raw >> 16), (uint8_t)(raw >> 8), (uint8_t)raw};
    sendOscPacket4(addr, 'f', be);
}

void sendOSC(const char* addr, int v) {
    int32_t val = (int32_t)v;
    uint8_t be[4] = {(uint8_t)(val >> 24), (uint8_t)(val >> 16), (uint8_t)(val >> 8), (uint8_t)val};
    sendOscPacket4(addr, 'i', be);
}

float argAsFloat(OSCMessage& msg, int idx, float defVal) {
    if (idx >= msg.size()) return defVal;
    if (msg.isFloat(idx)) return msg.getFloat(idx);
    if (msg.isInt(idx)) return (float)msg.getInt(idx);
    return defVal;
}

void onOscGamma(OSCMessage& msg)  { cfg.gamma     = constrain(argAsFloat(msg, 0, cfg.gamma), 0.1f, 5.0f); }
void onOscSmooth(OSCMessage& msg) { cfg.smoothK   = constrain(argAsFloat(msg, 0, cfg.smoothK), 0.01f, 1.0f); }
void onOscRate(OSCMessage& msg)   { cfg.streamHz  = constrain((int)argAsFloat(msg, 0, cfg.streamHz), 10, 1000); }
void onOscPause(OSCMessage& msg)  { streamingPaused = (argAsFloat(msg, 0, 0) != 0.0f); }

void onOscClip(OSCMessage& msg) {
    if (msg.size() >= 2) {
        float mn = constrain(argAsFloat(msg, 0, CLIP_MIN), 0.0f, ADC_MAX - 1.0f);
        float mx = constrain(argAsFloat(msg, 1, CLIP_MAX), mn + 1.0f, ADC_MAX);
        CLIP_MIN = mn;
        CLIP_MAX = mx;
    }
}

void onOscAccelGain(OSCMessage& msg) {
    cfg.accelGain = constrain(argAsFloat(msg, 0, cfg.accelGain), 1.0f, 30.0f);
}

void onOscAccelThresh(OSCMessage& msg) {
    if (msg.size() == 1) {
        float t = constrain(argAsFloat(msg, 0, 0.15f), 0.01f, 2.0f);
        cfg.accelThresh[0] = cfg.accelThresh[1] = cfg.accelThresh[2] = t;
    } else if (msg.size() >= 3) {
        cfg.accelThresh[0] = constrain(argAsFloat(msg, 0, cfg.accelThresh[0]), 0.01f, 2.0f);
        cfg.accelThresh[1] = constrain(argAsFloat(msg, 1, cfg.accelThresh[1]), 0.01f, 2.0f);
        cfg.accelThresh[2] = constrain(argAsFloat(msg, 2, cfg.accelThresh[2]), 0.01f, 2.0f);
    }
}

void onOscAccelThreshMag(OSCMessage& msg) {
    cfg.accelThreshMag = constrain(argAsFloat(msg, 0, cfg.accelThreshMag), 0.01f, 3.0f);
}

void onOscCalibrate(OSCMessage& msg) {
    (void)msg;
    calibrateAccelZero();
    sendOSC("/accel/calibrated", 1);
}

void onOscSleep(OSCMessage& msg) {
    if (argAsFloat(msg, 0, 1.0f) != 0.0f) sleepRequested = true;
}

void onOscSave(OSCMessage& msg) {
    (void)msg;
    saveConfig();
}

void onOscAnalogCount(OSCMessage& msg) {
    cfg.activeAnalogCount = constrain((int)argAsFloat(msg, 0, cfg.activeAnalogCount), 1, N_A);
}

void handleIncomingOSC() {
    if (!networkReady()) return;
    int packetSize = udp.parsePacket();
    while (packetSize > 0) {
        OSCMessage msg;
        while (udp.available()) msg.fill(udp.read());
        if (!msg.hasError()) {
            msg.dispatch("/config/gamma", onOscGamma);
            msg.dispatch("/config/smooth", onOscSmooth);
            msg.dispatch("/config/clip", onOscClip);
            msg.dispatch("/config/rate", onOscRate);
            msg.dispatch("/config/pause", onOscPause);
            msg.dispatch("/config/analogcount", onOscAnalogCount);
            msg.dispatch("/accel/gain", onOscAccelGain);
            msg.dispatch("/accel/thresh", onOscAccelThresh);
            msg.dispatch("/accel/threshmag", onOscAccelThreshMag);
            msg.dispatch("/accel/calibrate", onOscCalibrate);
            msg.dispatch("/system/sleep", onOscSleep);
            msg.dispatch("/system/save", onOscSave);
        }
        packetSize = udp.parsePacket();
    }
}

// ---------------------------------------------------------------------------
// WI-FI CONNECTION (CLIENT MODE & AP MODE) + DEEP SLEEP
// ---------------------------------------------------------------------------
void resetWiFiModule() {
    webServer.end();
    udp.stop();
    WiFi.disconnect();
    SpiDrv::end();
    delay(60);
    SpiDrv::begin(true); // Hardware-resets NINA-W102 so AP<->Client switches cleanly
}

void startAPMode() {
    Serial.println("Starting AP Mode...");
    resetWiFiModule();

    WiFi.config(AP_IP, AP_IP, AP_IP, SUBNET_MASK);
    uint8_t st = WiFi.beginAP(AP_SSID, AP_PASS);
    if (st != WL_AP_LISTENING) {
        Serial.println("ERROR: Failed to start AP Mode.");
        apModeActive = false;
        wifiConnected = false;
        return;
    }

    apModeActive  = true;
    wifiConnected = false;
    activeSsid[0] = '\0';
    DEST_IP       = AP_BCAST_IP; // Broadcast to all clients on 192.168.1.255

    udp.begin(LOCAL_OSC_PORT);
    webServer.begin();

    Serial.print("AP Mode ON! SSID: ");
    Serial.print(AP_SSID);
    Serial.print(" | IP: ");
    Serial.print(WiFi.localIP());
    Serial.print(" | OSC Broadcast: ");
    Serial.print(DEST_IP);
    Serial.print(":");
    Serial.println(DEST_PORT);
}

bool connectKnownNetwork() {
    apModeActive  = false;
    wifiConnected = false;
    activeSsid[0] = '\0';

    if (cfg.netCount == 0) {
        Serial.println("No known Wi-Fi networks stored in Flash. Press short+long (>3s) on D7 for AP Mode.");
        return false;
    }

    Serial.println("Scanning for known Wi-Fi networks...");
    WiFi.setTimeout(6000);
    int found = WiFi.scanNetworks();
    if (found <= 0) {
        Serial.println("No Wi-Fi networks found in scan.");
        return false;
    }

    // Match scanned networks against stored list
    for (uint8_t k = 0; k < cfg.netCount; k++) {
        bool seen = false;
        for (int s = 0; s < found; s++) {
            if (strcmp(WiFi.SSID(s), cfg.nets[k].ssid) == 0) {
                seen = true;
                break;
            }
        }
        if (!seen) continue;

        Serial.print("Connecting to known Wi-Fi: ");
        Serial.println(cfg.nets[k].ssid);

        // Apply optional static local IP if configured (non-zero)
        if (cfg.nets[k].localIp[0] != 0) {
            IPAddress lip(cfg.nets[k].localIp[0], cfg.nets[k].localIp[1], cfg.nets[k].localIp[2], cfg.nets[k].localIp[3]);
            IPAddress gw = (cfg.nets[k].gatewayIp[0] != 0)
                ? IPAddress(cfg.nets[k].gatewayIp[0], cfg.nets[k].gatewayIp[1], cfg.nets[k].gatewayIp[2], cfg.nets[k].gatewayIp[3])
                : IPAddress(lip[0], lip[1], lip[2], 1);
            WiFi.config(lip, gw, gw, SUBNET_MASK);
        }

        uint8_t st = WiFi.begin(cfg.nets[k].ssid, cfg.nets[k].pass);
        if (st == WL_CONNECTED) {
            WiFi.noLowPowerMode(); // Prevent radio sleep beacon-miss disconnects
            wifiConnected = true;
            wifiMissCount = 0;
            strncpy(activeSsid, cfg.nets[k].ssid, sizeof(activeSsid) - 1);
            activeSsid[sizeof(activeSsid) - 1] = '\0';
            if (cfg.nets[k].destIp[0] != 0) {
                DEST_IP = IPAddress(cfg.nets[k].destIp[0], cfg.nets[k].destIp[1], cfg.nets[k].destIp[2], cfg.nets[k].destIp[3]);
            } else {
                IPAddress ip = WiFi.localIP();
                DEST_IP = IPAddress(ip[0], ip[1], ip[2], 255);
            }
            udp.begin(LOCAL_OSC_PORT);
            webServer.begin();

            Serial.print("Connected! IP: ");
            Serial.print(WiFi.localIP());
            Serial.print(" | OSC Target: ");
            Serial.print(DEST_IP);
            Serial.print(":");
            Serial.println(DEST_PORT);
            return true;
        }
        WiFi.disconnect();
    }

    Serial.println("Failed to connect to any known network.");
    return false;
}

void startClientMode() {
    Serial.println("Switching to Client Mode (AP Mode OFF)...");
    resetWiFiModule();
    apModeActive = false;
    connectKnownNetwork();
}

void onWakeupIRQ() {
    // Interrupt callback required by LowPower.attachInterruptWakeup
}

void enterDeepSleep() {
    Serial.println("Entering Deep Sleep (press D7 to wake up)...");
    sendOSC("/status/sleep", 1);
    analogWrite(LED_BUILTIN, 0);
    delay(30);

    // Wait for switch release on D7 before arming falling-edge wakeup
    while (digitalRead(CMD_SWITCH_PIN) == LOW) {
        delay(10);
    }
    delay(60);

    webServer.end();
    udp.stop();
    WiFi.disconnect();
    SpiDrv::end(); // Put NINA-W102 into hardware reset while sleeping

    LowPower.attachInterruptWakeup(CMD_SWITCH_PIN, onWakeupIRQ, FALLING);
    LowPower.deepSleep();

    // --- WAKE UP HERE ---
    detachInterrupt(digitalPinToInterrupt(CMD_SWITCH_PIN));
    while (digitalRead(CMD_SWITCH_PIN) == LOW) {
        delay(10);
    }
    delay(50);

    swStableState   = HIGH;
    swLastRaw       = HIGH;
    swClickCount    = 0;
    swHoldHandled   = false;
    sleepRequested  = false;
    apTogglePending = false;

    if (apModeActive) startAPMode();
    else startClientMode();

    sendOSC("/status/sleep", 0);
}

// ---------------------------------------------------------------------------
// MOMENTARY COMMAND SWITCH (D7)
// ---------------------------------------------------------------------------
void updateCommandSwitch(uint32_t nowMs) {
    bool reading = digitalRead(CMD_SWITCH_PIN);

    if (reading != swLastRaw) {
        swDebounceAt = nowMs;
        swLastRaw = reading;
    }

    if ((nowMs - swDebounceAt) >= SW_DEBOUNCE_MS && reading != swStableState) {
        swStableState = reading;

        if (swStableState == LOW) {
            swPressedAt = nowMs;
            swHoldHandled = false;
            sendOSC("/switch/raw", 1);
        } else {
            sendOSC("/switch/raw", 0);

            if (!swHoldHandled) {
                swClickCount++;
                swLastReleaseAt = nowMs;
                if (swClickCount >= 3) {
                    calibrateAccelZero();
                    sendOSC("/switch/triple", 1);
                    sendOSC("/accel/calibrated", 1);
                    swClickCount = 0;
                }
            }
        }
    }

    // Check hold thresholds while button is held LOW
    if (swStableState == LOW && !swHoldHandled) {
        uint32_t heldMs = nowMs - swPressedAt;

        // Short + Long (> 3s) => toggle AP Mode ON / OFF
        if (swClickCount == 1 && heldMs >= SW_AP_HOLD_MS) {
            swHoldHandled   = true;
            swClickCount    = 0;
            apTogglePending = true;
            // Immediate visual acknowledgment on LED
            analogWrite(LED_BUILTIN, apModeActive ? 0 : 128);
        }
        // Direct Long (> 5s) => Deep Sleep
        else if (swClickCount == 0 && heldMs >= SW_SLEEP_HOLD_MS) {
            swHoldHandled  = true;
            swClickCount   = 0;
            sendOSC("/switch/long", 1);
            sleepRequested = true;
        }
    }

    // Validate short / double clicks after multi-click gap expires
    if (swStableState == HIGH && swClickCount > 0 && (nowMs - swLastReleaseAt) >= SW_MULTI_GAP_MS) {
        if (swClickCount == 1) sendOSC("/switch/short", 1);
        else if (swClickCount == 2) sendOSC("/switch/double", 1);
        swClickCount = 0;
    }
}

// ---------------------------------------------------------------------------
// EMBEDDED HTML MONITOR, WI-FI MANAGER & DOCUMENTATION
// ---------------------------------------------------------------------------
const char INDEX_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MKR 1010 OSC + Wi-Fi Manager</title>
<style>
body{font-family:system-ui,-apple-system,sans-serif;background:#111;color:#eee;margin:0;padding:16px;line-height:1.45}
h1{font-size:1.15rem;margin:0 0 12px}
h2{font-size:.98rem;margin:0 0 8px;color:#fff}
h3{font-size:.84rem;margin:12px 0 4px;color:#0a84ff;text-transform:uppercase;letter-spacing:.03em}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(310px,1fr));gap:12px}
.card{background:#1c1c1e;border:1px solid #2c2c2e;border-radius:10px;padding:14px}
.full{margin-top:12px}
label{display:flex;justify-content:space-between;font-size:.82rem;margin:7px 0 3px;color:#bbb}
input[type=range],input[type=text],input[type=password],select{width:100%;box-sizing:border-box}
input[type=text],input[type=password],select{background:#2c2c2e;color:#fff;border:1px solid #3a3a3c;border-radius:6px;padding:6px 8px;font-size:.84rem}
.bar-row{display:grid;grid-template-columns:46px 1fr 56px;align-items:center;gap:8px;font-size:.84rem;margin:6px 0}
.track{height:10px;background:#2c2c2e;border-radius:5px;overflow:hidden}
.fill{height:100%;width:0%}
.trigs{display:flex;flex-wrap:wrap;gap:8px;margin-top:8px}
.badge{padding:5px 10px;border-radius:6px;background:#2c2c2e;font-size:.78rem;font-weight:700;border:1px solid #3a3a3c}
.badge.hit{background:#ff453a;color:#fff;border-color:#ff453a}
.dirbadge{display:inline-block;min-width:74px;text-align:center;padding:4px 8px;border-radius:6px;background:#2c2c2e;font-size:.76rem;font-weight:700;border:1px solid #3a3a3c;font-variant-numeric:tabular-nums}
.dirbadge.up{background:#1b3a28;color:#30d158;border-color:#30d158}
.dirbadge.dn{background:#3a1d1c;color:#ff453a;border-color:#ff453a}
canvas{width:100%;height:140px;background:#0a0a0c;border-radius:6px;display:block;margin-top:8px}
button{background:#0a84ff;color:#fff;border:none;border-radius:6px;padding:7px 11px;cursor:pointer;margin:8px 6px 0 0;font-weight:600;font-size:.82rem}
table{width:100%;border-collapse:collapse;font-size:.8rem;margin-top:4px}
th,td{text-align:left;padding:5px 8px;border-bottom:1px solid #2c2c2e;vertical-align:top}
th{color:#999;font-weight:600}
code{background:#2c2c2e;padding:1px 5px;border-radius:4px;font-size:.78rem;color:#70d7ff}
.doc-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:14px}
.net-row{display:flex;justify-content:space-between;align-items:center;background:#2c2c2e;padding:6px 10px;border-radius:6px;margin-top:6px;font-size:.82rem}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:8px}
</style></head><body>
<h1>MKR WiFi 1010 — Accelerometer, OSC &amp; Wi-Fi Manager</h1>
<div id="netbanner" style="font-size:.82rem;color:#aaa;margin-bottom:10px">Loading device status...</div>

<div class="grid">
  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center">
      <strong>Dynamic Accelerometer (A0=X, A1=Y, A2=Z)</strong>
      <label style="margin:0;gap:6px"><input id="poll" type="checkbox" checked> Live</label>
    </div>
    <div class="trigs">
      <span id="tx" class="badge" style="border-color:#ff453a">TRIG X</span>
      <span id="ty" class="badge" style="border-color:#30d158">TRIG Y</span>
      <span id="tz" class="badge" style="border-color:#0a84ff">TRIG Z</span>
      <span id="tm" class="badge" style="border-color:#ffffff">TRIG MAG</span>
    </div>
    <div class="trigs" style="margin-top:6px">
      <span id="dx" class="dirbadge" style="color:#ff453a">DIR X •</span>
      <span id="dy" class="dirbadge" style="color:#30d158">DIR Y •</span>
      <span id="dz" class="dirbadge" style="color:#0a84ff">DIR Z •</span>
    </div>
    <div id="abars" style="margin-top:10px"></div>
    <canvas id="cv" width="420" height="140"></canvas>
    <div style="font-size:.75rem;color:#999;margin-top:5px">
      Colors: <span style="color:#ff453a">dynX (Red)</span> &bull;
      <span style="color:#30d158">dynY (Green)</span> &bull;
      <span style="color:#0a84ff">dynZ (Blue)</span> &bull;
      <span style="color:#ffffff">Mag (White)</span>
    </div>
  </div>

  <div class="card">
    <strong>Thresholds &amp; Gain Controls</strong>
    <label>Amplification Gain <span id="vgain">5.0</span></label>
    <input id="gain" type="range" min="1" max="30" step="0.5" value="5">
    <label>Threshold X <span id="vtx">0.60</span></label>
    <input id="rtx" type="range" min="0.02" max="1.0" step="0.01" value="0.60">
    <label>Threshold Y <span id="vty">0.60</span></label>
    <input id="rty" type="range" min="0.02" max="1.0" step="0.01" value="0.60">
    <label>Threshold Z <span id="vtz">0.60</span></label>
    <input id="rtz" type="range" min="0.02" max="1.0" step="0.01" value="0.60">
    <label>3D Magnitude Threshold <span id="vtm">0.60</span></label>
    <input id="rtm" type="range" min="0.02" max="1.5" step="0.01" value="0.60">
    <label>Smoothing k <span id="vs">0.18</span></label>
    <input id="smooth" type="range" min="0.01" max="1.0" step="0.01" value="0.18">
    <label>Active Analog Pins (3 = A0..A2) <span id="vac">3</span></label>
    <input id="acnt" type="range" min="1" max="7" step="1" value="3">
    <div>
      <button onclick="sendCfg(false)">Apply</button>
      <button onclick="sendCfg(true)" style="background:#5e5ce6">Save to Flash</button>
      <button onclick="fetch('/api/cal')" style="background:#30d158;color:#000">Calibrate Zero</button>
      <button onclick="if(confirm('Enter Deep Sleep? Press D7 to wake up.'))fetch('/api/sleep')" style="background:#ff453a">Deep Sleep</button>
    </div>
    <div id="status" style="font-size:.76rem;color:#8e8e93;margin-top:8px">Ready.</div>
  </div>

  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center">
      <strong>Known Wi-Fi Networks (Flash)</strong>
      <button onclick="scanWifi()" style="margin:0;padding:4px 8px;font-size:.75rem">Scan 2.4GHz</button>
    </div>
    <div id="netlist" style="margin-top:6px"></div>
    <div id="scanbox" style="margin-top:8px;display:none">
      <label>Nearby Networks</label>
      <select id="scansel" onchange="if(this.value)wssid.value=this.value"></select>
    </div>
    <label>Wi-Fi SSID</label>
    <input id="wssid" type="text" placeholder="e.g. airportexpressbruckert">
    <label>Password</label>
    <input id="wpass" type="password" placeholder="Wi-Fi passphrase">
    <div class="row2">
      <div>
        <label>OSC Target IP (Mac)</label>
        <input id="wdip" type="text" value="192.168.1.6">
      </div>
      <div>
        <label>MKR IP (0.0.0.0=DHCP)</label>
        <input id="wlip" type="text" value="192.168.1.2">
      </div>
    </div>
    <label>Gateway IP (0.0.0.0=auto .1)</label>
    <input id="wgw" type="text" value="192.168.1.1">
    <div>
      <button onclick="addWifi()" style="background:#30d158;color:#000">Add / Update Network</button>
      <button onclick="connectClient()" style="background:#ff9f0a;color:#000">Connect Client Mode</button>
    </div>
  </div>
</div>

<div class="card full">
  <h2>Full Function &amp; OSC Reference</h2>
  <div class="doc-grid">
    <div>
      <h3>Momentary Switch (Pin D7) &amp; LED (Pin D6)</h3>
      <table>
        <tr><th>Gesture</th><th>Action / OSC Output</th></tr>
        <tr><td>Press / Release</td><td>Immediate <code>/switch/raw 1</code> or <code>0</code>.</td></tr>
        <tr><td>1 Short Click</td><td>Emits <code>/switch/short 1</code> (after 450 ms window).</td></tr>
        <tr><td>2 Quick Clicks</td><td>Emits <code>/switch/double 1</code>.</td></tr>
        <tr><td>3 Quick Clicks</td><td>Runs <strong>Zero Calibration</strong> + emits <code>/switch/triple 1</code>.</td></tr>
        <tr><td><strong>Short + Hold &gt; 3s</strong></td><td>Toggles <strong>AP Mode ON / OFF</strong> (LED slow pulse = AP ON).</td></tr>
        <tr><td><strong>Direct Hold &gt; 5s</strong></td><td>Enters <strong>Deep Sleep</strong> (LED off; press D7 to wake).</td></tr>
        <tr><th>LED State (D6)</th><th>Meaning</th></tr>
        <tr><td>Solid Full ON</td><td>Client Mode connected to a known Wi-Fi network.</td></tr>
        <tr><td>Fast Blink</td><td>Client Mode disconnected / no known Wi-Fi in range.</td></tr>
        <tr><td>Slow Pulse</td><td>AP Mode active (<code>Arduino AP mode</code> / <code>192.168.1.128</code>).</td></tr>
      </table>
      <h3>AP Mode &amp; Flash Wi-Fi Storage</h3>
      <table>
        <tr><td>AP Credentials</td><td>SSID: <code>Arduino AP mode</code> &bull; Pass: <code>12345678</code> &bull; IP: <code>192.168.1.128</code></td></tr>
        <tr><td>AP OSC Streaming</td><td>Broadcasts on <code>192.168.1.255:3033</code> to all connected devices.</td></tr>
        <tr><td>Flash Persistence</td><td>Stores up to 5 networks + Gain/Thresholds across reboots &amp; Deep Sleep. Note: USB re-upload erases SAMD21 Flash.</td></tr>
      </table>
    </div>
    <div>
      <h3>Outgoing OSC Messages (UDP Port 3033)</h3>
      <table>
        <tr><th>OSC Address</th><th>Type &amp; Meaning</th></tr>
        <tr><td><code>/accel/x</code>, <code>/y</code>, <code>/z</code></td><td>float <code>-1..1</code> — calibrated tilt zoomed by Gain.</td></tr>
        <tr><td><code>/accel/dyn/x</code>, <code>/y</code>, <code>/z</code></td><td>float <code>-1.5..1.5</code> — high-pass dynamic motion.</td></tr>
        <tr><td><code>/accel/mag</code></td><td>float <code>&ge;0</code> — 3D dynamic magnitude.</td></tr>
        <tr><td><code>/accel/trig/x|y|z|mag</code></td><td>int <code>1</code> — pure bang pulse when |dyn| &ge; threshold (default 0.60, re-arms at 70%).</td></tr>
        <tr><td><code>/accel/dir/x|y|z</code></td><td>int <code>+1</code> (ascending), <code>-1</code> (descending), <code>0</code> (steady).</td></tr>
        <tr><td><code>/analog/A0..A6</code></td><td>int <code>0..4095</code> — raw 12-bit ADC value.</td></tr>
        <tr><td><code>/analog-one/A0..A6</code></td><td>float <code>0..1</code> — clipped, smoothed &amp; gamma-shaped.</td></tr>
        <tr><td><code>/analog-hundred/A0..A6</code></td><td>float <code>-100..100</code> — scaled output.</td></tr>
        <tr><td><code>/digital/D2..D14</code></td><td>int <code>0/1</code> — digital pins (excl. D6 LED &amp; D7 switch).</td></tr>
        <tr><td><code>/status/sleep</code>, <code>/timestamp</code></td><td>int <code>1/0</code> sleep; int ms uptime (1 Hz).</td></tr>
      </table>
      <h3>Incoming OSC (UDP Port 3034) &amp; HTTP API</h3>
      <table>
        <tr><td><code>/accel/gain</code>, <code>/thresh</code>, <code>/threshmag</code></td><td>Update Gain, axis thresholds, or magnitude threshold.</td></tr>
        <tr><td><code>/accel/calibrate</code>, <code>/system/save</code></td><td>Zero-calibrate A0..A2; save settings to Flash.</td></tr>
        <tr><td><code>/config/smooth</code>, <code>/gamma</code>, <code>/rate</code>, <code>/pause</code></td><td>Update smoothing, gamma, Hz, or pause stream.</td></tr>
        <tr><td><code>/system/sleep 1</code></td><td>Enter Deep Sleep immediately.</td></tr>
        <tr><td><code>GET /api/wifi</code>, <code>/api/wifi/add</code>, <code>/del</code>, <code>/scan</code></td><td>Manage stored Wi-Fi networks &amp; scan 2.4 GHz.</td></tr>
      </table>
    </div>
  </div>
</div>

<script>
const abars=document.getElementById('abars'),cv=document.getElementById('cv'),ctx=cv.getContext('2d');
const names=['dynX','dynY','dynZ','Mag'];
const cols=['#ff453a','#30d158','#0a84ff','#ffffff'];
names.forEach((n,i)=>{
  abars.innerHTML+=`<div class="bar-row"><span style="color:${cols[i]};font-weight:700">${n}</span><div class="track"><div id="af${i}" class="fill" style="background:${cols[i]}"></div></div><span id="at${i}" style="color:${cols[i]}">0.00</span></div>`;
});
const hx=[],hy=[],hz=[],hm=[];
function draw(){
  ctx.clearRect(0,0,cv.width,cv.height);
  const plot=(arr,col)=>{ctx.strokeStyle=col;ctx.lineWidth=1.8;ctx.beginPath();
    arr.forEach((v,i)=>{const x=i*(cv.width/100),y=cv.height-(Math.min(1,Math.abs(v))*cv.height);i?ctx.lineTo(x,y):ctx.moveTo(x,y);});ctx.stroke();};
  plot(hx,cols[0]);plot(hy,cols[1]);plot(hz,cols[2]);plot(hm,cols[3]);
}
async function tick(){
  if(!document.getElementById('poll').checked)return;
  try{
    const d=await (await fetch('/api/data')).json();
    const vals=[d.dyn[0],d.dyn[1],d.dyn[2],d.mag];
    vals.forEach((v,i)=>{
      const pct=Math.min(100,Math.abs(v)*100);
      document.getElementById('af'+i).style.width=pct+'%';
      document.getElementById('at'+i).textContent=v.toFixed(2);
    });
    if(d.dir){
      ['dx','dy','dz'].forEach((id,i)=>{
        const el=document.getElementById(id),v=d.dir[i],lbl=['DIR X','DIR Y','DIR Z'][i];
        el.textContent=lbl+' '+(v>0?'▲':(v<0?'▼':'•'));
        el.className='dirbadge'+(v>0?' up':(v<0?' dn':''));
      });
    }
    ['tx','ty','tz','tm'].forEach((id,i)=>{
      document.getElementById(id).className='badge'+(d.trig[i]?' hit':'');
    });
    hx.push(d.dyn[0]);hy.push(d.dyn[1]);hz.push(d.dyn[2]);hm.push(d.mag);
    if(hx.length>100){hx.shift();hy.shift();hz.shift();hm.shift();}
    draw();
  }catch(e){}
}
const pairs=[['gain','vgain'],['rtx','vtx'],['rty','vty'],['rtz','vtz'],['rtm','vtm'],['smooth','vs'],['acnt','vac']];
pairs.forEach(([i,s])=>document.getElementById(i).oninput=()=>{document.getElementById(s).textContent=document.getElementById(i).value;});
async function loadState(){
  try{
    const c=await (await fetch('/api/state')).json();
    gain.value=c.gn;rtx.value=c.tx;rty.value=c.ty;rtz.value=c.tz;rtm.value=c.tm;smooth.value=c.s;acnt.value=c.ac;
    pairs.forEach(([i,s])=>document.getElementById(s).textContent=document.getElementById(i).value);
    document.getElementById('netbanner').textContent=`Mode: ${c.mode} | MKR IP: ${c.ip} | OSC Target: ${c.dest}:${c.port}`;
  }catch(e){}
}
async function loadWifi(){
  try{
    const list=await (await fetch('/api/wifi')).json();
    const el=document.getElementById('netlist');
    if(!list.length){el.innerHTML='<div style="font-size:.8rem;color:#888">No networks saved yet.</div>';return;}
    el.innerHTML=list.map((n,i)=>`<div class="net-row"><div><strong>${n.s}</strong><br><span style="color:#aaa;font-size:.74rem">MKR:${n.l} &rarr; OSC:${n.d}</span></div><button onclick="delWifi(${i})" style="margin:0;background:#ff453a;padding:4px 8px">Delete</button></div>`).join('');
  }catch(e){}
}
async function addWifi(){
  if(!wssid.value)return alert('Enter SSID');
  const q=`s=${encodeURIComponent(wssid.value)}&p=${encodeURIComponent(wpass.value)}&d=${encodeURIComponent(wdip.value)}&l=${encodeURIComponent(wlip.value)}&g=${encodeURIComponent(wgw.value)}`;
  await fetch('/api/wifi/add?'+q);
  wpass.value='';
  await loadWifi();
  document.getElementById('status').textContent='Network saved to Flash.';
}
async function delWifi(idx){
  await fetch('/api/wifi/del?i='+idx);
  await loadWifi();
}
async function scanWifi(){
  const sel=document.getElementById('scansel');
  sel.innerHTML='<option>Scanning...</option>';
  document.getElementById('scanbox').style.display='block';
  const arr=await (await fetch('/api/scan')).json();
  sel.innerHTML='<option value="">-- Select SSID --</option>'+arr.map(n=>`<option value="${n.s}">${n.s} (${n.r} dBm)</option>`).join('');
}
async function connectClient(){
  if(confirm('Switch to Client Mode and connect to known Wi-Fi?'))await fetch('/api/client');
}
async function sendCfg(save){
  const q=`gn=${gain.value}&tx=${rtx.value}&ty=${rty.value}&tz=${rtz.value}&tm=${rtm.value}&s=${smooth.value}&ac=${acnt.value}&sv=${save?1:0}`;
  await fetch('/api/cfg?'+q);
  document.getElementById('status').textContent=save?'Settings applied and saved to Flash.':'Settings applied (RAM).';
}
loadState();
loadWifi();
setInterval(tick,100);
</script></body></html>)rawliteral";

// ---------------------------------------------------------------------------
// HTTP HELPERS & API HANDLERS
// ---------------------------------------------------------------------------
void urlDecode(const char* src, char* dst, size_t dstSize) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dstSize; i++) {
        if (src[i] == '+') {
            dst[j++] = ' ';
        } else if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            char hex[3] = {src[i + 1], src[i + 2], 0};
            dst[j++] = (char)strtol(hex, nullptr, 16);
            i += 2;
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

bool getQueryParam(const char* query, const char* key, char* out, size_t outSize) {
    char pat[16];
    snprintf(pat, sizeof(pat), "%s=", key);
    const char* p = query;
    while ((p = strstr(p, pat)) != nullptr) {
        if (p == query || *(p - 1) == '&') {
            p += strlen(pat);
            const char* end = strchr(p, '&');
            size_t rawLen = end ? (size_t)(end - p) : strlen(p);
            char tmp[96];
            if (rawLen >= sizeof(tmp)) rawLen = sizeof(tmp) - 1;
            memcpy(tmp, p, rawLen);
            tmp[rawLen] = '\0';
            urlDecode(tmp, out, outSize);
            return true;
        }
        p += strlen(pat);
    }
    return false;
}

float parseQueryFloat(const char* query, const char* key, float fallback) {
    char buf[24];
    if (!getQueryParam(query, key, buf, sizeof(buf))) return fallback;
    return (float)atof(buf);
}

void parseIpBytes(const char* str, uint8_t out[4]) {
    IPAddress ip;
    if (ip.fromString(str)) {
        for (int i = 0; i < 4; i++) out[i] = ip[i];
    } else {
        memset(out, 0, 4);
    }
}

void fmtFixed(char* out, size_t outSz, float v, int scale) {
    int32_t iv = (int32_t)roundf(v * (float)scale);
    bool neg = iv < 0;
    if (neg) iv = -iv;
    if (scale == 1000) snprintf(out, outSz, "%s%ld.%03ld", neg ? "-" : "", (long)(iv / 1000), (long)(iv % 1000));
    else snprintf(out, outSz, "%s%ld.%02ld", neg ? "-" : "", (long)(iv / 100), (long)(iv % 100));
}

void sendJsonHeaders(WiFiClient& client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
}

void sendJsonData(WiFiClient& client) {
    char dx[12], dy[12], dz[12], mg[12], body[160], resp[260];
    uint32_t nowMs = millis();
    fmtFixed(dx, sizeof(dx), accelDyn[0], 1000);
    fmtFixed(dy, sizeof(dy), accelDyn[1], 1000);
    fmtFixed(dz, sizeof(dz), accelDyn[2], 1000);
    fmtFixed(mg, sizeof(mg), accelMag, 1000);
    int bLen = snprintf(body, sizeof(body),
        "{\"dyn\":[%s,%s,%s],\"mag\":%s,\"trig\":[%d,%d,%d,%d],\"dir\":[%d,%d,%d]}",
        dx, dy, dz, mg,
        (lastTrigMs[0] && (nowMs - lastTrigMs[0]) < 220) ? 1 : 0,
        (lastTrigMs[1] && (nowMs - lastTrigMs[1]) < 220) ? 1 : 0,
        (lastTrigMs[2] && (nowMs - lastTrigMs[2]) < 220) ? 1 : 0,
        (lastTrigMs[3] && (nowMs - lastTrigMs[3]) < 220) ? 1 : 0,
        (int)accelDir[0], (int)accelDir[1], (int)accelDir[2]);
    int rLen = snprintf(resp, sizeof(resp),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
        bLen, body);
    client.write((const uint8_t*)resp, (size_t)rLen);
}

void sendJsonState(WiFiClient& client) {
    char gn[10], tx[10], ty[10], tz[10], tm[10], sm[10], body[240], resp[340];
    fmtFixed(gn, sizeof(gn), cfg.accelGain, 100);
    fmtFixed(tx, sizeof(tx), cfg.accelThresh[0], 100);
    fmtFixed(ty, sizeof(ty), cfg.accelThresh[1], 100);
    fmtFixed(tz, sizeof(tz), cfg.accelThresh[2], 100);
    fmtFixed(tm, sizeof(tm), cfg.accelThreshMag, 100);
    fmtFixed(sm, sizeof(sm), cfg.smoothK, 100);
    IPAddress lip = WiFi.localIP();
    int bLen = snprintf(body, sizeof(body),
        "{\"gn\":%s,\"tx\":%s,\"ty\":%s,\"tz\":%s,\"tm\":%s,\"s\":%s,\"ac\":%d,"
        "\"mode\":\"%s\",\"ip\":\"%d.%d.%d.%d\",\"dest\":\"%d.%d.%d.%d\",\"port\":%d}",
        gn, tx, ty, tz, tm, sm, (int)cfg.activeAnalogCount,
        apModeActive ? "AP Mode" : (wifiConnected ? activeSsid : "Disconnected"),
        lip[0], lip[1], lip[2], lip[3],
        DEST_IP[0], DEST_IP[1], DEST_IP[2], DEST_IP[3], DEST_PORT);
    int rLen = snprintf(resp, sizeof(resp),
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
        bLen, body);
    client.write((const uint8_t*)resp, (size_t)rLen);
}

void sendJsonWifiList(WiFiClient& client) {
    sendJsonHeaders(client);
    client.print('[');
    for (uint8_t i = 0; i < cfg.netCount; i++) {
        if (i > 0) client.print(',');
        client.print("{\"s\":\"");
        client.print(cfg.nets[i].ssid);
        client.print("\",\"l\":\"");
        client.print(IPAddress(cfg.nets[i].localIp[0], cfg.nets[i].localIp[1], cfg.nets[i].localIp[2], cfg.nets[i].localIp[3]));
        client.print("\",\"d\":\"");
        client.print(IPAddress(cfg.nets[i].destIp[0], cfg.nets[i].destIp[1], cfg.nets[i].destIp[2], cfg.nets[i].destIp[3]));
        client.print("\"}");
    }
    client.println(']');
}

void sendJsonScan(WiFiClient& client) {
    int n = WiFi.scanNetworks();
    sendJsonHeaders(client);
    client.print('[');
    bool first = true;
    for (int i = 0; i < n && i < 12; i++) {
        const char* s = WiFi.SSID(i);
        if (!s || !s[0]) continue;
        if (!first) client.print(',');
        first = false;
        client.print("{\"s\":\"");
        client.print(s);
        client.print("\",\"r\":");
        client.print(WiFi.RSSI(i));
        client.print('}');
    }
    client.println(']');
}

void handleWifiAdd(const char* query) {
    char ssid[33] = {0}, pass[65] = {0}, dip[20] = {0}, lip[20] = {0}, gw[20] = {0};
    if (!getQueryParam(query, "s", ssid, sizeof(ssid)) || !ssid[0]) return;
    getQueryParam(query, "p", pass, sizeof(pass));
    getQueryParam(query, "d", dip, sizeof(dip));
    getQueryParam(query, "l", lip, sizeof(lip));
    getQueryParam(query, "g", gw, sizeof(gw));

    int idx = -1;
    for (uint8_t i = 0; i < cfg.netCount; i++) {
        if (strcmp(cfg.nets[i].ssid, ssid) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (cfg.netCount < MAX_WIFI_NETS) {
            idx = cfg.netCount++;
        } else {
            idx = MAX_WIFI_NETS - 1; // Overwrite oldest last slot
        }
    }

    strncpy(cfg.nets[idx].ssid, ssid, sizeof(cfg.nets[idx].ssid) - 1);
    cfg.nets[idx].ssid[sizeof(cfg.nets[idx].ssid) - 1] = '\0';
    if (pass[0]) {
        strncpy(cfg.nets[idx].pass, pass, sizeof(cfg.nets[idx].pass) - 1);
        cfg.nets[idx].pass[sizeof(cfg.nets[idx].pass) - 1] = '\0';
    }
    parseIpBytes(dip, cfg.nets[idx].destIp);
    parseIpBytes(lip, cfg.nets[idx].localIp);
    parseIpBytes(gw, cfg.nets[idx].gatewayIp);
    saveConfig();
}

void handleWifiDel(const char* query) {
    int idx = (int)parseQueryFloat(query, "i", -1);
    if (idx < 0 || idx >= cfg.netCount) return;
    for (uint8_t i = idx; i + 1 < cfg.netCount; i++) {
        cfg.nets[i] = cfg.nets[i + 1];
    }
    cfg.netCount--;
    memset(&cfg.nets[cfg.netCount], 0, sizeof(WifiEntry));
    saveConfig();
}

void handleWebServer() {
    WiFiClient client = webServer.available();
    if (!client) return;

    char reqLine[320] = {0};
    uint16_t idx = 0;
    bool firstLineDone = false;
    uint8_t blankRun = 0; // Tracks \r\n\r\n (or \n\n) end of HTTP headers
    uint32_t tStart = millis();

    while (client.connected() && (millis() - tStart) < 250) {
        if (client.available()) {
            char c = client.read();
            if (!firstLineDone) {
                if (c == '\r' || c == '\n') {
                    firstLineDone = true;
                } else if (idx < sizeof(reqLine) - 1) {
                    reqLine[idx++] = c;
                }
            }
            if (c == '\n') {
                blankRun++;
                if (blankRun >= 2) break;
            } else if (c != '\r') {
                blankRun = 0;
            }
        }
    }

    // Strip trailing " HTTP/1.x"
    char* httpVer = strstr(reqLine, " HTTP/");
    if (httpVer) *httpVer = '\0';

    if (strncmp(reqLine, "GET /api/data", 13) == 0) {
        sendJsonData(client);
    } else if (strncmp(reqLine, "GET /api/state", 14) == 0) {
        sendJsonState(client);
    } else if (strncmp(reqLine, "GET /api/wifi/add?", 18) == 0) {
        handleWifiAdd(reqLine + 18);
        client.println("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
    } else if (strncmp(reqLine, "GET /api/wifi/del?", 18) == 0) {
        handleWifiDel(reqLine + 18);
        client.println("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
    } else if (strncmp(reqLine, "GET /api/wifi", 13) == 0) {
        sendJsonWifiList(client);
    } else if (strncmp(reqLine, "GET /api/scan", 13) == 0) {
        sendJsonScan(client);
    } else if (strncmp(reqLine, "GET /api/client", 15) == 0) {
        client.println("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
        reconnectPending = true;
    } else if (strncmp(reqLine, "GET /api/cfg?", 13) == 0) {
        const char* q = reqLine + 13;
        cfg.accelGain      = constrain(parseQueryFloat(q, "gn", cfg.accelGain), 1.0f, 30.0f);
        cfg.accelThresh[0] = constrain(parseQueryFloat(q, "tx", cfg.accelThresh[0]), 0.01f, 2.0f);
        cfg.accelThresh[1] = constrain(parseQueryFloat(q, "ty", cfg.accelThresh[1]), 0.01f, 2.0f);
        cfg.accelThresh[2] = constrain(parseQueryFloat(q, "tz", cfg.accelThresh[2]), 0.01f, 2.0f);
        cfg.accelThreshMag    = constrain(parseQueryFloat(q, "tm", cfg.accelThreshMag), 0.01f, 3.0f);
        cfg.smoothK           = constrain(parseQueryFloat(q, "s", cfg.smoothK), 0.01f, 1.0f);
        cfg.activeAnalogCount = constrain((int)parseQueryFloat(q, "ac", cfg.activeAnalogCount), 1, N_A);
        if ((int)parseQueryFloat(q, "sv", 0) == 1) saveConfig();
        client.println("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
    } else if (strncmp(reqLine, "GET /api/cal", 12) == 0) {
        calibrateAccelZero();
        sendOSC("/accel/calibrated", 1);
        client.println("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
    } else if (strncmp(reqLine, "GET /api/sleep", 14) == 0) {
        client.println("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");
        sleepRequested = true;
    } else {
        const size_t totalLen = strlen(INDEX_HTML);
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html; charset=utf-8");
        client.print("Content-Length: ");
        client.println(totalLen);
        client.println("Connection: close");
        client.println();
        const size_t chunkSize = 256;
        for (size_t off = 0; off < totalLen; off += chunkSize) {
            size_t len = (totalLen - off) > chunkSize ? chunkSize : (totalLen - off);
            client.write((const uint8_t*)(INDEX_HTML + off), len);
            delay(2);
        }
    }

    delay(15);
    client.stop();
}

// ---------------------------------------------------------------------------
// 3-AXIS ACCELEROMETER PROCESSING (A0=X, A1=Y, A2=Z) & THRESHOLDS
// ---------------------------------------------------------------------------
void updateAccelAxis(uint8_t a, int raw, uint32_t nowMs) {
    static const char* zoomAddrs[3] = {"/accel/x", "/accel/y", "/accel/z"};
    static const char* dynAddrs[3]  = {"/accel/dyn/x", "/accel/dyn/y", "/accel/dyn/z"};
    static const char* trigAddrs[3] = {"/accel/trig/x", "/accel/trig/y", "/accel/trig/z"};
    static const char* dirAddrs[3]  = {"/accel/dir/x", "/accel/dir/y", "/accel/dir/z"};

    if (!accelBaseInit) {
        accelSlow[a] = (float)raw;
    } else {
        accelSlow[a] += ((float)raw - accelSlow[a]) * ACCEL_HP_ALPHA;
    }

    float z = ((float)raw - accelZero[a]) / 2048.0f * cfg.accelGain;
    accelZoom[a] = constrain(z, -1.0f, 1.0f);

    if (fabsf(accelZoom[a] - lastZoom[a]) > 0.003f) {
        sendOSC(zoomAddrs[a], accelZoom[a]);
        lastZoom[a] = accelZoom[a];
    }

    float d = ((float)raw - accelSlow[a]) / 2048.0f * cfg.accelGain;
    accelDyn[a] = constrain(d, -1.5f, 1.5f);

    if (fabsf(accelDyn[a] - lastDyn[a]) > 0.003f) {
        sendOSC(dynAddrs[a], accelDyn[a]);
        lastDyn[a] = accelDyn[a];
    }

    // Ascending (+1) / descending (-1) / steady (0) slope detection
    float delta = accelDyn[a] - prevDynVal[a];
    prevDynVal[a] = accelDyn[a];
    dynSlope[a] += (delta - dynSlope[a]) * 0.35f;
    int8_t newDir = 0;
    if (dynSlope[a] > DIR_DEADBAND) newDir = 1;
    else if (dynSlope[a] < -DIR_DEADBAND) newDir = -1;
    if (newDir != accelDir[a]) {
        accelDir[a] = newDir;
        sendOSC(dirAddrs[a], (int)newDir);
    }

    // Pure bang trigger when |dyn| crosses threshold (re-arms below 70% hysteresis)
    float absD = fabsf(accelDyn[a]);
    if (trigArmed[a] && absD >= cfg.accelThresh[a] && (nowMs - lastTrigMs[a]) >= TRIG_COOLDOWN_MS) {
        trigArmed[a] = false;
        lastTrigMs[a] = nowMs;
        sendOSC(trigAddrs[a], 1);
    } else if (!trigArmed[a] && absD < (cfg.accelThresh[a] * HYST_RATIO)) {
        trigArmed[a] = true;
    }
}

// ---------------------------------------------------------------------------
// SETUP & MAIN LOOP
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    uint32_t waitSer = millis();
    while (!Serial && (millis() - waitSer) < 1500) {}

    pinMode(LED_BUILTIN, OUTPUT);
    analogWrite(LED_BUILTIN, 0);
    pinMode(CMD_SWITCH_PIN, INPUT_PULLUP);
    for (uint8_t i = 0; i < N_D; i++) {
        pinMode(D_PINS[i], INPUT_PULLUP);
        lastDig[i] = digitalRead(D_PINS[i]);
    }

    analogReadResolution(12);
    loadConfig();
    calibrateAccelZero();

    startClientMode();
}

void loop() {
    uint32_t nowMs = millis();
    uint32_t nowUs = micros();

    if (sleepRequested) {
        enterDeepSleep();
        return;
    }

    if (apTogglePending) {
        apTogglePending = false;
        if (apModeActive) startClientMode();
        else startAPMode();
        return;
    }

    if (reconnectPending) {
        reconnectPending = false;
        startClientMode();
        return;
    }

    // Monitor Client Mode Wi-Fi health (2-strike check every 500ms to ignore transient SPI glitches)
    if (!apModeActive) {
        if (wifiConnected && (nowMs - lastWifiCheckMs) >= 500) {
            lastWifiCheckMs = nowMs;
            if (WiFi.status() != WL_CONNECTED) {
                wifiMissCount++;
                if (wifiMissCount >= 2) {
                    wifiConnected = false;
                    Serial.print("Wi-Fi connection lost. Reason code: ");
                    Serial.println(WiFi.reasonCode());
                }
            } else {
                wifiMissCount = 0;
            }
        }
        if (!wifiConnected && (nowMs - lastWifiCheckMs) >= WIFI_RESCAN_MS) {
            lastWifiCheckMs = nowMs;
            connectKnownNetwork();
        }
    }

    updateLedStatus(nowMs);
    handleIncomingOSC();
    updateCommandSwitch(nowMs);

    if (networkReady() && (nowMs - lastWebPollMs) >= WEB_POLL_MS) {
        lastWebPollMs = nowMs;
        handleWebServer();
    }

    uint16_t effectiveHz = cfg.streamHz > 0 ? cfg.streamHz : 125;
    if (apModeActive && effectiveHz > 50) effectiveHz = 50; // Keep AP broadcast from starving HTTP
    uint32_t intervalUs = 1000000UL / effectiveHz;
    if (networkReady() && !streamingPaused && (nowUs - lastSampleUs) >= intervalUs) {
        lastSampleUs = nowUs;

        uint8_t nActive = cfg.activeAnalogCount;
        if (nActive < 1 || nActive > N_A) nActive = 3;
        for (uint8_t i = 0; i < nActive; i++) {
            analogRead(A_PINS[i]);
            int raw = analogRead(A_PINS[i]);

            if (abs(raw - (int)lastRaw[i]) >= RAW_EPS) {
                char addr[24];
                snprintf(addr, sizeof(addr), "/analog/A%d", i);
                sendOSC(addr, raw);
                lastRaw[i] = raw;
            }

            float vNorm  = clipAndNormalize((float)raw);
            float s      = applySmoothing(smoothVal[i], vNorm);
            smoothVal[i] = s;
            float vGamma = applyGamma(s, cfg.gamma);

            if (fabsf(vGamma - lastOne[i]) > EPS_ONE) {
                char addr[24];
                snprintf(addr, sizeof(addr), "/analog-one/A%d", i);
                sendOSC(addr, vGamma);
                lastOne[i] = vGamma;
            }

            float vHund = (vGamma * 200.0f) - 100.0f;
            if (fabsf(vHund - lastHundred[i]) > EPS_HUNDRED) {
                char addr[28];
                snprintf(addr, sizeof(addr), "/analog-hundred/A%d", i);
                sendOSC(addr, vHund);
                lastHundred[i] = vHund;
            }

            if (i < 3) updateAccelAxis(i, raw, nowMs);
        }
        accelBaseInit = true;

        accelMag = sqrtf(accelDyn[0]*accelDyn[0] + accelDyn[1]*accelDyn[1] + accelDyn[2]*accelDyn[2]);
        if (fabsf(accelMag - lastMagSent) > 0.005f) {
            sendOSC("/accel/mag", accelMag);
            lastMagSent = accelMag;
        }
        if (trigArmed[3] && accelMag >= cfg.accelThreshMag && (nowMs - lastTrigMs[3]) >= TRIG_COOLDOWN_MS) {
            trigArmed[3] = false;
            lastTrigMs[3] = nowMs;
            sendOSC("/accel/trig/mag", 1);
        } else if (!trigArmed[3] && accelMag < (cfg.accelThreshMag * HYST_RATIO)) {
            trigArmed[3] = true;
        }

        for (uint8_t i = 0; i < N_D; i++) {
            uint8_t v = digitalRead(D_PINS[i]);
            if (v != lastDig[i]) {
                char addr[24];
                snprintf(addr, sizeof(addr), "/digital/D%d", D_PINS[i]);
                sendOSC(addr, (int)v);
                lastDig[i] = v;
            }
        }
    }

    if (networkReady() && (nowMs - lastTimestampMs) >= TIMESTAMP_MS) {
        lastTimestampMs = nowMs;
        sendOSC("/timestamp", (int)nowMs);
    }
}
