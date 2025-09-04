constexpr const char* VERSION= "Version 0.05b";

//Changelog:
// 02.09: V0.01a waage.cpp fehlende Kalibrierung wird nur einmal über den seriellen Monitor ausgegeben
//        V0.01b Software Titel Anzeige angepasst
//        V0.02  OLED Display auf 100khz und Tiemout 50
//        V0.03  Waage.cpp angepasst
//        V0.03a Menü Zeile aus Menü entfernt
// 03.09  V0.04a Neue Button Press Mechanik übersichtlicher zu lesen
//        V0.05b Kajibirieunganzeige und NAckommastellen

#include <Arduino.h>
#include "Waage.h"
#include "WifiConfigManager.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <WiFi.h>                  // für WiFi.localIP()
#include <U8g2_for_Adafruit_GFX.h>
#include <Bounce2.h>



#define WAAGE_DEBUG 1   // 0=aus, 1=Seriell-Diagnose

// ------------------------------
// Pins & OLED
// ------------------------------
const int HX711_DOUT = 25;
const int HX711_SCK  = 27;
const int BUTTON_PIN = 13;
const int LED_PIN    = 17;
#define OLED_SDA 21
#define OLED_SCL 22

// ------------------------------
// Display
// ------------------------------
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
U8G2_FOR_ADAFRUIT_GFX u8g2;   // U8g2-Textengine auf Adafruit_GFX

bool    oledAvailable        = false;
bool    oledAnnouncedMissing = false;
uint8_t oledAddr             = 0x3C; // 0x3C/0x3D

// ------------------------------
// WifiConfigManager – Strukturen
// ------------------------------

ConfigStruc config = {
  "Weller Controller", // title
  "",             // ssid
  "",             // ssidpasswd
  "WellerESP",        // mdns (nur Default; zur Laufzeit immer Getter nutzen!)
  "",             // mqttip (nur Default)
  1883,            // mqttport (nur Default)
  "",             // mqttUser (nur Default)
  "",             // mqttPasswd (nur Default)
  false            // configured
};


#define key_Kalibirierungsgewicht "calWeight"   // g im Webformular
#define key_Kalibirierungfaktor   "calFactor"
#define key_offset                "offset"
#define key_kalibriert            "calibrated"
#define key_akkusticalarm         "alarm"

ExtraStruc extraParams[] = {
    //                                 nur einer der Werte TEXTvalue bis                       LONGvalue ist gültig abhängig von FormType
    //key                                       FormType   TEXTvalue   FLOATvaue  BOOLvalue    LONGvalue  optional   inputParam
  {   key_Kalibirierungsgewicht ,               FLOAT,     "",          0.41,     false,       -1,        false,      true       },  // 1
  {   key_Kalibirierungfaktor,                  FLOAT,     "",          1.0,      false,       -1,        false,      false      },  // 2
  {   key_offset,                               LONG,      "",         -1.0,      false,        0,        false,      false      },  // 3
  {   key_kalibriert,                           BOOL,      "",         -1.0,      false,       -1,        false,      false      },  // 4
  {   key_akkusticalarm,                        BOOL,      "",         -1.0,      false,        -1,       false,      true       }   // 5
};

constexpr size_t ANZ_EXTRA_PARAMS = sizeof(extraParams) / sizeof(extraParams[0]);

const WebStruc webForm[] = {
  // LineType    Label                        relatedKey
  { TITLE,       "Weller Controller",         ""                        },
  { CONFIGBLOCK, "",                          ""                        },
  { BLANK,       "",                          ""                        },
  { SEPARATOR,   "",                          ""                        },
  { BLANK,       "",                          ""                        },
  { PARAMETER,   "Kalibrierungsgewicht (kg)", key_Kalibirierungsgewicht },
  { PARAMETER,   "Kalibrierungsfaktor",       key_Kalibirierungfaktor   },
  { PARAMETER,   "Waagen-Offset",             key_offset                },
  { PARAMETER,   "Waage kalibriert",          key_kalibriert            },
  { BLANK,       "",                          ""                        },
  { PARAMETER,   "Akkustischer Alarm",        key_akkusticalarm         },
  { BLANK,       "",                          ""                        }

};

