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
  LONG_1_5S,
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
  void displayReady(unsigned long standbyTime);
  void displayActive(unsigned long operationTime, unsigned long standbyTime);
  void displayInactive(unsigned long standbyTime);
  void displayStandby(unsigned long standbyTime);
  void displaySetupMain(int menuIndex);
  void displaySetupStandbyTime(int newStandbyTime);
  void displayWeighing(float weight);
  void drawTarePage();
  void drawCalibratePage();
  void drawInfoPage(long tareOffset, float calFactor, String ip, bool isMqttConnected);
  void drawResetPage();
  void displayConfirmation(const char* message);
  void showMessage(const char* line1, const char* line2, int delayMs = 0);
  void showMessage(const char* line1, const char* line2, const char* line3, int delayMs=0);
  void clear();

  void drawCheckmark();
  
  // LED methods
  void blinkLed(int count, int delayMs);
  void setLed(bool on);
  void setStandby(bool standby);

private:
  void initOLED(const char* version);
  void splash(const char* version);
  String formatTime(unsigned long timeSeconds);

  int _buttonPin;
  int _ledPin;
  bool _in_standby = false;
  bool _led_state = false;

  Adafruit_SSD1306 _display;
  U8G2_FOR_ADAFRUIT_GFX _u8g2;
  Bounce _debouncer;

  bool _oledAvailable;
  uint8_t _oledAddr;
};

#endif // UI_H
