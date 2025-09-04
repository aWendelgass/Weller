Ich habe mir kürzlich eine Weller WE1010 Lötstation zugelegt. Die erste Station habe ich direkt zurückgeschickt, da ich einen Fehler vermutete: Während des Lötens wechselte sie plötzlich in den Standby-Modus.

Doch auch das Austauschgerät zeigte dasselbe Verhalten – und ich musste feststellen, dass es sich nicht um einen Fehler, sondern um ein vorgesehenes Feature handelt: Die Lötstation geht nach einer fest definierten Zeit gnadenlos in den Standby, unabhängig davon, ob sie gerade in Gebrauch gerade ist. Das macht den an sich sinnvollen Standby Modus in meinen Augen unbrauchbar.
Meine Lösung:
Der Lötkolbenhalter steht auf einer schmucken, 3D-gedruckten Waage. Diese erkennt, ob der Lötkolben gerade verwendet wird (also nicht in der Halterung steckt). Sobald das der Fall ist, verhindert sie den Standby-Modus, indem sie die Lötstation für 100 Millisekunden kurz ausschaltet und sofort wieder einschaltet. Dadurch startet der interne Standby-Timer neu – und die Station bleibt aktiv.

English:
I recently bought a Weller WE1010 soldering station. I sent the first station straight back as I suspected a fault: during soldering, it suddenly switched to standby mode.

However, the replacement unit also exhibited the same behaviour - and I discovered that this was not a fault, but an intended feature: The soldering station mercilessly goes into standby after a predefined time, regardless of whether it is currently in use. In my opinion, this makes the standby mode, which is actually useful, unusable.
My solution:
The soldering iron holder stands on a smart, 3D-printed scale. This recognises whether the soldering iron is currently in use (i.e. not in the holder). As soon as this is the case, it prevents standby mode by switching the soldering station off briefly for 100 milliseconds and switching it on again immediately. This restarts the internal standby timer - and the station remains active.


## Features

- **Weight Sensing:** Measures weight using an HX711 load cell amplifier and a standard load cell.
- **OLED Display:** Shows the current weight, calibration status, and an interactive menu on a 128x64 SSD1306 OLED display.
- **Web-Based Configuration:** If not configured, the device starts a WiFi Access Point (AP). A web page allows you to set up WiFi, MQTT credentials, and other parameters.
- **MQTT Integration:** Publishes the salt weight and device status to an MQTT broker, making it easy to integrate with platforms like Home Assistant, openHAB, or Node-RED.
- **On-Device Controls:** A single button is used to navigate a simple menu for taring the scale, starting the calibration process, and viewing device information.
- **Persistent Storage:** All configuration is saved to the ESP32's non-volatile storage and is retained after a reboot or power loss.
- **mDNS Discovery:** The device can be discovered on the local network via its mDNS hostname (e.g., `myESP.local`).
- ** Power Control of the WELLER 1010 station

## Hardware Requirements

- ESP32 Development Board
- HX711 Load Cell Amplifier
- Load Cell (e.g., 50kg capacity, depending on your salt container)
- SSD1306 I2C OLED Display (128x64 pixels)
- Tactile Push Button
- 5V Power Supply
- 5 Volt Relais
- A LED for status indication.
- 3D Prints from Printables.com
- Various small electrical parts (sockets, cable, etc...)

### Default Pinout

The firmware is configured for the following pin connections. This can be changed in `SaltMonitor.ino`.

| Component      | Pin Name     | ESP32 Pin |
|----------------|--------------|-----------|
| HX711          | `DOUT`       | 25        |
| HX711          | `SCK`        | 27        |
| OLED Display   | `SDA`        | 21        |
| OLED Display   | `SCL`        | 22        |
| Control Button | `BUTTON_PIN` | 13        |
| Status LED     | `LED_PIN`    | 17        |


## Software & Libraries

This project is built using the Arduino framework for the ESP32. You will need to install the following libraries through the Arduino Library Manager:

- `HX711_ADC` by olkal
- `Adafruit GFX Library` by Adafruit
- `Adafruit SSD1306` by Adafruit
- `U8g2_for_Adafruit_GFX` by olikraus
- `ESPAsyncWebServer`
- `AsyncTCP`
- `PubSubClient` by Nick O'Leary
- `Bounce2.h`

## Initial Setup & Configuration

