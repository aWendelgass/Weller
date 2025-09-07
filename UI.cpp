#include "UI.h"
#include <Wire.h>
#include <WiFi.h> // For WiFi.localIP()

// Pin definitions from Weller.ino
#define OLED_SDA 21
#define OLED_SCL 22

// Screen dimensions from Weller.ino
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// Button press timings
const uint16_t DEBOUNCE_MS        = 30;

UI::UI(int buttonPin, int ledPin) :
  _buttonPin(buttonPin),
  _ledPin(ledPin),
  _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1),
  _debouncer(),
  _oledAvailable(false),
  _oledAddr(0x3C)
{
}

void UI::begin(const char* version) {
  pinMode(_buttonPin, INPUT_PULLUP);
  pinMode(_ledPin, OUTPUT);
  digitalWrite(_ledPin, HIGH); // Turn LED ON at boot

  _debouncer.attach(_buttonPin, INPUT_PULLUP);
  _debouncer.interval(DEBOUNCE_MS);

  initOLED(version);
}

void UI::handleUpdates(bool isWifiConnected) {
    _debouncer.update();

    // LED Status Logic (can be expanded if needed)
    static unsigned long previousMillis = 0;
    static bool ledState = LOW;
    const long interval = 500;

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

void UI::showMessage(const char* line1, const char* line2, const char* line3, int delayMs) {
    if(!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf);  _u8g2.setCursor(0,12); if(line1) _u8g2.print(line1);
    _u8g2.setFont(u8g2_font_helvR14_tf); _u8g2.setCursor(0,36); if(line2) _u8g2.print(line2);
    _u8g2.setFont(u8g2_font_6x13_tf);  _u8g2.setCursor(0,56); if(line3) _u8g2.print(line3);

    _display.display();
    if (delayMs > 0) {
        delay(delayMs);
    }
}

void UI::clear() {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _display.display();
}

ButtonPressType UI::getButtonPress() {
    if (_debouncer.rose()) {
        unsigned long duration = _debouncer.previousDuration();
        if (duration < 500) {
            return ButtonPressType::SHORT;
        } else if (duration >= 1000 && duration < 5000) {  //AW Mher als 1 Sekund ist lang
            return ButtonPressType::LONG_1_5S;
        } else if (duration >= 5000 && duration < 10000) {
            return ButtonPressType::LONG_5S;
        } else if (duration >= 10000) {
            return ButtonPressType::LONG_10S;
        }
    }
    return ButtonPressType::NONE;
}

bool UI::isHeld() {
    return _debouncer.read() == LOW;
}

unsigned long UI::getHoldDuration() {
    return _debouncer.duration();
}

void UI::drawCheckmark() {
    if (!_oledAvailable) return;
    _u8g2.setFont(u8g2_font_unifont_t_symbols);
    _u8g2.drawGlyph(118, 62, 0x2713); // Draw ✓ at bottom right
    _display.display();
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
  _u8g2.setFont(u8g2_font_7x14B_tf); _u8g2.setCursor(0,18); _u8g2.print(F("Weller Controller"));
  _u8g2.setFont(u8g2_font_6x13_tf);  _u8g2.setCursor(0,38); _u8g2.print(F("Smart Standby"));
  _u8g2.setCursor(0,58); _u8g2.print(version);
  _display.display();
  delay(1200);
}

String UI::formatTime(unsigned long timeSeconds) {
    char buf[6];
    sprintf(buf, "%02lu:%02lu", (timeSeconds % 3600) / 60, timeSeconds % 60);
    return String(buf);
}

void UI::displayReady(unsigned long standbyTime) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_helvB18_tf);
    _u8g2.setCursor(0, 24);
    _u8g2.print(F("Bereit"));
    _u8g2.setFont(u8g2_font_6x13_tf);
    _u8g2.setCursor(0, 52);
    _u8g2.print("Standby in: " + formatTime(standbyTime));
    _display.display();
}

void UI::displayActive(unsigned long operationTime, unsigned long standbyTime) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_helvB18_tf);
    _u8g2.setCursor(0, 24);
    _u8g2.print(F("Aktiv"));
    _u8g2.setFont(u8g2_font_6x13_tf);
    _u8g2.setCursor(0, 42);
    _u8g2.print("Aktiv seit: " + formatTime(operationTime));
    _u8g2.setCursor(0, 56);
    _u8g2.print("Refresh in: " + formatTime(standbyTime));
    _display.display();
}

void UI::displayInactive(unsigned long standbyTime) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_helvB18_tf);
    _u8g2.setCursor(0, 24);
    _u8g2.print(F("Standby in"));
    _u8g2.setFont(u8g2_font_logisoso24_tn);
    _u8g2.setCursor(0, 56);
    _u8g2.print(formatTime(standbyTime));
    _display.display();
}

