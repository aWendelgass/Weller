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

## State Machine
The controller's logic is based on a state machine. The following table describes the states, triggers, and actions.

### State Table: Weller Controller

| Current State | Trigger | Next State | Actions |
| :--- | :--- | :--- | :--- |
| **Operational States** | | | |
| *Any Operational State* | Button Hold > 5s | `SETUP_MAIN` | `stopOperationTimer()`, `stopStandbyTimer()` |
| `INIT` | Program Start (Normal) | `INACTIVE` | - |
| `INIT` | Program Start (WiFi AP Mode) | `SHOW_AP_INFO` | `configManager.startAP()` |
| `SHOW_AP_INFO` | Timer > 5s | `INACTIVE` | - |
| `READY` | Weight < -(Threshold) | `ACTIVE` | `startStandbyTimer()`, `startOperationTimer()` |
| `READY` | Standby Timer Expires | `STANDBY` | `stopStandbyTimer()` |
| `ACTIVE` | Weight > -(Threshold) | `INACTIVE` | `stopOperationTimer()` |
| `ACTIVE` | Standby Timer Expires | `ACTIVE` (re-entrant) | `restartStation()`, `startStandbyTimer()` |
| `INACTIVE` | Weight < -(Threshold) | `ACTIVE` | `startOperationTimer()` |
| `INACTIVE` | Standby Timer Expires | `STANDBY` | `stopStandbyTimer()` |
| `STANDBY` | Short Button Press | `READY` | `restartStation()`, `startStandbyTimer()` |
| `STANDBY` | Weight < -(Threshold) | `ACTIVE` | `restartStation()`, `startStandbyTimer()`, `startOperationTimer()` |
| **Setup States** | | | |
| `SETUP_MAIN` | Short Button Press | `SETUP_MAIN` (re-entrant) | Select next menu item |
| `SETUP_MAIN` | Long Press (depending on menu) | Various Menu States | Enters the selected menu |
| `SETUP_MAIN` | Long Press (on "Exit") | `INACTIVE` | `saveConfig()` (if changed), `restartStation()`, `startStandbyTimer()` |
| `SETUP_STANDBY_TIME`| Short Button Press | `SETUP_STANDBY_TIME` (re-entrant) | `setup_standby_time_minutes++` |
| `SETUP_STANDBY_TIME`| Long Press | `SETUP_MAIN` | Return to main menu |
| `MENU_TARE` | Short Press | `SETUP_MAIN` | Return to main menu |
| `MENU_TARE` | Long Press | `INACTIVE` | `meineWaage.tare()`, `ui.showMessage(...)` |
| `MENU_CALIBRATE` | Short Press | `SETUP_MAIN` | Return to main menu |
| `MENU_CALIBRATE` | Long Press | `CALIBRATION_CHECK_WEIGHT`| Start calibration process |
| `MENU_INFO` | Short Press | `SETUP_MAIN` | Return to main menu |
| `MENU_WIEGEN` | Short Press | `SETUP_MAIN` | Return to main menu |
| `MENU_RESET` | Short Press | `SETUP_MAIN` | Return to main menu |
| `MENU_RESET` | Long Press | `MENU_RESET_CONFIRM` | Show confirmation dialog |
| `MENU_RESET_CONFIRM`| Short Press | `SETUP_MAIN` | Return to main menu |
| `MENU_RESET_CONFIRM`| Long Press | - (Device reboots) | `factoryResetAndReboot()` |
| **Calibration Sub-states** | | | |
| `CALIBRATION_CHECK_WEIGHT` | `calWeight > 0` | `CALIBRATION_STEP_1_START`| - |
| `CALIBRATION_CHECK_WEIGHT` | `calWeight <= 0` | `INACTIVE` | `ui.showMessage(...)` |
| `CALIBRATION_STEP_1_START` | Short Press | `CALIBRATION_STEP_2_EMPTY` | `meineWaage.tare()` |
| `CALIBRATION_STEP_2_EMPTY` | Short Press | `CALIBRATION_DONE` | `meineWaage.refreshDataSet()`, `getNewCalibration(...)`, `saveConfig()` |
| `CALIBRATION_DONE` | Auto (after message) | `INACTIVE` | `ui.showMessage(...)`, `meineWaage.tare()` |

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

## How it Works
The core of the project is an ESP32 microcontroller that continuously monitors the weight of the soldering iron holder using an HX711 load cell. The logic is governed by a state machine (see the table above for details). When the soldering iron is lifted from the holder (detected by a significant decrease in weight), the controller considers the station "ACTIVE". It then prevents the Weller station from entering its automatic standby mode by briefly toggling a relay connected to the station's power. This power cycle is short enough not to interrupt the soldering iron's temperature but long enough to reset the Weller's internal standby timer. When the iron is placed back, the controller enters an "INACTIVE" state and allows the station's own timer to run, eventually entering standby to save power. All status information is visible on an OLED display, and the device can be configured via a web interface.

## Initial Setup & Configuration
1.  **Power On:** Connect the ESP32 to a 5V power supply.
2.  **Access Point (AP) Mode:** On its first boot, or if no WiFi credentials are saved, the device will create its own WiFi network. The network name (SSID) will be **`Weller-XXXX`** (where `XXXX` is derived from the device's MAC address).
3.  **Connect to the Device:** Use your computer or smartphone to connect to this `Weller-XXXX` WiFi network.
4.  **Open the Configuration Page:** Once connected, open a web browser and navigate to the IP address **`192.168.4.1`**. This will load the device's configuration portal.
5.  **Configure WiFi & MQTT:** On this page, you can enter the credentials (SSID and password) for your local WiFi network. You can also optionally provide connection details for an MQTT broker.
6.  **Save and Reboot:** Click "Daten übernehmen" (Save Data). The page will confirm that the settings have been saved. You must then **manually restart** the device (e.g., by pressing the reset button or power cycling it).
7.  **Connect to Network:** After rebooting, the device will automatically connect to the WiFi network you configured. You can find its new IP address from your router's client list or by accessing it via its mDNS name, which is `WellerESP.local` by default (this can also be changed in the web interface).

## Firmware Update (OTA)
The firmware can be updated wirelessly over-the-air (OTA).

1.  **Compile the Firmware:** Compile the new version of the firmware in your Arduino IDE or PlatformIO environment. This will generate a `.bin` file.
2.  **Access the Web Interface:** Ensure your computer is connected to the same network as the Weller Controller. Open a web browser and navigate to the device's IP address or its mDNS name (e.g., `http://WellerESP.local`).
3.  **Upload the Firmware:** Scroll down to the "Firmware Update (OTA)" section on the configuration page.
4.  **Select File:** Click the "Choose File" button and select the new firmware `.bin` file you compiled.
5.  **Start Update:** Click the "Update starten" (Start Update) button. The upload progress will be shown.
6.  **Reboot:** After the update completes successfully, you will see a confirmation alert. You must then **manually restart** the device to run the new firmware.