constexpr size_t ANZ_WEBFORM_ITEMS = sizeof(webForm) / sizeof(webForm[0]);


WifiConfigManager configManager(&config, extraParams, webForm, ANZ_WEBFORM_ITEMS, ANZ_EXTRA_PARAMS, VERSION);

// ------------------------------
// Waage & Taster
// ------------------------------
Waage meineWaage(HX711_DOUT, HX711_SCK, BUTTON_PIN, LED_PIN);
const uint16_t SHORT_PRESS_MAX_MS = 500;   // < 0.5 s: Menü
const uint16_t TARE_PRESS_MS      = 2000;  // 2–5 s: Tare
const uint16_t CAL_PRESS_MS       = 5000;  // 5–10 s: Kalibrieren
const uint16_t RESET_PRESS_MS     = 10000; // >=10 s : Werkseinstellung
const uint32_t MENU_TIMEOUT_MS    = 8000;  // 8 s Inaktivität
const uint16_t DEBOUNCE_MS        = 30;

static bool buttonPressed = false;
static unsigned long buttonPressStart = 0;
static bool lastRaw = HIGH;
static bool stableState = HIGH;
static unsigned long lastChange = 0;



// ------------------------------
// Menü-Logik
// ------------------------------
enum class UiPage : uint8_t { LIVE=0, TARE, CALIBRATION, INFO, RESET };
static UiPage       uiPage             = UiPage::LIVE;
static unsigned long lastInteractionMs = 0;

// ------------------------------
// Helpers: Basistopic aus Manager-Getter ableiten
// ------------------------------
static String getBaseTopic(){
  String base = configManager.getMdnsName(); // Getter!
  if (base.length() == 0) base = F("waage");
  return base;
}

// ------------------------------
// OLED Utils
// ------------------------------
static bool i2cPresent(uint8_t addr){ Wire.beginTransmission(addr); return (Wire.endTransmission()==0); }

static void splash(){ if(!oledAvailable) return; display.clearDisplay();
  u8g2.setFont(u8g2_font_7x14B_tf); u8g2.setCursor(0,18); u8g2.print(F("Weller Kontroller"));
  u8g2.setFont(u8g2_font_6x13_tf);  u8g2.setCursor(0,38); u8g2.print(F("Smart Standby"));
u8g2.print(VERSION);
  display.display(); delay(1200);
}

static void initOLED(){
  Wire.begin(OLED_SDA, OLED_SCL); delay(50);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  if(i2cPresent(0x3C)) oledAddr=0x3C; else if(i2cPresent(0x3D)) oledAddr=0x3D; else{
    if(!oledAnnouncedMissing){ Serial.println(F("Kein Display angeschlossen.")); oledAnnouncedMissing=true; }
    oledAvailable=false; return; }
  if(!display.begin(SSD1306_SWITCHCAPVCC, oledAddr)){
    if(!oledAnnouncedMissing){ Serial.println(F("Kein Display angeschlossen.")); oledAnnouncedMissing=true; }
    oledAvailable=false; return; }
  u8g2.begin(display); u8g2.setFontMode(1); u8g2.setFontDirection(0); u8g2.setForegroundColor(SSD1306_WHITE);
  display.clearDisplay(); u8g2.setFont(u8g2_font_6x13_tf); u8g2.setCursor(0,12); u8g2.print(F("Waage gestartet")); display.display();
  oledAvailable=true; splash();
}

// Hilfsfunktion: Float -> String mit Komma als Dezimaltrenner (nur Anzeige)
static String formatKgComma(float kg, uint8_t decimals){
  char buf[24]; dtostrf(kg, 0, decimals, buf); // erzeugt mit Punkt
  String s(buf);
  s.replace('.', ',');
  return s;
}