void UI::displayStandby(unsigned long standbyTime) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_helvB18_tf);
    _u8g2.setCursor(0, 24);
    _u8g2.print(F("Standby"));
    _u8g2.setFont(u8g2_font_6x13_tf);
    _u8g2.setCursor(0, 52);
    _u8g2.print("seit: " + formatTime(standbyTime));
    _display.display();
}

void UI::displaySetupMain(int menuIndex) {
    if (!_oledAvailable) return;
    const char* items[] = {"Standby Time", "Tara", "Kalibrierung", "Info", "Waage", "Werkseinstellung", "Exit"};
    const int numItems = sizeof(items) / sizeof(items[0]);
    const int displayItems = 4;

    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_helvB12_tf);
    _u8g2.setCursor(0, 14);
    _u8g2.print(F("Setup"));
    
    _u8g2.setFont(u8g2_font_7x14_tf);

    int start = 0;
    if (menuIndex >= displayItems) {
        start = menuIndex - (displayItems - 1);
    }
    if (start > numItems - displayItems) {
        start = numItems - displayItems;
    }


    for (int i = 0; i < displayItems; i++) {
        int currentItemIndex = start + i;
        if (currentItemIndex >= numItems) break;

        int y = 28 + i * 12;
        if (currentItemIndex == menuIndex) {
            _u8g2.setCursor(0, y);
            _u8g2.print(">");
        }
        _u8g2.setCursor(8, y);
        _u8g2.print(items[currentItemIndex]);
    }
    _display.display();
}

void UI::displayConfirmation(const char* message) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_helvB12_tf);
    _u8g2.setCursor(0, 24);
    _u8g2.print(message);

    _u8g2.setFont(u8g2_font_6x13_tf);
    _u8g2.setCursor(0, 52);
    _u8g2.print(F("2s halten: Ja"));
    _display.display();
}

void UI::displaySetupStandbyTime(int newStandbyTime) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_helvB18_tf);
    _u8g2.setCursor(0, 24);
    _u8g2.print(F("Weller Standby"));

    char buf[20];
    sprintf(buf, "%d Minuten", newStandbyTime);
    _u8g2.setFont(u8g2_font_helvR14_tf);
    _u8g2.setCursor(0, 52);
    _u8g2.print(buf);
    
    _display.display();
}

void UI::displayWeighing(float weight) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf);
    _u8g2.setCursor(0, 12);
    _u8g2.print("Gewicht");

    char buf[16];
    dtostrf(weight, 0, 0, buf);
    _u8g2.setFont(u8g2_font_logisoso24_tn);
    _u8g2.setCursor(0, 48);
    _u8g2.print(buf);
    _u8g2.print(" g");

    _display.display();
}

void UI::drawInfoPage(long tareOffset, float calFactor, String ip, bool isMqttConnected) {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x12_tf);
    _u8g2.setCursor(0,20); _u8g2.print(F("CalF: "));  _u8g2.print(calFactor, 4);
    _u8g2.setCursor(0,32); _u8g2.print(F("Offset: ")); _u8g2.print(tareOffset);
    _u8g2.setCursor(0,44); _u8g2.print(F("IP: "));     _u8g2.print(ip);
    _u8g2.setCursor(0,56); _u8g2.print(F("MQTT: "));   _u8g2.print(isMqttConnected ? F("verbunden") : F("NICHT"));
    _display.display();
}

void UI::drawTarePage() {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf);
    _u8g2.setCursor(0, 28);
    _u8g2.print(F("Tare ausfuehren?"));
    _u8g2.setFont(u8g2_font_helvR14_tf);
    _u8g2.setCursor(0,52);
    _u8g2.print(F("2s halten"));
    _display.display();
}

void UI::drawCalibratePage() {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf);
    _u8g2.setCursor(0, 28);
    _u8g2.print(F("Kalibrierung?"));
    _u8g2.setFont(u8g2_font_helvR14_tf);
    _u8g2.setCursor(0,52);
    _u8g2.print(F("2s halten"));
    _u8g2.setCursor(0,62);
    _u8g2.print(F("Canel -> kurz"));
    _display.display();
}

void UI::drawResetPage() {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_helvR14_tf);
    _u8g2.setCursor(0,25);
    _u8g2.print(F("D E L E T E ?"));

    _u8g2.setFont(u8g2_font_6x13_tf);
    _u8g2.setCursor(0,43);
    _u8g2.print(F("2 Sek. --> DELETE!"));
    _u8g2.setCursor(0,58);
    _u8g2.print(F("Kurz   --> Abbruch"));

    _display.display();
}
