#ifndef WAAGE_H
#define WAAGE_H

#include <Arduino.h>
#include <HX711_ADC.h>

// Struktur für Kalibrierdaten
struct KalibrierungsDaten {
  float kalibrierungsfaktor; // CalFactor der HX711_ADC-Bibliothek
  long  tareOffset;          // Tare-Offset der HX711_ADC-Bibliothek
  bool  istKalibriert;       // Flag, ob gültige Daten vorliegen
};

class Waage {
public:
  Waage(int doutPin, int sckPin);

  // Start mit übergebenen Kalibrierdaten
  void begin(const KalibrierungsDaten& daten);

  // zyklisch aufrufen
  void loop();

  // Anzeigeeinstellungen: Genauigkeit in Gramm (z. B. 100 -> 1 Nachkommastelle, 10 -> 2, 1 -> 3)
  void setAnzeigeGenauigkeitGramm(uint16_t genauigkeit_g);

  // Bedienfunktionen & Kalibrierungs-Helfer
  void tare();
  void refreshDataSet();
  float getNewCalibration(float known_mass);
  void setKalibrierungsfaktor(float factor);
  void setTareOffset(long offset);
  void setIstKalibriert(bool isCalibrated);


  // Getter
  float getGewicht();     // in der Kalibriereinheit (hier: Gramm)
  float getGewichtKg();   // in Kilogramm
  float getKalibrierungsfaktor();
  long  getTareOffset();
  bool  istKalibriert();
  
private:
  HX711_ADC          _loadCell;
  float              _lastWeight;            // letzte Rohmessung (in g) – ungeglättet
  float              _emaWeight;             // geglätteter Wert (in g)
  bool               _emaInit;               // EMA initialisiert
  float              _lastPrintedKg;         // letzte ausgegebene, GERUNDETE kg
  bool               _hasLastPrinted;        // schon etwas ausgegeben
  bool               _hasLastOutput;         // Alt: verhindert Fluten
  KalibrierungsDaten _daten;

  // Anzeigeformatierung
  uint16_t _anzeigeGenauigkeit_g;            // z. B. 100 g, 10 g, 1 g
  uint8_t  _anzeigeDezimalstellen;           // abgeleitet aus Genauigkeit

  uint8_t _berechneDezimalstellen(uint16_t genauigkeit_g);
};

#endif