static void drawWeightValue(float kg, int16_t x, int16_t baselineY){
  // Anzeige mit Komma
  u8g2.setFont(u8g2_font_logisoso16_tn);
  u8g2.setCursor(x, baselineY);
  u8g2.print(formatKgComma(kg, 2)); // 2 Nachkommastelle mit Komma
  u8g2.setFont(u8g2_font_6x13_tf); u8g2.print(F(" kg"));
}

static void drawTwoLine(const char* l1, const char* l2){ if(!oledAvailable) return; display.clearDisplay();
  u8g2.setFont(u8g2_font_6x13_tf);  u8g2.setCursor(0,12); if(l1) u8g2.print(l1);
  u8g2.setFont(u8g2_font_helvR14_tf); u8g2.setCursor(0,36); if(l2) u8g2.print(l2);
  display.display();
}

static void drawLive(){ if(!oledAvailable) return; display.clearDisplay();
  u8g2.setFont(u8g2_font_6x13_tf); u8g2.setCursor(0,12); u8g2.print(meineWaage.istKalibriert()?F("Waage OK"):F("NICHT KAL."));
  drawWeightValue(meineWaage.getGewichtKg(), 0, 36);
  if (configManager.isWifiConnected()) {
    String rssiStr = "RSSI: " + String(configManager.getRSSI()) + " dBm";
    u8g2.setFont(u8g2_font_6x13_tf); u8g2.setCursor(0,62); u8g2.print(rssiStr);
  } else {
    u8g2.setFont(u8g2_font_6x13_tf); u8g2.setCursor(0,62); u8g2.print(F("WiFi nicht verbunden"));
  }
  display.display();
}

static void drawMenu(){ if(!oledAvailable) return; display.clearDisplay();
  //u8g2.setFont(u8g2_font_6x13_tf); u8g2.setCursor(0,12); u8g2.print(F("MENÜ (kurz=weiter)"));
  u8g2.setFont(u8g2_font_6x13_tf); u8g2.setCursor(0,12); 
  switch(uiPage){
    case UiPage::TARE:
      u8g2.setCursor(0,28); u8g2.print(F("> Tare"));
      u8g2.setFont(u8g2_font_helvR14_tf); u8g2.setCursor(0,52); u8g2.print(F("2 s halten"));
      break;
    case UiPage::CALIBRATION:
      u8g2.setFont(u8g2_font_6x13_tf); u8g2.setCursor(0,28); u8g2.print(F("< Kalibrieren"));
      u8g2.setFont(u8g2_font_helvR14_tf); u8g2.setCursor(0,52); u8g2.print(F("5 s halten"));
      break;
    case UiPage::INFO: {
      u8g2.setFont(u8g2_font_6x12_tf);
      u8g2.setCursor(0,20); u8g2.print(F("CalF: "));  u8g2.print(meineWaage.getKalibrierungsfaktor(), 4);
      u8g2.setCursor(0,32); u8g2.print(F("Offset: ")); u8g2.print(meineWaage.getTareOffset());
      u8g2.setCursor(0,44); u8g2.print(F("IP: "));     u8g2.print(WiFi.localIP().toString());
      u8g2.setCursor(0,56); u8g2.print(F("MQTT: "));   u8g2.print(configManager.isMqttConnected()?F("verbunden"):F("NICHT"));
      }
    /* UiPage::INFO: {
      u8g2.setFont(u8g2_font_6x12_tf);
      u8g2.setCursor(0,20); u8g2.print(F("< Info"));
      u8g2.setCursor(0,32); u8g2.print(F("CalF: "));  u8g2.print(meineWaage.getKalibrierungsfaktor(), 4);
      u8g2.setCursor(0,44); u8g2.print(F("Offset: ")); u8g2.print(meineWaage.getTareOffset());
      u8g2.setCursor(0,56); u8g2.print(F("IP: "));     u8g2.print(WiFi.localIP().toString());
      u8g2.setCursor(0,68); u8g2.print(F("MQTT: "));   u8g2.print(configManager.isMqttConnected()?F("verbunden"):F("NICHT"));
      */
      break;
    case UiPage::RESET:
      u8g2.setFont(u8g2_font_6x13_tf); u8g2.setCursor(0,28); u8g2.print(F("NVS löschen"));
      u8g2.setFont(u8g2_font_helvR14_tf); u8g2.setCursor(0,52); u8g2.print(F("10 s halten"));
      break;
    default: break;
  }
  display.display();
}

