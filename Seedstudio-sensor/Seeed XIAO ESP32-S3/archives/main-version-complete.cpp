/**
 * =================================================================================
 * PROJET : IMU GANT OSC - VERSION 6.4 (STABLE, WEARABLE & DYNAMIC IP)
 * MATÉRIEL : Seeed Studio XIAO ESP32S3 + Adafruit BNO055 + Batterie LiPo 3.7V
 * AUTEUR : Max Bruckert | DATE : AVRIL 2026
 * =================================================================================
 * 
 * 1. BRANCHEMENTS MATÉRIELS :
 *    - BNO055 (I2C) : SDA -> Pin D4 (GPIO 4) | SCL -> Pin D5 (GPIO 5)
 *    - BOUTON : Entre Pin D0 (GPIO 2) et GND.
 *    - BATTERIE : Pad BAT+ (rouge) et Pad BAT- (noir) au dos du XIAO.
 *    - ANTENNE : Connectée sur le connecteur U.FL (Indispensable !).
 * 
 * 2. GESTION DU BOUTON (Pin D0) :
 *    - ALLUMAGE : Si éteint (Deep Sleep), un appui court réveille l'appareil.
 *    - MISE EN SOMMEIL (Deep Sleep) : Maintenir appuyé > 6 secondes.
 *    - SET ZERO (Relatif) : Faire 3 appuis courts et rapides (triple-clic).
 * 
 * 3. RETOUR VISUEL LED (GPIO 21) :
 *    - CLIGNOTEMENT RAPIDE (200ms)  : Recherche du réseau WiFi.
 *    - CLIGNOTEMENT LENT (1000ms)   : WiFi OK, mais en attente de message "/connect".
 *    - FIXE (Allumé)                : WiFi OK + Appareil lié à un PC (Flux actif).
 *    - FLASH TRÈS RAPIDE (x10)      : Confirmation de commande (Zero/Sleep/Calib).
 * 
 * 4. PROCÉDURE DE CALIBRATION (Auto-Calibration BNO055) :
 *    - GYRO : Poser le gant immobile sur une table (3 sec). Scores de 0 à 3.
 *    - MAG  : Faire des "8" dans l'air avec la main. Scores de 0 à 3.
 *    - ACCEL : Pivoter la main lentement sur les 6 axes (90° par face). Scores de 0 à 3.
 * 
 * 5. DICTIONNAIRE OSC (Port Sortie 8000 | Port Entrée 9000 | Host: imu-gant.local) :
 *    ------------------------------------------------------------------------------
 *    ///// SORTIE (ESP32 -> PC) :
 *    - /euler/[x,y,z]      : Angles absolus (Nord magnétique).
 *    - /quat/[w,x,y,z]     : Quaternions absolus (Rotation 3D fluide).
 *    - /rel/euler/[x,y,z]  : Angles relatifs au dernier "/set_zero" (Visée).
 *    - /rel/quat/[w,x,y,z] : Quaternions relatifs au point ZERO défini.
 *    - /accel/[x,y,z]      : Accélération linéaire (m/s²) sans la gravité.
 *    - /calib/[sys,g,a,m]  : Score de calibration (0 à 3). 3 = Parfait.
 *    - /battery            : Pourcentage de batterie LiPo (0-100%).
 * 
 *    ///// ENTRÉE (PC -> ESP32) :
 *    - /connect            : Lie l'IP du PC au gant et démarre le flux OSC.
 *    - /set_zero           : L'orientation actuelle devient le point 0 relatif.
 *    - /recalibrate_gyro   : Réinitialise la fusion de données du capteur.
 *    - /recalibrate_mag    : Réinitialise la fusion de données du capteur.
 *    - /recalibrate_acc    : Réinitialise la fusion de données du capteur.
 *    - /deepsleep          : Éteint l'appareil à distance (Deep Sleep).
 * =================================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h> 
#include <OSCBundle.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

// --- CONFIGURATION RÉSEAU ---
const char* ssid     = "bruckertbox";  
const char* password = "dancelascive1987";         
// Remplace par ton IP PC par défaut (elle sera mise à jour au premier message reçu)
IPAddress currentDestIp(192, 168, 1, 6); 

const unsigned int destPort = 8000;         
const unsigned int localPort = 9000;        

// --- PINS MATÉRIEL ---
const int PIN_BUTTON      = 2;              
const int PIN_VBAT_READ   = 1;              
const int PIN_VBAT_ENABLE = 14;             
const int LED_USER        = LED_BUILTIN;    

// --- OBJETS ---
WiFiUDP Udp;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);

// --- VARIABLES DE GESTION ---
unsigned long lastBlinkTime = 0;
unsigned long lastBatteryCheck = 0;
unsigned long lastCalibLog = 0;
bool isConnectedWiFi = false;
bool hasRecipient = false; // Passe à true dès qu'un message OSC est reçu
bool ledState = HIGH;

unsigned long buttonPressStart = 0;
int clickCount = 0;
unsigned long lastClickTime = 0;
bool isPressed = false;

imu::Quaternion quatOffset = imu::Quaternion(1, 0, 0, 0);

// ==========================================
// FONCTION : FEEDBACK VISUEL (LED)
// ==========================================
void visualFeedback(int speed, int count) {
    for(int i=0; i<count; i++) {
        digitalWrite(LED_USER, LOW); delay(speed);
        digitalWrite(LED_USER, HIGH); delay(speed);
    }
}

// ==========================================
// FONCTION : PASSAGE EN DEEP SLEEP (VEILLE)
// ==========================================
void goToSleep() {
    Serial.println("Action Sleep : Deep Sleep activé");
    visualFeedback(100, 10);
    digitalWrite(LED_USER, HIGH); // Éteint la LED
    // Réveil sur bouton D0 (GPIO 2)
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_BUTTON, ESP_EXT1_WAKEUP_ANY_LOW);
    bno.enterSuspendMode();
    esp_deep_sleep_start();
}

// ==========================================
// FONCTION : LECTURE BATTERIE (%)
// ==========================================
float getBatteryPercentage() {
    digitalWrite(PIN_VBAT_ENABLE, LOW);
    delay(2);
    int raw = analogRead(PIN_VBAT_READ);
    digitalWrite(PIN_VBAT_ENABLE, HIGH); 
    float voltage = (raw * 3.3 / 4095.0) * 2.0; 
    float percent = (voltage - 3.4) / (4.2 - 3.4) * 100.0;
    return constrain(percent, 0.0, 100.0);
}

// ==========================================
// SETUP : INITIALISATION
// ==========================================
void setup() {
    Serial.begin(115200);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(LED_USER, OUTPUT);
    pinMode(PIN_VBAT_ENABLE, OUTPUT);
    analogReadResolution(12);

    // WiFi Config
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Réduit la latence OSC
    WiFi.begin(ssid, password);
    
    // Initialisation mDNS (imu-gant.local)
    if (MDNS.begin("imu-gant")) {
        Serial.println("mDNS: imu-gant.local");
    }

    if (!bno.begin()) Serial.println("ERREUR : BNO055 non trouvé");
    else bno.setExtCrystalUse(true);

    Udp.begin(localPort);
}

// ==========================================
// LOOP : BOUCLE PRINCIPALE
// ==========================================
void loop() {
    unsigned long currentMillis = millis();

    // --- 1. GESTION WIFI & LED STATUS ---
    if (WiFi.status() != WL_CONNECTED) {
        isConnectedWiFi = false;
        if (currentMillis - lastBlinkTime >= 200) { // Flash rapide : cherche réseau
            lastBlinkTime = currentMillis;
            ledState = !ledState;
            digitalWrite(LED_USER, ledState);
        }
    } else {
        isConnectedWiFi = true;
        if (hasRecipient) {
            digitalWrite(LED_USER, LOW); // FIXE : Flux actif
        } else {
            if (currentMillis - lastBlinkTime >= 1000) { // Flash lent : attend /connect
                lastBlinkTime = currentMillis;
                ledState = !ledState;
                digitalWrite(LED_USER, ledState);
            }
        }
    }

    // --- 2. SÉCURITÉ BATTERIE (Toutes les 60s) ---
    if (currentMillis - lastBatteryCheck > 60000) {
        lastBatteryCheck = currentMillis;
        if (getBatteryPercentage() < 5.0) goToSleep(); 
    }

    // --- 3. GESTION BOUTON (Deep Sleep & Zero) ---
    bool buttonLevel = digitalRead(PIN_BUTTON);
    if (buttonLevel == LOW && !isPressed) {
        buttonPressStart = currentMillis;
        isPressed = true;
    } 
    else if (buttonLevel == HIGH && isPressed) {
        unsigned long duration = currentMillis - buttonPressStart;
        isPressed = false;
        if (duration < 500) { clickCount++; lastClickTime = currentMillis; }
    }
    // Deep Sleep (6 sec)
    if (isPressed && (currentMillis - buttonPressStart > 6000)) goToSleep();
    // Triple Clic (Set Zero)
    if (clickCount >= 3) {
        clickCount = 0;
        quatOffset = bno.getQuat(); 
        visualFeedback(50, 8);
    }
    if (currentMillis - lastClickTime > 800) clickCount = 0;

    // --- 4. LECTURE CAPTEUR & CALCULS RELATIFS ---
    sensors_event_t orient, gyro, accel, mag;
    bno.getEvent(&orient, Adafruit_BNO055::VECTOR_EULER);
    bno.getEvent(&gyro,   Adafruit_BNO055::VECTOR_GYROSCOPE);
    bno.getEvent(&accel,  Adafruit_BNO055::VECTOR_ACCELEROMETER);
    bno.getEvent(&mag,    Adafruit_BNO055::VECTOR_MAGNETOMETER);
    
    imu::Quaternion quatCurrent = bno.getQuat();
    imu::Vector<3> linAccel = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
    
    // Calcul de la rotation relative (Visée)
    imu::Quaternion quatRel = quatOffset.conjugate() * quatCurrent;
    quatRel.normalize();
    imu::Vector<3> relEuler = quatRel.toEuler();
    
    uint8_t c_sys, c_gyro, c_acc, c_mag;
    bno.getCalibration(&c_sys, &c_gyro, &c_acc, &c_mag);

    // Log Calibration en série pour aide au réglage
    if (currentMillis - lastCalibLog > 2000) {
        lastCalibLog = currentMillis;
        Serial.printf("CALIB: S:%d G:%d A:%d M:%d\n", c_sys, c_gyro, c_acc, c_mag);
    }

    // --- 5. ENVOI DU FLUX OSC (Si PC lié) ---
    if (isConnectedWiFi && hasRecipient) {
        OSCBundle bundle;
        // Absolus
        bundle.add("/euler/x").add(orient.orientation.x);
        bundle.add("/euler/y").add(orient.orientation.y);
        bundle.add("/euler/z").add(orient.orientation.z);
        bundle.add("/quat/w").add((float)quatCurrent.w());
        bundle.add("/quat/x").add((float)quatCurrent.x());
        bundle.add("/quat/y").add((float)quatCurrent.y());
        bundle.add("/quat/z").add((float)quatCurrent.z());
        // Relatifs (Visée)
        bundle.add("/rel/euler/x").add(relEuler.x() * 180 / M_PI);
        bundle.add("/rel/euler/y").add(relEuler.y() * 180 / M_PI);
        bundle.add("/rel/euler/z").add(relEuler.z() * 180 / M_PI);
        bundle.add("/rel/quat/w").add((float)quatRel.w());
        bundle.add("/rel/quat/x").add((float)quatRel.x());
        bundle.add("/rel/quat/y").add((float)quatRel.y());
        bundle.add("/rel/quat/z").add((float)quatRel.z());
        // Physique
        bundle.add("/accel/x").add((float)linAccel.x());
        bundle.add("/accel/y").add((float)linAccel.y());
        bundle.add("/accel/z").add((float)linAccel.z());
        bundle.add("/gyro/x").add(gyro.gyro.x);
        bundle.add("/gyro/y").add(gyro.gyro.y);
        bundle.add("/gyro/z").add(gyro.gyro.z);
        // Calibration & Batterie
        bundle.add("/calib/sys").add(c_sys);
        bundle.add("/calib/gyro").add(c_gyro);
        bundle.add("/calib/acc").add(c_acc);
        bundle.add("/calib/mag").add(c_mag);
        bundle.add("/battery").add(getBatteryPercentage());

        Udp.beginPacket(currentDestIp, destPort);
        bundle.send(Udp);
        Udp.endPacket();
        bundle.empty(); 
    }

    // --- 6. RÉCEPTION OSC (COMMANDES & HANDSHAKE) ---
    OSCMessage msgIn;
    int size = Udp.parsePacket();
    if (size > 0) {
        // Apprentissage dynamique de l'IP du PC
        currentDestIp = Udp.remoteIP();
        hasRecipient = true; 
        
        while (size--) msgIn.fill(Udp.read());
        if (!msgIn.hasError()) {
            if (msgIn.fullMatch("/set_zero") || msgIn.fullMatch("/connect")) {
                quatOffset = bno.getQuat();
                visualFeedback(50, 10);
            }
            if (msgIn.fullMatch("/recalibrate_gyro") || msgIn.fullMatch("/recalibrate_mag") || msgIn.fullMatch("/recalibrate_acc")) {
                bno.begin(); 
                visualFeedback(50, 10);
            }
            if (msgIn.fullMatch("/deepsleep")) goToSleep();
        }
    }
    delay(10); 
}