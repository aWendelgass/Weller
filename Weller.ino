constexpr const char* VERSION = "Version 0.20g";

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

// ------------------------------
// WifiConfigManager – Strukturen
// ------------------------------
ConfigStruc config = {
  "Weller Controller", "","","WellerESP","",1883,"","",false
};

#define key_Kalibirierungsgewicht "calWeight"
#define key_Kalibirierungfaktor   "calFactor"
#define key_offset                "offset"
#define key_kalibriert            "calibrated"
#define key_akkusticalarm         "alarm"

ExtraStruc extraParams[] = {
  { key_Kalibirierungsgewicht, FLOAT, "", 0.41, false, -1, false, true },
  { key_Kalibirierungfaktor,   FLOAT, "", 1.0,  false, -1, false, false },
  { key_offset,                LONG,  "", -1.0, false, 0,  false, false },
  { key_kalibriert,            BOOL,  "", -1.0, false, -1, false, false },
  { key_akkusticalarm,         BOOL,  "", -1.0, false, -1, false, true }
};
constexpr size_t ANZ_EXTRA_PARAMS = sizeof(extraParams) / sizeof(extraParams[0]);

const WebStruc webForm[] = {
  { TITLE, "Weller Controller", "" },
  { CONFIGBLOCK, "", "" },
  { BLANK, "", "" }, { SEPARATOR, "", "" }, { BLANK, "", "" },
  { PARAMETER, "Kalibrierungsgewicht (kg)", key_Kalibirierungsgewicht },
  { PARAMETER, "Kalibrierungsfaktor",       key_Kalibirierungfaktor },
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
    LIVE_VIEW,
    MENU_TARE,
    MENU_CALIBRATE,
    MENU_INFO,
    MENU_RESET,
    CALIBRATION_CHECK_WEIGHT,
    CALIBRATION_STEP_1_START,
    CALIBRATION_STEP_2_EMPTY,
    CALIBRATION_STEP_3_WEIGHT,
    CALIBRATION_DONE
};

SystemState currentState = SystemState::LIVE_VIEW;
#if WAAGE_DEBUG
SystemState lastPublishedState = SystemState::LIVE_VIEW;
#endif
unsigned long menuTimeoutStart = 0;
const unsigned long MENU_TIMEOUT_MS    = 8000;

// ------------------------------
// Helpers
// ------------------------------
static void setExtraFloat(const char* key, float v){ for(size_t i=0; i<ANZ_EXTRA_PARAMS; i++){ if(strcmp(extraParams[i].keyName,key)==0){ extraParams[i].FLOATvalue=v; return; } } }
static void setExtraLong (const char* key, long v) { for(size_t i=0; i<ANZ_EXTRA_PARAMS; i++){ if(strcmp(extraParams[i].keyName,key)==0){ extraParams[i].LONGvalue =v; return; } } }
static void setExtraBool (const char* key, bool v) { for(size_t i=0; i<ANZ_EXTRA_PARAMS; i++){ if(strcmp(extraParams[i].keyName,key)==0){ extraParams[i].BOOLvalue =v; return; } } }