static void updateOLED(){ if(!oledAvailable) return; if(uiPage==UiPage::LIVE) drawLive(); else drawMenu(); }
static void nextPage(){ lastInteractionMs=millis(); switch(uiPage){ case UiPage::LIVE: uiPage=UiPage::TARE; break; case UiPage::TARE: uiPage=UiPage::CALIBRATION; break; case UiPage::CALIBRATION: uiPage=UiPage::INFO; break; case UiPage::INFO: uiPage=UiPage::RESET; break; case UiPage::RESET: uiPage=UiPage::LIVE; break; } }


// ------------------------------
// ExtraParam Update & Factory Reset
// ------------------------------
static void setExtraFloat(const char* key, float v){ for(int i=0;i<ANZ_EXTRA_PARAMS;i++){ if(strcmp(extraParams[i].keyName,key)==0){ extraParams[i].FLOATvalue=v; return; } } }
static void setExtraLong (const char* key, long v){ for(int i=0;i<ANZ_EXTRA_PARAMS;i++){ if(strcmp(extraParams[i].keyName,key)==0){ extraParams[i].LONGvalue =v; return; } } }
static void setExtraBool (const char* key, bool v){ for(int i=0;i<ANZ_EXTRA_PARAMS;i++){ if(strcmp(extraParams[i].keyName,key)==0){ extraParams[i].BOOLvalue =v; return; } } }

static void factoryResetAndReboot(){ Preferences p; p.begin("network", false); p.clear(); p.end(); p.begin("operation", false); p.clear(); p.end(); delay(200); ESP.restart(); }

// ------------------------------
// MQTT – gewicht_kg mit Punkt, calibrated 1/0
// ------------------------------
static unsigned long lastMqttPub = 0;
static const unsigned long MQTT_PUB_MS = 5000; // alle 5 s

static void mqttPublishLoop(){
  if (!configManager.isWifiConnected()) return;

  configManager.ensureMqttConnected();
  if (!configManager.isMqttConnected()) return;

  unsigned long now = millis();
  if (now - lastMqttPub < MQTT_PUB_MS) return;
  lastMqttPub = now;

  String base = getBaseTopic();

  // gewicht_kg mit Punkt (numerisch gut weiterverarbeitbar)
  String tGew = base + F("/gewicht_kg");
  char buf[16];
  dtostrf(meineWaage.getGewichtKg(), 0, 1, buf); // 3.5 etc.
  configManager.publish(tGew.c_str(), String(buf), true, 0);

  // calibrated
  String tCal = base + F("/calibrated");
  configManager.publish(tCal.c_str(), meineWaage.istKalibriert()?"1":"0", true, 0);

  // rssi
  String tRssi = base + F("/rssi");
  configManager.publish(tRssi.c_str(), String(configManager.getRSSI()), true, 0);
}

// ------------------------------
// Status LED
// ------------------------------
static void handleLedStatus() {
  static unsigned long previousMillis = 0;
  static bool ledState = LOW;
  const long interval = 500; // Blink interval 500ms -> 1Hz

  // Do not interfere with calibration blinking, which is a blocking process
  if (uiPage == UiPage::CALIBRATION) {
    // The Waage class handles its own blinking with delay()
    return;
  }

  if (configManager.isWifiConnected()) {
    if (ledState == LOW) {
      digitalWrite(LED_PIN, HIGH);
      ledState = HIGH;
    }
  } else {
    // Blink LED if not connected
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  }
}


