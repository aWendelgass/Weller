#include "Waage.h"
#include <math.h>

// Optional: Parameter für Ausgabe/Update
static const int   UPDATE_INTERVAL_MS       = 500;
static const float OUTPUT_TOLERANCE_PERCENT = 5.0f;
static const float EMA_ALPHA                = 0.3f; // Glättung (0..1)

Waage::Waage(int doutPin, int sckPin)
: _loadCell(doutPin, sckPin),
  _lastWeight(0.0f),
  _emaWeight(0.0f),
  _emaInit(false),
  _hasLastOutput(false)
{}

void Waage::begin(const KalibrierungsDaten& daten) {
  _daten = daten;

  _loadCell.begin();
  _loadCell.setSamplesInUse(16);
  _loadCell.start(2000); // Vorwärmzeit

  if (_daten.istKalibriert) {
    _loadCell.setCalFactor(_daten.kalibrierungsfaktor);
    _loadCell.setTareOffset(_daten.tareOffset);
    Serial.println(F("Waage mit geladenen Daten initialisiert."));
  } else {
    Serial.println(F("Waage unkalibriert. Benötigt Kalibrierung."));
  }

  // Reset Tracking
  _hasLastOutput  = false;
  _lastWeight     = 0.0f;
  _emaInit        = false;
}


bool waitmessagesent=false;
void Waage::loop() {

  static unsigned long lastUpdate = 0;
  _loadCell.update();

  const unsigned long now = millis();
  if (now - lastUpdate >= UPDATE_INTERVAL_MS) {
    if (_daten.istKalibriert) {
      float gewicht_g = _loadCell.getData();

      // Plausibilitätscheck / Fehlerbehandlung
      if (isnan(gewicht_g) || gewicht_g < -100.0f) {
        Serial.println(F("Fehlerhafte Messung erkannt. Sensor wird neu gestartet."));
        _loadCell.start(2000);
        lastUpdate = now;
        return;
      }

      // Glättung (EMA)
      if (!_emaInit) { _emaWeight = gewicht_g; _emaInit = true; }
      else { _emaWeight = EMA_ALPHA * gewicht_g + (1.0f - EMA_ALPHA) * _emaWeight; }

      // Signifikanz-Logik: Prozentänderung ODER absolute Schwelle
      const float absDelta_g    = fabsf(_emaWeight - _lastWeight);
      const float pctChange     = (_hasLastOutput && fabsf(_lastWeight) > 0.0f)
                                ? (absDelta_g / fabsf(_lastWeight)) * 100.0f
                                : 0.0f;
      const float absThreshold_g = 1.0f; // 1g

      bool significant = false;
      if (!_hasLastOutput) {
        significant = true; // erste Ausgabe
      } else if (fabsf(_lastWeight) > 0.0f) {
        significant = (pctChange >= OUTPUT_TOLERANCE_PERCENT) || (absDelta_g >= absThreshold_g);
      } else {
        significant = (absDelta_g >= absThreshold_g);
      }

      if (significant) {
        _lastWeight    = _emaWeight;
        _hasLastOutput = true;

        Serial.print(F("Gewicht: "));
        Serial.print(roundf(_emaWeight));
        Serial.println(F(" g"));
      }
    } else {
      if (!waitmessagesent){
        Serial.println(F("Waage nicht kalibriert. Warte auf Kalibrierung."));
        waitmessagesent=true;
      }
    }
    lastUpdate = now;
  }
}

void Waage::tare() {
  Serial.println(F("Stabilisiere vor Tare..."));
  _loadCell.update();
  delay(200);
  _loadCell.update();

  _loadCell.tare();
  _hasLastOutput  = false; // nächste Ausgabe wieder zulassen
  _lastWeight     = 0.0f;
  _emaInit        = false;
  Serial.println(F("Tare durchgeführt."));
}

void Waage::refreshDataSet() {
    _loadCell.refreshDataSet();
}

float Waage::getNewCalibration(float known_mass) {
    return _loadCell.getNewCalibration(known_mass);
}

void Waage::setKalibrierungsfaktor(float factor) {
    _daten.kalibrierungsfaktor = factor;
    _loadCell.setCalFactor(factor);
}

void Waage::setTareOffset(long offset) {
    _daten.tareOffset = offset;
    _loadCell.setTareOffset(offset);
}

void Waage::setIstKalibriert(bool isCalibrated) {
    _daten.istKalibriert = isCalibrated;
}

float Waage::getGewicht() {
  return _emaWeight;
}

float Waage::getKalibrierungsfaktor() { return _daten.kalibrierungsfaktor; }
long  Waage::getTareOffset() { return _loadCell.getTareOffset(); }
bool  Waage::istKalibriert() { return _daten.istKalibriert; }