static void factoryResetAndReboot(){
  Preferences p;
  p.begin("network", false); p.clear(); p.end();
  p.begin("operation", false); p.clear(); p.end();
  delay(200);
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
        case SystemState::LIVE_VIEW: return "LIVE_VIEW";
        case SystemState::MENU_TARE: return "MENU_TARE";
        case SystemState::MENU_CALIBRATE: return "MENU_CALIBRATE";
        case SystemState::MENU_INFO: return "MENU_INFO";
        case SystemState::MENU_RESET: return "MENU_RESET";
        case SystemState::CALIBRATION_CHECK_WEIGHT: return "CALIBRATION_CHECK_WEIGHT";
        case SystemState::CALIBRATION_STEP_1_START: return "CALIBRATION_STEP_1_START";
        case SystemState::CALIBRATION_STEP_2_EMPTY: return "CALIBRATION_STEP_2_EMPTY";
        case SystemState::CALIBRATION_STEP_3_WEIGHT: return "CALIBRATION_STEP_3_WEIGHT";
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
    dtostrf(meineWaage.getGewichtKg(), 0, 1, buf);
    configManager.publish((base + F("/gewicht_kg")).c_str(), String(buf), true, 0);
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

    KalibrierungsDaten kd{};
    kd.kalibrierungsfaktor = configManager.getExtraParamFloat(key_Kalibirierungfaktor);
    kd.tareOffset          = configManager.getExtraParamInt(key_offset);
    kd.istKalibriert       = configManager.getExtraParamBool(key_kalibriert);
    meineWaage.begin(kd);
    meineWaage.tare();
    meineWaage.setAnzeigeGenauigkeitGramm(100);
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

    // --- Menu Timeout Logic ---
    bool isMenuState = (currentState == SystemState::MENU_TARE ||
                        currentState == SystemState::MENU_CALIBRATE ||
                        currentState == SystemState::MENU_INFO ||
                        currentState == SystemState::MENU_RESET);

    if (isMenuState && millis() - menuTimeoutStart > MENU_TIMEOUT_MS) {
        currentState = SystemState::LIVE_VIEW;
    }

    // --- FSM State Handling ---
    switch (currentState) {
        case SystemState::LIVE_VIEW:
            ui.drawLivePage(meineWaage.getGewichtKg(), meineWaage.istKalibriert(), configManager.isWifiConnected(), configManager.getRSSI());
            if (press == ButtonPressType::SHORT) {
                currentState = SystemState::MENU_TARE;
                menuTimeoutStart = millis();
            }
            break;

        case SystemState::MENU_TARE:
            ui.drawTarePage();
            if (press == ButtonPressType::SHORT) {
                currentState = SystemState::MENU_CALIBRATE;
                menuTimeoutStart = millis();
            } else if (press == ButtonPressType::LONG_2S) {
                meineWaage.tare();
                setExtraLong(key_offset, meineWaage.getTareOffset());
                configManager.saveConfig();
                ui.showMessage("Tare", "erfolgreich", 1000);
                currentState = SystemState::LIVE_VIEW;
            }
            break;

        case SystemState::MENU_CALIBRATE:
            ui.drawCalibratePage();
            if (ui.isHeld() && ui.getHoldDuration() > 5000) {
                ui.drawCheckmark();
            }
            if (press == ButtonPressType::SHORT) {
                currentState = SystemState::MENU_INFO;
                menuTimeoutStart = millis();
            } else if (press == ButtonPressType::LONG_5S) {
                currentState = SystemState::CALIBRATION_CHECK_WEIGHT;
            }
            break;

        case SystemState::MENU_INFO:
            ui.drawInfoPage(meineWaage.getTareOffset(), meineWaage.getKalibrierungsfaktor(), WiFi.localIP().toString(), configManager.isMqttConnected());
            if (press == ButtonPressType::SHORT) {
                currentState = SystemState::MENU_RESET;
                menuTimeoutStart = millis();
            }
            break;

        case SystemState::MENU_RESET:
            ui.drawResetPage();
            if (press == ButtonPressType::SHORT) {
                currentState = SystemState::LIVE_VIEW;
            } else if (press == ButtonPressType::LONG_10S) {
                factoryResetAndReboot();
            }
            break;
            
        case SystemState::CALIBRATION_CHECK_WEIGHT:
            {
                float calW_kg = configManager.getExtraParamFloat(key_Kalibirierungsgewicht);
                if (calW_kg <= 0.0f) {
                    ui.showMessage("Kal.-Gew. fehlt", "im Webformular", 2000);
                    currentState = SystemState::LIVE_VIEW;
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
            //float calW_kg = configManager.getExtraParamFloat(key_Kalibirierungsgewicht);
            char line2[32];
            sprintf(line2, "%.2f kg auflegen", configManager.getExtraParamFloat(key_Kalibirierungsgewicht));

            ui.showMessage("Kalibrierung..",line2, "Dann Taste druecken!");
             if (press == ButtonPressType::SHORT) {
                meineWaage.refreshDataSet();
                float calW_kg = configManager.getExtraParamFloat(key_Kalibirierungsgewicht);
                float newCalFactor = meineWaage.getNewCalibration(calW_kg * 1000.0f);
                meineWaage.setKalibrierungsfaktor(newCalFactor);
                meineWaage.setIstKalibriert(true);
                
                setExtraFloat(key_Kalibirierungfaktor, newCalFactor);
                setExtraLong(key_offset, meineWaage.getTareOffset());
                setExtraBool(key_kalibriert, true);
                configManager.saveConfig();
                
                currentState = SystemState::CALIBRATION_DONE;
            }
            break;

        case SystemState::CALIBRATION_DONE:
            ui.showMessage("Kalibrierung", "erfolgreich", 1200);
            currentState = SystemState::LIVE_VIEW;
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
