#include "UI.h"
#include <Wire.h>
#include <WiFi.h> // For WiFi.localIP()

// Pin definitions from Weller.ino
#define OLED_SDA 21
#define OLED_SCL 22

// Screen dimensions from Weller.ino
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// Button press timings from Weller.ino
const uint16_t SHORT_PRESS_MAX_MS = 500;
const uint16_t TARE_PRESS_MS      = 2000;
const uint16_t CAL_PRESS_MS       = 5000;
const uint16_t RESET_PRESS_MS     = 10000;
const uint32_t MENU_TIMEOUT_MS    = 8000;

UI::UI(int buttonPin, int ledPin) :
  _buttonPin(buttonPin),
  _ledPin(ledPin),
  _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
  _oledAvailable(false),
  _oledAddr(0x3C),
  _buttonPressed(false),
  _buttonPressStart(0),
  _uiPage(UiPage::LIVE),
  _lastInteractionMs(0)
{
}

void UI::begin(const char* version) {
  pinMode(_buttonPin, INPUT_PULLUP);
  pinMode(_ledPin, OUTPUT);
  digitalWrite(_ledPin, HIGH); // Turn LED ON at boot

  initOLED(version);
}

void UI::update(float weight, bool isCalibrated, bool isWifiConnected, int rssi, long tareOffset, float calFactor, String ip, bool isMqttConnected) {
    static unsigned long lastOled = 0;
    unsigned long now = millis();

    if (now - lastOled >= 500) {
        if (_oledAvailable) {
            if (_uiPage == UiPage::LIVE) {
                drawLive(weight, isCalibrated, isWifiConnected, rssi);
            } else {
                drawMenu(tareOffset, calFactor, ip, isMqttConnected);
            }
        }
        lastOled = now;
    }

    handleButton();
    handleLedStatus(isWifiConnected);

    if (_uiPage != UiPage::LIVE && (now - _lastInteractionMs >= MENU_TIMEOUT_MS)) {
        _uiPage = UiPage::LIVE;
    }
}


void UI::showMessage(const char* line1, const char* line2, int delayMs) {
    if(!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf);  _u8g2.setCursor(0,12); if(line1) _u8g2.print(line1);
    _u8g2.setFont(u8g2_font_helvR14_tf); _u8g2.setCursor(0,36); if(line2) _u8g2.print(line2);
    _display.display();
    if (delayMs > 0) {
        delay(delayMs);
    }
}

ButtonPressType UI::getButtonPress() {
    unsigned long pressMs = millis() - _buttonPressStart;
    if (pressMs >= RESET_PRESS_MS) {
        return ButtonPressType::VERY_LONG;
    }
    if (pressMs >= CAL_PRESS_MS) {
        if (_uiPage == UiPage::CALIBRATION) return ButtonPressType::LONG;
    }
    if (pressMs >= TARE_PRESS_MS) {
        if (_uiPage == UiPage::TARE) return ButtonPressType::LONG;
    }
    if (pressMs < SHORT_PRESS_MAX_MS) {
        return ButtonPressType::SHORT;
    }
    return ButtonPressType::NONE;
}

void UI::handleButton() {
    unsigned long now = millis();
    if (digitalRead(_buttonPin) == LOW && !_buttonPressed) {
        _buttonPressed = true;
        _buttonPressStart = now;
    }

    if (_buttonPressed && digitalRead(_buttonPin) == HIGH) {
        _buttonPressed = false;
        ButtonPressType pressType = getButtonPress();
        if (pressType == ButtonPressType::SHORT) {
            _lastInteractionMs = now;
            if (_uiPage == UiPage::LIVE) {
                _uiPage = UiPage::TARE;
            } else {
                switch(_uiPage) {
                    case UiPage::TARE: _uiPage = UiPage::CALIBRATION; break;
                    case UiPage::CALIBRATION: _uiPage = UiPage::INFO; break;
                    case UiPage::INFO: _uiPage = UiPage::RESET; break;
                    case UiPage::RESET: _uiPage = UiPage::LIVE; break;
                    default: _uiPage = UiPage::LIVE; break;
                }
            }
        }
    }
}


void UI::blinkLed(int count, int delayMs) {
  for (int i = 0; i < count; ++i) {
    digitalWrite(_ledPin, HIGH);
    delay(delayMs);
    digitalWrite(_ledPin, LOW);
    delay(delayMs);
  }
}

void UI::setLed(bool on) {
    digitalWrite(_ledPin, on ? HIGH : LOW);
}

