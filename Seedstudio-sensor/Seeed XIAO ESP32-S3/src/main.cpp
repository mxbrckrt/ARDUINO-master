/**
 * =================================================================================
 * PROJET : IMU GANT OSC PRO - VERSION 12.1 (STABLE & ERGONOMIQUE)
 * MATÉRIEL : Seeed Studio XIAO ESP32S3 + Adafruit BNO055 + Batterie LiPo 3.7V
 * AUTEUR : Max Bruckert | ARCHIVE FINALE
 * =================================================================================
 * 
 * 1. COMMANDES PHYSIQUES (BOUTON SUR D1 / GPIO 2) :
 *    - 1 CLIC COURT   : Définit le "Zéro" relatif (Tare) pour la visée.
 *    - 3 CLICS COURTS : Bascule Mode AP (Gant-WiFi) <-> Mode Station (Box WiFi).
 *    - MAINTIEN > 5s  : Mise en veille profonde (Deep Sleep).
 *    - SORTIE VEILLE  : Un appui simple réveille l'appareil.
 * 
 * 2. CODES LED (ORANGE - GPIO 21) :
 *    - MODE AP (Autonome) : 2 flashs courts + pause 1s.
 *    - MODE STATION (Box) : Clignotement lent régulier (0.5s On / 0.5s Off).
 *    - CONNECTÉ (PC trouvé): LED Fixe (Allumée) -> Flux OSC actif.
 * 
 * 3. OSC (Host: imu-gant.local | IN: 9000 | OUT: 7000 par défaut) :
 *    - /euler, /rel/euler, /quat, /rel/quat, /accel, /battery, /calib
 * =================================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h> 
#include <WebServer.h>
#include <Preferences.h> 
#include <OSCBundle.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

// =================================================================================
// CONFIGURATION PAR DÉFAUT (VALEURS D'USINE)
// =================================================================================
const char* DEFAULT_SSID = "bruckertbox";         
const char* DEFAULT_PASS = "dancelascive1987";    
const char* DEFAULT_MDNS = "imu-gant";            
const int   DEFAULT_PORT_OUT = 7000;              

// =================================================================================
// BROCHES ET OBJETS
// =================================================================================
const int PIN_BUTTON      = 2;   // D1 (GPIO 2)
const int PIN_VBAT_READ   = 1;   // GPIO 1
const int PIN_VBAT_ENABLE = 14;  // GPIO 14
const int LED_USER        = 21;  

WiFiUDP Udp;
WebServer server(80);
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
Preferences prefs;

struct Config {
    char ssid[32]; char pass[32]; char mdns[32]; char targetIp[16];
    int portOut; bool isAP;
} cfg;

// Variables Système
IPAddress currentDestIp;
bool hasRecipient = false;
unsigned long lastOSCTime = 0;
unsigned long btnPressStart = 0;
unsigned long lastClickTime = 0;
int clickCount = 0;
bool isPressed = false;
imu::Quaternion quatOffset = imu::Quaternion(1, 0, 0, 0);

// =================================================================================
// FONCTIONS ÉNERGIE ET SYSTÈME
// =================================================================================

void goToSleep() {
    Serial.println(">>> MISE EN VEILLE (DEEP SLEEP)");
    digitalWrite(LED_USER, HIGH); // Eteint (logique inversée)
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_BUTTON, ESP_EXT1_WAKEUP_ANY_LOW);
    bno.enterSuspendMode();
    esp_deep_sleep_start();
}

float getBatteryPercent() {
    digitalWrite(PIN_VBAT_ENABLE, LOW); delay(5);
    int raw = analogRead(PIN_VBAT_READ);
    digitalWrite(PIN_VBAT_ENABLE, HIGH);
    float v = (raw * 3.3 / 4095.0) * 2.1; 
    float p = (v - 3.4) / (4.2 - 3.4) * 100.0;
    return constrain(p, 0.0, 100.0);
}

void toggleWiFiMode() {
    cfg.isAP = !cfg.isAP;
    prefs.begin("imu", false);
    prefs.putBytes("cfg", &cfg, sizeof(Config));
    prefs.end();
    // Flash de confirmation
    for(int i=0; i<10; i++) { digitalWrite(LED_USER, !digitalRead(LED_USER)); delay(50); }
    ESP.restart(); 
}

// =================================================================================
// INTERFACE WEB
// =================================================================================

void handleRoot() {
    String h = "<html><body style='font-family:sans-serif;padding:20px;'><h1>Configuration Gant IMU</h1>";
    h += "<p>Batterie : " + String(getBatteryPercent(), 0) + "%</p>";
    h += "<form action='/save' method='POST'>";
    h += "WiFi SSID : <input name='s' value='"+String(cfg.ssid)+"'><br><br>";
    h += "WiFi PASS : <input name='p' type='password' placeholder='Inchange'><br><br>";
    h += "Destination IP PC : <input name='ip' value='"+String(cfg.targetIp)+"'><br><br>";
    h += "Port de Sortie : <input name='po' type='number' value='"+String(cfg.portOut)+"'><br><br>";
    h += "<input type='submit' value='Enregistrer et Redemarrer'></form></body></html>";
    server.send(200, "text/html", h);
}

void handleSave() {
    if (server.arg("s")!="") strncpy(cfg.ssid, server.arg("s").c_str(), 32);
    if (server.arg("p")!="") strncpy(cfg.pass, server.arg("p").c_str(), 32);
    if (server.arg("ip")!="") strncpy(cfg.targetIp, server.arg("ip").c_str(), 16);
    if (server.arg("po")!="") cfg.portOut = server.arg("po").toInt();
    prefs.begin("imu", false);
    prefs.putBytes("cfg", &cfg, sizeof(Config));
    prefs.end();
    server.send(200, "text/plain", "Sauvegarde OK. Redemarrage en cours...");
    delay(2000); ESP.restart();
}

// =================================================================================
// SETUP
// =================================================================================

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(LED_USER, OUTPUT);
    pinMode(PIN_VBAT_ENABLE, OUTPUT);
    analogReadResolution(12);

    // Charger les réglages de la mémoire flash
    prefs.begin("imu", false);
    if (prefs.getBytes("cfg", &cfg, sizeof(Config)) == 0) {
        strncpy(cfg.ssid, DEFAULT_SSID, 32); strncpy(cfg.pass, DEFAULT_PASS, 32);
        strcpy(cfg.mdns, DEFAULT_MDNS); strcpy(cfg.targetIp, "0.0.0.0");
        cfg.portOut = DEFAULT_PORT_OUT; cfg.isAP = true;
    }
    prefs.end();

    currentDestIp.fromString(cfg.targetIp);

    // Démarrage WiFi
    if (cfg.isAP) {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Gant-IMU-Config", "");
    } else {
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.ssid, cfg.pass);
    }
    MDNS.begin(cfg.mdns);

    // Serveur Web
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.begin();

    // Capteur IMU
    Wire.begin(4, 5);
    Wire.setClock(100000); // 100kHz pour stabilité rallonge
    if (bno.begin()) {
        bno.setExtCrystalUse(true);
        Serial.println("BNO055 OK");
    }

    Udp.begin(9000);
    Serial.println("Systeme Pret");
}

// =================================================================================
// LOOP
// =================================================================================

void loop() {
    server.handleClient();
    unsigned long now = millis();

    // 1. GESTION LED (Visual Feedback)
    if (hasRecipient && WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_USER, LOW); // FIXE si envoi OSC
    } else if (cfg.isAP) {
        int cycle = now % 1500; // Pattern AP : Bip-Bip ---
        if (cycle < 80 || (cycle > 180 && cycle < 260)) digitalWrite(LED_USER, LOW);
        else digitalWrite(LED_USER, HIGH);
    } else {
        // Pattern Station : Blink lent
        if (now % 1000 < 500) digitalWrite(LED_USER, LOW); else digitalWrite(LED_USER, HIGH);
    }

    // 2. GESTION BOUTON
    bool btnDown = (digitalRead(PIN_BUTTON) == LOW);
    if (btnDown && !isPressed) { btnPressStart = now; isPressed = true; delay(20); }
    else if (!btnDown && isPressed) {
        unsigned long dur = now - btnPressStart; isPressed = false;
        if (dur > 50 && dur < 600) { clickCount++; lastClickTime = now; }
    }

    if (isPressed && (now - btnPressStart > 5000)) goToSleep();

    if (clickCount > 0 && (now - lastClickTime > 500)) {
        if (clickCount == 3) toggleWiFiMode();
        if (clickCount == 1) { 
            quatOffset = bno.getQuat(); 
            for(int i=0; i<6; i++) { digitalWrite(LED_USER, !digitalRead(LED_USER)); delay(80); }
        }
        clickCount = 0;
    }

    // 3. ENVOI OSC (100Hz)
    if (WiFi.status() == WL_CONNECTED && now - lastOSCTime >= 10) {
        lastOSCTime = now;
        if (hasRecipient) {
            sensors_event_t o; bno.getEvent(&o, Adafruit_BNO055::VECTOR_EULER);
            imu::Quaternion qC = bno.getQuat();
            imu::Quaternion qR = quatOffset.conjugate() * qC; qR.normalize();
            imu::Vector<3> eR = qR.toEuler();
            uint8_t sys, g, a, m; bno.getCalibration(&sys, &g, &a, &m);

            OSCBundle bundle;
            bundle.add("/euler/x").add(o.orientation.x);
            bundle.add("/rel/euler/x").add(eR.x() * 180 / M_PI);
            bundle.add("/battery").add(getBatteryPercent());
            bundle.add("/calib/sys").add(sys);
            
            Udp.beginPacket(currentDestIp, cfg.portOut);
            bundle.send(Udp);
            Udp.endPacket();
            bundle.empty();
        }
    }

    // 4. RÉCEPTION OSC (Handshake)
    int size = Udp.parsePacket();
    if (size > 0) {
        currentDestIp = Udp.remoteIP();
        hasRecipient = true;
        OSCMessage msgIn;
        while (size--) msgIn.fill(Udp.read());
    }
}