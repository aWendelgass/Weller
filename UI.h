#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Bounce2.h>

// Enum for button press types
enum class ButtonPressType {
  NONE,
  SHORT,
  LONG_2S,
  LONG_5S,
  LONG_10S
};

class UI {
public:
  UI(int buttonPin, int ledPin);
  void begin(const char* version);
  void handleUpdates(bool isWifiConnected);
  ButtonPressType getButtonPress();
  bool isHeld();
  unsigned long getHoldDuration();

  // Drawing methods
  void drawLivePage(float weight, bool isCalibrated, bool isWifiConnected, int rssi);
  void drawTarePage();
  void drawCalibratePage();
  void drawInfoPage(long tareOffset, float calFactor, String ip, bool isMqttConnected);
  void drawResetPage();
  void showMessage(const char* line1, const char* line2, int delayMs = 0);
  void showMessage(const char* line1, const char* line2, const char* line3, int delayMs=0);
  void clear();

  void drawCheckmark();
  
  // LED methods
  void blinkLed(int count, int delayMs);
  void setLed(bool on);

private:
  void initOLED(const char* version);
  void splash(const char* version);
  String formatKgComma(float kg, uint8_t decimals);
  void drawWeightValue(float kg, int16_t x, int16_t baselineY);

  int _buttonPin;
  int _ledPin;

  Adafruit_SSD1306 _display;
  U8G2_FOR_ADAFRUIT_GFX _u8g2;
  Bounce _debouncer;

  bool _oledAvailable;
  uint8_t _oledAddr;
};

#endif // UI_H
