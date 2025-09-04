#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>

// Enum for button press types
enum class ButtonPressType {
  NONE,
  SHORT,
  LONG,
  VERY_LONG
};

class UI {
public:
  UI(int buttonPin, int ledPin);
  void begin(const char* version);
  void update(float weight, bool isCalibrated, bool isWifiConnected, int rssi, long tareOffset, float calFactor, String ip, bool isMqttConnected);
  void showMessage(const char* line1, const char* line2, int delayMs = 0);
  ButtonPressType getButtonPress();
  void blinkLed(int count, int delayMs);
  void setLed(bool on);

  // This enum is now public to be accessible from Weller.ino
  enum class UiPage : uint8_t { LIVE=0, TARE, CALIBRATION, INFO, RESET };
  UiPage getUiPage() { return _uiPage; }
  void setUiPage(UiPage page) { _uiPage = page; }


private:
  void initOLED(const char* version);
  void drawLive(float weight, bool isCalibrated, bool isWifiConnected, int rssi);
  void drawMenu(long tareOffset, float calFactor, String ip, bool isMqttConnected);
  void handleButton();
  void handleLedStatus(bool isWifiConnected);
  void splash(const char* version);
  String formatKgComma(float kg, uint8_t decimals);
  void drawWeightValue(float kg, int16_t x, int16_t baselineY);

  int _buttonPin;
  int _ledPin;

  Adafruit_SSD1306 _display;
  U8G2_FOR_ADAFRUIT_GFX _u8g2;

  bool _oledAvailable;
  uint8_t _oledAddr;

  // Button handling
  bool _buttonPressed;
  unsigned long _buttonPressStart;

  // Menu state
  UiPage _uiPage;
  unsigned long _lastInteractionMs;
};

#endif // UI_H
