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
  _oledAddr(0x3C),
  _clickState(IDLE),
  _click_timer(0)
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
    // This implementation uses a small state machine to detect single and double clicks.
    // It introduces a small latency for single clicks but is robust.
    const int DOUBLE_CLICK_WINDOW_MS = 300;

    // First, check for long presses on release, as they are unambiguous.
    if (_debouncer.rose()) {
        unsigned long duration = _debouncer.previousDuration();
        if (duration >= 2000 && duration < 5000) return ButtonPressType::LONG_2S;
        if (duration >= 5000 && duration < 10000) return ButtonPressType::LONG_5S;
        if (duration >= 10000) return ButtonPressType::LONG_10S;
    }

    // Next, handle the click state machine based on the press edge.
    if (_debouncer.fell()) {
        if (_clickState == IDLE) {
            _clickState = AWAITING_SECOND_CLICK;
            _click_timer = millis();
        } else if (_clickState == AWAITING_SECOND_CLICK) {
            if (millis() - _click_timer < DOUBLE_CLICK_WINDOW_MS) {
                _clickState = IDLE;
                return ButtonPressType::DOUBLE_CLICK;
            }
        }
    }
    
    // Finally, if the time window for a double click has passed, register a single click.
    if (_clickState == AWAITING_SECOND_CLICK && millis() - _click_timer > DOUBLE_CLICK_WINDOW_MS) {
        _clickState = IDLE;
        return ButtonPressType::SHORT;
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
  _u8g2.setFont(u8g2_font_7x14B_tf); _u8g2.setCursor(0,18); _u8g2.print(F("Weller Kontroller"));
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
    _u8g2.print(F("Ready 2 go"));
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

void UI::displaySetupMain(int menuIndex) {
    if (!_oledAvailable) return;
    const char* items[] = {"Weller Standby Time", "Tara", "Kalibrierung", "Info", "Waage", "Werkseinstellung", "Exit"};
    const int numItems = sizeof(items) / sizeof(items[0]);
    const int displayItems = 4; // Max items to show at once

    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_helvB12_tf);
    _u8g2.setCursor(0, 14);
    _u8g2.print(F("Setup"));
    
    _u8g2.setFont(u8g2_font_6x13_tf);

    int start = 0;
    if (menuIndex >= displayItems) {
        start = menuIndex - displayItems + 1;
    }

    for (int i = 0; i < displayItems; i++) {
        int currentItemIndex = start + i;
        if (currentItemIndex >= numItems) break;

        int y = 30 + i * 10;
        if (currentItemIndex == menuIndex) {
            // Use background color to create a highlight bar
            _u8g2.setFontMode(0); // Opaque font mode
            _u8g2.setBackgroundColor(WHITE);
            _u8g2.setForegroundColor(BLACK);
            _u8g2.setCursor(2, y);
            // A small hack to draw a full bar by printing spaces
            _u8g2.print("                    "); 
            _u8g2.setCursor(2, y);
            _u8g2.print(items[currentItemIndex]);
            // Reset to default
            _u8g2.setFontMode(1); // Transparent
            _u8g2.setBackgroundColor(BLACK);
            _u8g2.setForegroundColor(WHITE);
        } else {
            _u8g2.setCursor(2, y);
            _u8g2.print(items[currentItemIndex]);
        }
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
    sprintf(buf, "> %d < Minuten", newStandbyTime);
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

void UI::drawTarePage() {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,28); _u8g2.print(F("> Tare"));
    _u8g2.setFont(u8g2_font_helvR14_tf); _u8g2.setCursor(0,52); _u8g2.print(F("2 s halten"));
    _display.display();
}

void UI::drawCalibratePage() {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,28); _u8g2.print(F("< Kalibrieren"));
    _u8g2.setFont(u8g2_font_helvR14_tf); _u8g2.setCursor(0,52); _u8g2.print(F("5 s halten"));
    _display.display();
}

void UI::drawResetPage() {
    if (!_oledAvailable) return;
    _display.clearDisplay();
    _u8g2.setFont(u8g2_font_6x13_tf); _u8g2.setCursor(0,28); _u8g2.print(F("NVS löschen"));
    _u8g2.setFont(u8g2_font_helvR14_tf); _u8g2.setCursor(0,52); _u8g2.print(F("10 s halten"));
    _display.display();
}
