constexpr const char* VERSION = "Version 0.06";

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

WifiConfigManager configManager(&config, extraParams, webForm, ANZ_WEBFORM_ITEMS, ANZ_EXTRA_PARAMS, VERSION);

// ------------------------------
// Module instances
// ------------------------------
Waage meineWaage(HX711_DOUT, HX711_SCK);
UI ui(BUTTON_PIN, LED_PIN);

// ------------------------------
// Globals for calibration state machine
// ------------------------------
CalibrationState calibrationState = CalibrationState::IDLE;

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

static void uiCallback(const char* line1, const char* line2) {
    ui.showMessage(line1, line2);
}

// ------------------------------
// MQTT
// ------------------------------
static void mqttPublishLoop() {
    static unsigned long lastMqttPub = 0;
    const unsigned long MQTT_PUB_MS = 5000;

    if (!configManager.isWifiConnected()) return;
    configManager.ensureMqttConnected();
    if (!configManager.isMqttConnected()) return;

    unsigned long now = millis();
    if (now - lastMqttPub < MQTT_PUB_MS) return;
    lastMqttPub = now;

    String base = getBaseTopic();
    char buf[16];
    dtostrf(meineWaage.getGewichtKg(), 0, 1, buf);
    configManager.publish((base + F("/gewicht_kg")).c_str(), String(buf), true, 0);
    configManager.publish((base + F("/calibrated")).c_str(), meineWaage.istKalibriert() ? "1" : "0", true, 0);
    configManager.publish((base + F("/rssi")).c_str(), String(configManager.getRSSI()), true, 0);
}

// ------------------------------
// Setup & Loop
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
    meineWaage.setAnzeigeGenauigkeitGramm(100);
    meineWaage.setUiCallback(uiCallback);
}

void loop() {
    configManager.handleLoop();
    meineWaage.loop();
    mqttPublishLoop();

    ui.update(
        meineWaage.getGewichtKg(),
        meineWaage.istKalibriert(),
        configManager.isWifiConnected(),
        configManager.getRSSI(),
        meineWaage.getTareOffset(),
        meineWaage.getKalibrierungsfaktor(),
        WiFi.localIP().toString(),
        configManager.isMqttConnected()
    );

    ButtonPressType press = ui.getButtonPress();

    if (ui.getUiPage() == UI::UiPage::CALIBRATION) {
        if (calibrationState == CalibrationState::IDLE) {
            calibrationState = meineWaage.kalibriereWaage(calibrationState, 0.0f);
        }
        if (press == ButtonPressType::SHORT) {
            if (calibrationState == CalibrationState::WAITING_FOR_TARE) {
                calibrationState = CalibrationState::TARE_DONE;
                ui.blinkLed(3, 100);
            } else if (calibrationState == CalibrationState::WAITING_FOR_WEIGHT) {
                calibrationState = CalibrationState::WEIGHT_DONE;
            }
        }
        float calW_kg = configManager.getExtraParamFloat(key_Kalibirierungsgewicht);
        if (calW_kg <= 0.0f) {
            ui.showMessage("Kal.-Gew. fehlt", "im Webformular", 2000);
            ui.setUiPage(UI::UiPage::LIVE);
        } else {
            const float calW_g = calW_kg * 1000.0f;
            calibrationState = meineWaage.kalibriereWaage(calibrationState, calW_g);

            if (calibrationState == CalibrationState::FINISHED) {
                KalibrierungsDaten neu = meineWaage.getKalibrierungsdaten();
                setExtraFloat(key_Kalibirierungfaktor, neu.kalibrierungsfaktor);
                setExtraLong(key_offset, neu.tareOffset);
                setExtraBool(key_kalibriert, true);
                configManager.saveConfig();
                ui.showMessage("Kalibrierung", "beendet", 1200);
                ui.setUiPage(UI::UiPage::LIVE);
                calibrationState = CalibrationState::IDLE;
            }
        }
    } else {
        calibrationState = CalibrationState::IDLE;
    }

    if (press == ButtonPressType::LONG) {
        if (ui.getUiPage() == UI::UiPage::TARE) {
            meineWaage.tare();
            long newOffset = meineWaage.getTareOffset();
            setExtraLong(key_offset, newOffset);
            setExtraBool(key_kalibriert, true);
            configManager.saveConfig();
        }
    }

    if (press == ButtonPressType::VERY_LONG) {
        if (ui.getUiPage() == UI::UiPage::RESET) {
            factoryResetAndReboot();
        }
    }
}