void UI::handleLedStatus(bool isWifiConnected) {
    static unsigned long previousMillis = 0;
    static bool ledState = LOW;
    const long interval = 500; // Blink interval 500ms -> 1Hz

    if (_uiPage == UiPage::CALIBRATION) {
        return;
    }

    if (isWifiConnected) {
        if (ledState == LOW) {
            digitalWrite(_ledPin, HIGH);
            ledState = HIGH;
        }
    } else {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;
            ledState = !ledState;
            digitalWrite(_ledPin, ledState);
        }
    }
}

static bool i2cPresent(uint8_t addr){ Wire.beginTransmission(addr); return (Wire.endTransmission()==0); }

void UI::initOLED(const char* version) {
  Wire.begin(OLED_SDA, OLED_SCL); delay(50);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  if(i2cPresent(0x3C)) _oledAddr=0x3C; else if(i2cPresent(0x3D)) _oledAddr=0x3D; else{
    Serial.println(F("Kein Display angeschlossen."));
    _oledAvailable=false; return; }
  if(!_display.begin(SSD1306_SWITCHCAPVCC, _oledAddr)){
    Serial.println(F("Kein Display angeschlossen."));
    _oledAvailable=false; return; }
  _u8g2.begin(_display); _u8g2.setFontMode(1); _u8g2.setFontDirection(0); _u8g2.setForegroundColor(SSD1306_WHITE);
  _display.clearDisplay(); _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,12); _u8g2.print(F("Waage gestartet")); _display.display();
  _oledAvailable=true;
  splash(version);
}

void UI::splash(const char* version){
  if(!_oledAvailable) return;
  _display.clearDisplay();
  _u8g2.setFont(u8g2_font_7x14B_tf); _u8g2.setCursor(0,18); _u8g2.print(F("Weller Kontroller"));
  _u8g2.setFont(u8g2_font_6x13_tf);  _u8g2.setCursor(0,38); _u8g2.print(F("Smart Standby"));
  _u8g2.print(version);
  _display.display();
  delay(1200);
}

String UI::formatKgComma(float kg, uint8_t decimals){
  char buf[24];
  dtostrf(kg, 0, decimals, buf);
  String s(buf);
  s.replace('.', ',');
  return s;
}

void UI::drawWeightValue(float kg, int16_t x, int16_t baselineY){
  _u8g2.setFont(u8g2_font_logisoso16_tn);
  _u8g2.setCursor(x, baselineY);
  _u8g2.print(formatKgComma(kg, 2));
  _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.print(F(" kg"));
}

void UI::drawLive(float weight, bool isCalibrated, bool isWifiConnected, int rssi) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,12); _u8g2.print(isCalibrated ? F("Waage OK") : F("NICHT KAL."));
    drawWeightValue(weight, 0, 36);
    if (isWifiConnected) {
        String rssiStr = "RSSI: " + String(rssi) + " dBm";
        _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,62); _u8g2.print(rssiStr);
    } else {
        _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,62); _u8g2.print(F("WiFi nicht verbunden"));
    }
    _display.display();
}

void UI::drawMenu(long tareOffset, float calFactor, String ip, bool isMqttConnected) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,12);
    switch(_uiPage){
        case UiPage::TARE:
            _u8g2.setCursor(0,28); _u8g2.print(F("> Tare"));
            _u8g2.setFont(u8g2_font_helvR14_tf); _u8g2.setCursor(0,52); _u8g2.print(F("2 s halten"));
            break;
        case UiPage::CALIBRATION:
            _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,28); _u8g2.print(F("< Kalibrieren"));
            _u8g2.setFont(u8g2_font_helvR14_tf); _u8g2.setCursor(0,52); _u8g2.print(F("5 s halten"));
            break;
        case UiPage::INFO: {
            _u8g2.setFont(u8g2_font_6x12_tf);
            _u8g2.setCursor(0,20); _u8g2.print(F("CalF: "));  _u8g2.print(calFactor, 4);
            _u8g2.setCursor(0,32); _u8g2.print(F("Offset: ")); _u8g2.print(tareOffset);
            _u8g2.setCursor(0,44); _u8g2.print(F("IP: "));     _u8g2.print(ip);
            _u8g2.setCursor(0,56); _u8g2.print(F("MQTT: "));   _u8g2.print(isMqttConnected ? F("verbunden") : F("NICHT"));
            }
            break;
        case UiPage::RESET:
            _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,28); _u8g2.print(F("NVS löschen"));
            _u8g2.setFont(u8g2_font_helvR14_tf); _u8g2.setCursor(0,52); _u8g2.print(F("10 s halten"));
            break;
        default: break;
    }
    _display.display();
}
