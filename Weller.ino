constexpr const char* VERSION = "Version 0.40";


// Changelog:
//    V0.30:    Neues Konfigurationselement: Lötkolbengewicht eingeführt 46g Default
//    V0.30a:   Kakibrieungsgewicht nun in gramm
//    V0.30c:   Anzeige in gramm
//    V0.30d:   angepasstes Tara
//    V0.30e    verbesserter Tara und Restart Prozess  
//    V0.30f:   verbesserterRestart Prozess  
//    V0.40     Die Waage Funktion verschnidet in den Hintergrund: Jetzt sind wir beim Weller Standby Controller :-)

#include <Arduino.h>
#include "Waage.h"
#include "WifiConfigManager.h"
#include "UI.h"
#include <Preferences.h>
#include <WiFi.h>

#define WAAGE_DEBUG 1

// ------------------------------
// Pins
// ------------------------------
const int HX711_DOUT = 25;
const int HX711_SCK  = 27;
const int BUTTON_PIN = 13;
const int LED_PIN    = 17;
const int RELAY_PIN  = 16;

// ------------------------------
// WifiConfigManager – Strukturen
// ------------------------------
ConfigStruc config = {
  "Weller Controller", "","","WellerESP","",1883,"","",false
};

// Keys: die nichtflüchtigen Parameter werden unter diesen Keys in den Preferences (NVM) gespeichert
#define key_Kalibirierungsgewicht "calWeight"
#define key_Kalibrierungsfaktor   "calFactor"
#define key_offset                "offset"
#define key_kalibriert            "calibrated"
#define key_akkusticalarm         "alarm"
#define key_kolbengewicht         "ioronG"

ExtraStruc extraParams[] = {  // Reiehnfolge der Typen: keyName[16] | FormType | TEXTvalue[64] | FLOATvalue | BOOLvalue | LONGvalue | bool optional | bool inputParam
  { key_Kalibirierungsgewicht, LONG, "", -1.0, false, 410, false, true },
  { key_Kalibrierungsfaktor,   FLOAT, "", 1.0,  false, -1, false, false },
  { key_offset,                LONG,  "", -1.0, false, 0,  false, false },
  { key_kalibriert,            BOOL,  "", -1.0, false, -1, false, false },
  { key_akkusticalarm,         BOOL,  "", -1.0, false, -1, false, true },
  { key_kolbengewicht,         LONG,  "", -1.0, false, 46, false, true }
};
constexpr size_t ANZ_EXTRA_PARAMS = sizeof(extraParams) / sizeof(extraParams[0]);

const WebStruc webForm[] = {
  { TITLE, "Weller Controller", "" },
  { CONFIGBLOCK, "", "" },
  { BLANK, "", "" }, { SEPARATOR, "", "" }, { BLANK, "", "" },
  { PARAMETER, "Kalibrierungsgewicht [g]", key_Kalibirierungsgewicht },
  { PARAMETER, "Lötkolbengewicht [g]",      key_kolbengewicht },
  { PARAMETER, "Kalibrierungsfaktor",       key_Kalibrierungsfaktor },
  { PARAMETER, "Waagen-Offset",             key_offset },
  { PARAMETER, "Waage kalibriert",          key_kalibriert },
  { BLANK, "", "" },
  { PARAMETER, "Akkustischer Alarm",        key_akkusticalarm },
  { BLANK, "", "" }
};
constexpr size_t ANZ_WEBFORM_ITEMS = sizeof(webForm) / sizeof(webForm[0]);

UI ui(BUTTON_PIN, LED_PIN);
WifiConfigManager configManager(&config, extraParams, webForm, ANZ_WEBFORM_ITEMS, ANZ_EXTRA_PARAMS, VERSION, &ui);

// ------------------------------
// Module instances
// ------------------------------
Waage meineWaage(HX711_DOUT, HX711_SCK);

// ------------------------------
// Finite State Machine (FSM)
// ------------------------------
enum class SystemState {
    INIT,
    READY,
    ACTIVE,
    INACTIVE,
    STANDBY,
    // Setup States
    MENU_TARE,
    MENU_CALIBRATE,
    MENU_INFO,
    MENU_RESET,
    CALIBRATION_CHECK_WEIGHT,
    CALIBRATION_STEP_1_START,
    CALIBRATION_STEP_2_EMPTY,
    CALIBRATION_DONE
};