// ------------------------------
// Setup & Loop
// ------------------------------
void setup(){
  Serial.begin(115200); delay(300);
  Serial.println(VERSION);
  Serial.print("ANZ_EXTRA_PARAMS:  ");
  Serial.println(ANZ_EXTRA_PARAMS); 
  Serial.print("ANZ_WEBFORM_ITEMS:  ");
  Serial.println(ANZ_WEBFORM_ITEMS); 
  pinMode(BUTTON_PIN, INPUT_PULLUP); pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH); // Turn LED ON at boot

  // Manager starten; lädt auch gespeicherte Konfig
  configManager.begin("SmartScale");

  // OLED
  initOLED();

  // Kalibrierungsdaten aus Manager-Extras holen
  KalibrierungsDaten kd{};
  kd.kalibrierungsfaktor = configManager.getExtraParamFloat(key_Kalibirierungfaktor);
  kd.tareOffset          = configManager.getExtraParamInt  (key_offset);
  kd.istKalibriert       = configManager.getExtraParamBool (key_kalibriert);

#if WAAGE_DEBUG
  Serial.println(F("== Waage Setup =="));
  Serial.print(F("CalFactor init: ")); Serial.println(kd.kalibrierungsfaktor, 6);
  Serial.print(F("TareOffset init: ")); Serial.println(kd.tareOffset);
  Serial.print(F("IstKalibriert:  ")); Serial.println(kd.istKalibriert);
#endif

  meineWaage.begin(kd);
  meineWaage.setAnzeigeGenauigkeitGramm(100); // 100 g -> 1 Dezimalstelle in kg

  // UI-Callback (Lambda!)
  meineWaage.setUiCallback([](const char* l1, const char* l2){ drawTwoLine(l1, l2); });

  updateOLED();
}


void loop(){
  configManager.handleLoop();
  meineWaage.loop();
  mqttPublishLoop();
  handleLedStatus();

  static unsigned long lastOled=0; unsigned long now=millis();
  if(now-lastOled>=500){ updateOLED(); lastOled=now; }

  // Taster-Handling
  if(digitalRead(BUTTON_PIN)==LOW && !buttonPressed){ buttonPressed=true; buttonPressStart=now; }
  if(buttonPressed && digitalRead(BUTTON_PIN)==HIGH){
    unsigned long pressMs = now - buttonPressStart; buttonPressed=false;
    if(pressMs < SHORT_PRESS_MAX_MS)
      { lastInteractionMs=now; if(uiPage==UiPage::LIVE) uiPage=UiPage::TARE; else nextPage(); updateOLED(); }
    else 
      if(pressMs>=TARE_PRESS_MS && pressMs<CAL_PRESS_MS && uiPage==UiPage::TARE){
      meineWaage.tare(); long newOffset = meineWaage.getTareOffset();
      setExtraLong(key_offset, newOffset); setExtraBool(key_kalibriert, true); configManager.saveConfig(); updateOLED();
    }
    else if(pressMs>=CAL_PRESS_MS && pressMs<RESET_PRESS_MS){
      float calW_kg = configManager.getExtraParamFloat(key_Kalibirierungsgewicht);
      if(calW_kg<=0.0f){ drawTwoLine("Kal.-Gew. fehlt", "im Webformular"); }
      else{
        const float calW_g = calW_kg * 1000.0f; // Waage erwartet Gramm
        KalibrierungsDaten neu = meineWaage.kalibriereWaage(calW_g);
        setExtraFloat(key_Kalibirierungfaktor, neu.kalibrierungsfaktor);
        setExtraLong (key_offset,              neu.tareOffset);
        setExtraBool (key_kalibriert,          true);
        configManager.saveConfig();
        drawTwoLine("Kalibrierung", "beendet"); delay(1200); uiPage=UiPage::LIVE; updateOLED();
      }
    }
  }

  // Reset bei sehr langem Druck (sofort)
  if(buttonPressed){ unsigned long held=now-buttonPressStart; if(held>=RESET_PRESS_MS){ Serial.println(F("Werkseinstellung - Neustart")); factoryResetAndReboot(); } }

  // Menü-Timeout
  if(uiPage!=UiPage::LIVE && (now-lastInteractionMs>=MENU_TIMEOUT_MS)) uiPage=UiPage::LIVE;
}

