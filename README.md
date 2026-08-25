# Atari 2600 CX30+ Wireless Mod for MiSTer FPGA / Superstation One

A non-destructive wireless mod for the **Atari 2600 CX30+** paddle controller (the modern remake by Atari). This project converts the original wired paddle into a low-latency wireless controller that pairs seamlessly with **MiSTer FPGA**, **Superstation One**, or any USB-compatible retro platform using dedicated ESP-NOW receivers.

The mod requires no permanent alterations to the original top enclosure or components. You can revert the controller back to its factory stock condition at any time.

---

## Key Features

* **3D Printed Bottom Enclosure:** Custom bottom shell designed to hold a Li-Pol battery and a Seeed Studio XIAO ESP32-C3 board with a dedicated USB-C port cutout for easy charging.
* **Full MiSTer FPGA Compatibility:** Enables independent 2-player paddle setups via two separate USB receivers. Uses low-latency Native USB HID (Daemonbite mirror profile).
* **Built-in Web Diagnostic Page:** Each receiver hosts a diagnostic interface (`http://atari-rx.local`) for easy pairing status verification, ADC raw value monitoring, and USB report rate diagnostics.
* **Advanced Power Management:** Automatic Deep Sleep mode on inactivity to conserve battery, with instant wake-up on button press.

---

## Repository Structure

    ├── 3d_models/
    │   ├── paddle.bottom.fcstd          # Source FreeCAD project file for the bottom shell
    │   ├── paddle.bottom.stl            # 3D printable bottom shell
    │   └── paddle.bottom.blocker.stl    # Locking insert to prevent ESP32 USB-C movement
    ├── receiver/
    │   ├── esp32_s3_odbiornik.ino       # Main Arduino sketch for the receiver
    │   ├── hid_descriptor.h             # Native USB HID descriptor definitions
    │   └── web_page.h                   # Embedded HTML/JS web panel interface
    └── transmitter/
        └── esp32_c3_nadajnik.ino        # Main Arduino sketch for the transmitter (paddle)

---

## Bill of Materials (BOM)

* **1x Seeed Studio XIAO ESP32-C3** (Transmitter) – *Note: Includes integrated Li-Pol battery charging circuit.*
* **1x Seeed Studio XIAO ESP32-S3** (Receiver)
* **1x Li-Pol Battery Akyga 1200mAh 1S 3.7V** (JST-BEC connector + socket, dimensions: 40 x 34 x 8 mm)
* 24 AWG / 28 AWG flexible hook-up wires
* 3D printing filament (e.g., Bambu Lab PLA Matte Black)

> **Important Notes:**
> 1. The 3D model was specifically designed to fit the exact dimensions of the Akyga 1200mAh battery and the Seeed Studio XIAO footprint.
> 2. The Seeed Studio XIAO ESP32-C3 fully supports onboard 3.7V Li-Pol battery charging via its USB-C port.

---

## Atari 2600 CX30+ Preparation

1. Unscrew the 2 screws located on the bottom of the original CX30+ paddle enclosure.
2. Carefully desolder the 2 wires connected to the potentiometer.
3. Desolder the 2 wires connected to the Fire button switch.
4. Set aside the original bottom shell and the wired cable assembly. 

*To restore the controller to its factory state in the future, simply re-attach the original cable, resolder the 4 wires back to the potentiometer and Fire switch, and screw the original bottom shell back on.*

---

## Assembly & Installation

### 1. 3D Printing

* **paddle.bottom.stl**: Print at **0.16mm layer height** with **20% infill**. Enable supports **only for the USB-C port cutout**. *(Tested on a Bambu Lab A1 mini using Bambu PLA Matte Black).*
* **paddle.bottom.blocker.stl**: Print without supports. This insert locks the ESP32 board firmly in place, preventing it from sliding backwards when plugging in a USB-C cable.

### 2. Wiring the Transmitter (ESP32-C3)

Solder 5 flexible wires (~10 cm long) from the original top enclosure to the XIAO ESP32-C3 board:

* **Potentiometer:**
  * **Middle pin** -> **D0** (Paddle 1) or **D1** (Paddle 2) on ESP32-C3
  * **Left pin** -> **3.3V**
  * **Right pin** -> **GND**
* **Fire Button:**
  * One contact -> **D1** (Paddle 1) or **D2** (Paddle 2)
  * Other contact -> **GND**
* **Battery Connection:**
  * Solder the Li-Pol battery leads/connector directly to the battery pads on the underside of the XIAO ESP32-C3 *(double-check polarity before soldering!)*.

### 3. Transmitter Final Assembly

1. Press the Li-Pol battery into its dedicated slot in the printed bottom shell, utilizing the built-in cable routing channels.
2. Gently push the XIAO ESP32-C3 board into its socket. The fit is intentionally tight so the board won't move when plugging in a USB-C cable. Route the battery and Fire button wires neatly toward their respective paths.
3. Once the USB-C connector aligns with the external port opening, connect the 2.4GHz external IPEX antenna to the board and rotate the connector slightly to lay flat.
4. Slide the 3D-printed **blocker insert** (`paddle.bottom.blocker.stl`) behind the ESP32 board to lock it securely in place.
5. Secure the flat antenna inside the bottom shell using its adhesive backing.
6. Neatly route all remaining wires to prevent them from getting pinched or interfering with the mechanical action of the Fire button.
7. Reinstall the Fire button mechanism, align the top and bottom shell halves, and fasten them together using the original screws.

### 4. Receiver Assembly (ESP32-S3)

1. Print a standalone enclosure for the Seeed Studio XIAO ESP32-S3 receiver. We recommend using this community model: [Printables - Seeed XIAO ESP32-S3 Case](https://www.printables.com/model/1275829-seeed-xiao-esp32-s3-case), which includes integrated button actuators for flashing.
2. Mount the ESP32-S3 inside the case, ensuring the external Wi-Fi/Bluetooth antenna is connected and routed outside or positioned properly.

---

## Flashing Firmware

Flashing is done via the **Arduino IDE**.

### Required Board Support Packages
In Arduino IDE, go to `Preferences` -> `Additional Boards Manager URLs` and add:

    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

Go to **Boards Manager** and install **esp32 by Espressif Systems**. Select:
* **Transmitter:** XIAO_ESP32C3
* **Receiver:** XIAO_ESP32S3

### Required Libraries
Install the following libraries via the Arduino Library Manager:
* **ResponsiveAnalogRead** by Damien Clarke (used in transmitter for ADC filtering)
* **Preferences** (built-in Espressif library for NVS storage)
* **WiFi, esp_now, ESPmDNS, WebServer** (built-in Espressif ESP32 core libraries)

---

## First Startup & Operation

### Auto-Pairing & Handshake (P3P)
1. Plug the Receiver (ESP32-S3) into a USB port on your MiSTer FPGA or PC.
2. Power on or wake up the Transmitter (Paddle).
3. The transmitter broadcasts an initial pairing request (`PAIR_REQ`). Upon receiving it, the receiver binds to the paddle's MAC address, saves it to non-volatile storage (NVS), and sends back an acknowledgment (`PAIR_ACK`).
4. Once paired, transmission switches to low-latency **Unicast**. The transmitter and receiver will remain permanently locked to each other across reboot and deep sleep cycles.

### Resetting Pairing
* **Via Web Panel:** Connect your phone/PC to the receiver's Wi-Fi network (`MiSTer-RX-XXXX`), open `http://atari-rx.local`, and click **Reset Permanent Pairing**.
* **Via Hardware (Transmitter):** Hold the **Fire button** for **5 seconds** while powering on or waking the paddle to clear its saved receiver MAC address.

### Web Diagnostic Interface
Connect to the receiver's SoftAP Wi-Fi network (`SSID: MiSTer-RX-XXXX`, `Password: atari1234`) and navigate to `http://atari-rx.local` in any web browser to view:
* Real-time ADC raw input values and Fire button states.
* Active USB HID output rate (Hz).
* Pairing status and bound MAC addresses.
* CPU frequency control (80MHz / 160MHz / 240MHz).
* Sleep timer countdown monitor.