SystemState currentState = SystemState::INIT;

#if WAAGE_DEBUG
SystemState lastPublishedState = SystemState::INIT;
#endif

// FSM Timers and Constants
const long StationStandbyTime = 30; // seconds kurz für Test
const long secureTime = 10; // seconds
unsigned long standbyTimer_start = 0;
unsigned long operationTimer_start = 0;
unsigned long standby_entered_timestamp = 0;
unsigned long menuTimeoutStart = 0;
const unsigned long MENU_TIMEOUT_MS    = 8000;

// ------------------------------
// Helpers
// ------------------------------
static void setExtraFloat(const char* key, float v){ for(size_t i=0; i<ANZ_EXTRA_PARAMS; i++){ if(strcmp(extraParams[i].keyName,key)==0){ extraParams[i].FLOATvalue=v; return; } } }
static void setExtraLong (const char* key, long v) { for(size_t i=0; i<ANZ_EXTRA_PARAMS; i++){ if(strcmp(extraParams[i].keyName,key)==0){ extraParams[i].LONGvalue =v; return; } } }
static void setExtraBool (const char* key, bool v) { for(size_t i=0; i<ANZ_EXTRA_PARAMS; i++){ if(strcmp(extraParams[i].keyName,key)==0){ extraParams[i].BOOLvalue =v; return; } } }

// --- Action Functions ---
void restartStation() {
    digitalWrite(RELAY_PIN, HIGH);
    delay(100);
    digitalWrite(RELAY_PIN, LOW);
}

void startStandbyTimer() {
    standbyTimer_start = millis();
}

void stopStandbyTimer() {
    standbyTimer_start = 0;
}

void startOperationTimer() {
    operationTimer_start = millis();
}

void stopOperationTimer() {
    operationTimer_start = 0;
}

static void factoryResetAndReboot(){
  Preferences p;
  p.begin("network", false); p.clear(); p.end();
  p.begin("operation", false); p.clear(); p.end();
  ui.showMessage("Neustart.....", "", 0);
  unsigned long startTime = millis();
  while(millis() - startTime < 2000) { /* non-blocking delay */ }
  ESP.restart();
}

static String getBaseTopic() {
    String base = configManager.getMdnsName();
    if (base.length() == 0) base = F("waage");
    return base;
}

#if WAAGE_DEBUG
const char* systemStateToString(SystemState state) {
    switch (state) {
        case SystemState::INIT:     return "INIT";
        case SystemState::READY:    return "READY";
        case SystemState::ACTIVE:   return "ACTIVE";
        case SystemState::INACTIVE: return "INACTIVE";
        case SystemState::STANDBY:  return "STANDBY";
        case SystemState::MENU_TARE: return "MENU_TARE";
        case SystemState::MENU_CALIBRATE: return "MENU_CALIBRATE";
        case SystemState::MENU_INFO: return "MENU_INFO";
        case SystemState::MENU_RESET: return "MENU_RESET";
        case SystemState::CALIBRATION_CHECK_WEIGHT: return "CALIBRATION_CHECK_WEIGHT";
        case SystemState::CALIBRATION_STEP_1_START: return "CALIBRATION_STEP_1_START";
        case SystemState::CALIBRATION_STEP_2_EMPTY: return "CALIBRATION_STEP_2_EMPTY";
        case SystemState::CALIBRATION_DONE: return "CALIBRATION_DONE";
        default: return "UNKNOWN_STATE";
    }
}
#endif


// ------------------------------
// MQTT
// ------------------------------
static void mqttPublishLoop() {
    static unsigned long lastMqttPub = 0;
    if (millis() - lastMqttPub < 5000) return;
    lastMqttPub = millis();

    if (!configManager.isWifiConnected()) return;
    configManager.ensureMqttConnected();
    if (!configManager.isMqttConnected()) return;

    String base = getBaseTopic();
    char buf[16];
    dtostrf(meineWaage.getGewicht(), 0, 0, buf);
    configManager.publish((base + F("/gewicht_g")).c_str(), String(buf), true, 0);
    configManager.publish((base + F("/calibrated")).c_str(), meineWaage.istKalibriert() ? "1" : "0", true, 0);
    configManager.publish((base + F("/rssi")).c_str(), String(configManager.getRSSI()), true, 0);
}

// ------------------------------
// Setup
// ------------------------------
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println(VERSION);

    configManager.begin("SmartScale");
    ui.begin(VERSION);
    
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // Relay is NC, LOW means station is ON

    KalibrierungsDaten kd{};
    kd.kalibrierungsfaktor = configManager.getExtraParamFloat(key_Kalibrierungsfaktor);
    kd.tareOffset          = configManager.getExtraParamInt(key_offset);
    kd.istKalibriert       = configManager.getExtraParamBool(key_kalibriert);
    meineWaage.begin(kd);
    meineWaage.tare(); 
}

// ------------------------------
// Loop - FSM Implementation
// ------------------------------
void loop() {
    // --- Continuous updates ---
    configManager.handleLoop();
    meineWaage.loop();
    mqttPublishLoop();
    ui.handleUpdates(configManager.isWifiConnected());

    // --- Get button press ---
    ButtonPressType press = ui.getButtonPress();

    // --- Get sensor data ---
    float currentWeight = meineWaage.getGewicht();
    long ironWeight = configManager.getExtraParamInt(key_kolbengewicht);
    long weightThreshold = ironWeight / 2;

    // --- FSM State Handling ---
    unsigned long now = millis();
    
    // --- Setup Mode Entry & Timeout ---
    bool isMenuState = (currentState == SystemState::MENU_TARE || currentState == SystemState::MENU_CALIBRATE || currentState == SystemState::MENU_INFO || currentState == SystemState::MENU_RESET);
    
    if (!isMenuState && press == ButtonPressType::LONG_5S) {
        currentState = SystemState::MENU_TARE;
        menuTimeoutStart = now;
    }
    if (isMenuState && (now - menuTimeoutStart > MENU_TIMEOUT_MS)) {
        currentState = SystemState::READY;
    }

    // --- Operational Mode Logic ---
    unsigned long standbyTimeLeft = (standbyTimer_start > 0) ? ((StationStandbyTime - secureTime) - (now - standbyTimer_start) / 1000) : 0;

    if (standbyTimer_start > 0 && (now - standbyTimer_start > (StationStandbyTime - secureTime) * 1000)) {
        if (currentState == SystemState::READY || currentState == SystemState::INACTIVE) {
            stopStandbyTimer(); // A5
            standby_entered_timestamp = now;
            currentState = SystemState::STANDBY;
        } else if (currentState == SystemState::ACTIVE) {
            restartStation(); // A1
            startStandbyTimer(); // A2
        }
    }

    switch (currentState) {
        case SystemState::INIT:
            startStandbyTimer(); // A2
            ui.displayReady(standbyTimeLeft);
            currentState = SystemState::READY;
            break;

        case SystemState::READY:
            ui.displayReady(standbyTimeLeft);
            if (currentWeight < -weightThreshold) { // T1: ironRemoved
                restartStation();
                startStandbyTimer();
                startOperationTimer();
                currentState = SystemState::ACTIVE;
            }
            break;

        case SystemState::ACTIVE:
            {
                unsigned long operationTime = (operationTimer_start > 0) ? (now - operationTimer_start) / 1000 : 0;
                ui.displayActive(operationTime, standbyTimeLeft);
            }
            if (currentWeight > -weightThreshold) { // T2: ironBack
                stopOperationTimer();
                currentState = SystemState::INACTIVE;
            }
            break;

        case SystemState::INACTIVE:
            ui.displayInactive(standbyTimeLeft);
            if (currentWeight < -weightThreshold) { // T1: ironRemoved
                restartStation();
                startStandbyTimer();
                startOperationTimer();
                currentState = SystemState::ACTIVE;
            }
            break;

        case SystemState::STANDBY:
            {
                unsigned long standbyTime = (standby_entered_timestamp > 0) ? (now - standby_entered_timestamp) / 1000 : 0;
                ui.displayStandby(standbyTime);
            }
            if (press == ButtonPressType::SHORT) { // T3: buttonPress
                currentState = SystemState::READY;
            }
            if (currentWeight < -weightThreshold) { // T1: ironRemoved
                restartStation();
                startStandbyTimer();
                startOperationTimer();
                currentState = SystemState::ACTIVE;
            }
            break;

        // --- Setup Mode States ---
        case SystemState::MENU_TARE:
            ui.drawTarePage();
            if (press == ButtonPressType::SHORT) {
                currentState = SystemState::MENU_CALIBRATE;
                menuTimeoutStart = now;
            } else if (press == ButtonPressType::LONG_2S) {
                meineWaage.tare();
                ui.showMessage("Tare", "erfolgreich", 1000);
                currentState = SystemState::READY;
            }
            break;

        case SystemState::MENU_CALIBRATE:
            ui.drawCalibratePage();
            if (ui.isHeld() && ui.getHoldDuration() > 5000) {
                ui.drawCheckmark();
            }
            if (press == ButtonPressType::SHORT) {
                currentState = SystemState::MENU_INFO;
                menuTimeoutStart = now;
            } else if (press == ButtonPressType::LONG_5S) {
                currentState = SystemState::CALIBRATION_CHECK_WEIGHT;
            }
            break;

        case SystemState::MENU_INFO:
            ui.drawInfoPage(meineWaage.getTareOffset(), meineWaage.getKalibrierungsfaktor(), WiFi.localIP().toString(), configManager.isMqttConnected());
            if (press == ButtonPressType::SHORT) {
                currentState = SystemState::MENU_RESET;
                menuTimeoutStart = now;
            }
            break;

        case SystemState::MENU_RESET:
            ui.drawResetPage();
            if (press == ButtonPressType::SHORT) {
                currentState = SystemState::READY;
            } else if (press == ButtonPressType::LONG_10S) {
                factoryResetAndReboot();
            }
            break;
            
        case SystemState::CALIBRATION_CHECK_WEIGHT:
            {
                long calW_g = configManager.getExtraParamInt(key_Kalibirierungsgewicht);
                if (calW_g <= 0) {
                    ui.showMessage("Kal.-Gew. fehlt", "im Webformular", 2000);
                    currentState = SystemState::READY;
                } else {
                    currentState = SystemState::CALIBRATION_STEP_1_START;
                }
            }
            break;

        case SystemState::CALIBRATION_STEP_1_START:
            ui.showMessage("Kalibrierung..", "Platte leeren ","Dann Taste druecken!");
            if (press == ButtonPressType::SHORT) {
                meineWaage.tare();
                currentState = SystemState::CALIBRATION_STEP_2_EMPTY;
            }
            break;

        case SystemState::CALIBRATION_STEP_2_EMPTY:
            {
                char line2[32];
                sprintf(line2, "%ld g auflegen", configManager.getExtraParamInt(key_Kalibirierungsgewicht));

                ui.showMessage("Kalibrierung..",line2, "Dann Taste druecken!");
                if (press == ButtonPressType::SHORT) {
                    meineWaage.refreshDataSet();
                    long calW_g = configManager.getExtraParamInt(key_Kalibirierungsgewicht);
                    float newCalFactor = meineWaage.getNewCalibration(calW_g);
                    meineWaage.setKalibrierungsfaktor(newCalFactor);
                    meineWaage.setIstKalibriert(true);
                    
                    setExtraFloat(key_Kalibrierungsfaktor, newCalFactor);
                    setExtraLong(key_offset, meineWaage.getTareOffset());
                    setExtraBool(key_kalibriert, true);
                    configManager.saveConfig();
                    
                    currentState = SystemState::CALIBRATION_DONE;
                }
            }
            break;

        case SystemState::CALIBRATION_DONE:
            ui.showMessage("Kalibrierung", "erfolgreich", 1200);
            meineWaage.tare();
            currentState = SystemState::READY;
            break;
    }

    #if WAAGE_DEBUG
    if (currentState != lastPublishedState) {
        if (configManager.isWifiConnected()) {
            configManager.ensureMqttConnected();
            if (configManager.isMqttConnected()) {
                char json_payload[128];
                snprintf(json_payload, sizeof(json_payload), "{\"id\":%d, \"state\":\"%s\"}", static_cast<int>(currentState), systemStateToString(currentState));
                String topic = getBaseTopic() + F("/fsm_state");
                configManager.publish(topic.c_str(), json_payload, true, 0);
                Serial.printf("Published FSM state: %s\n", json_payload);
            }
        }
        lastPublishedState = currentState;
    }
    #endif
}
