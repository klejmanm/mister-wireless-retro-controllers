# Wireless Retro Controllers for MiSTer FPGA (CX30 Paddle & Spinner Mod)

A non-destructive wireless modification for classic Atari 2600 CX30 Paddles and retro spinners, designed specifically for **MiSTer FPGA**. 

This project utilizes **ESP-NOW** for ultra-low latency (~15ms) wireless communication, bypassing the inherent lag of standard Bluetooth modes while retaining native BLE HID support for alternative setups.

---

## 🚀 Key Features

* **Non-Destructive Mod:** Custom 3D-printed replacement bottom shell for Atari CX30 paddles to house a battery and microcontroller without altering the original vintage hardware.
* **Ultra-Low Latency (ESP-NOW):** Direct peer-to-peer wireless protocol operating on a fixed Wi-Fi channel (Channel 6) with zero IP stack overhead.
* **Native MiSTer FPGA Compatibility:** Uses a custom USB-C receiver dongle acting as a **Daemonbite-compatible USB HID device** (`MiSTer-S1 Spinner`).
* **Dynamic Multi-Device Binding:** Automatically routes connected transmitters into dynamic slots (**Paddle 1**, **Paddle 2**, or **Spinner**).
* **Built-in Web Diagnostic UI:** Receiver hosts a local Access Point (`MiSTer-Retro-RX`) with an embedded web dashboard (`http://atari-rx.local`) for live monitoring, connection rates (Hz), CPU clock tuning, and slot resetting.
* **Dual-Mode Transmitters:** Supports both ESP-NOW (for gaming) and Bluetooth BLE HID (Daemonbite mirror profile).
* **Power Efficient:** Intelligent deep-sleep management with motion and button wakeup triggers.

---

## 🛠️ Hardware Requirements

1. **Receiver Dongle:** 
   * [Seeed Studio XIAO ESP32-S3](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32-S3-p-5627.html)
2. **Transmitter (Paddle):** 
   * [Seeed Studio XIAO ESP32-C3](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32-C3-p-5431.html)
   * LiPo Battery & charging circuit
   * Original Atari CX30 Paddle controller (or compatible clone)
3. **3D Printed Part:** 
   * Custom replacement bottom shell (files available in `3d_models/`).

---

## 📂 Repository Structure

* `receiver/` – Firmware for the XIAO ESP32-S3 USB receiver dongle (split into modular header files).
* `transmitter/` – Firmware for the XIAO ESP32-C3 paddle controller (supports 12-bit ADC filtering via `ResponsiveAnalogRead`).
* `3d_models/` – Information and STL notes regarding the non-destructive 3D-printable bottom shell.

---

## ⚙️ Quick Start & Installation

### 1. Receiver (XIAO ESP32-S3)
1. Install [Arduino IDE](https://www.arduino.cc/) with the ESP32 board package.
2. Ensure you have the `ResponsiveAnalogRead` and `NimBLE-Arduino` libraries installed (for transmitters).
3. Open the `receiver/` folder as an Arduino project, select board `XIAO_ESP32S3` (USB CDC On Boot: Enabled), and flash.
4. Plug the receiver into your MiSTer FPGA USB port. It will expose itself as `MiSTer-S1 Spinner`.

### 2. Transmitter (XIAO ESP32-C3)
1. Open `transmitter/esp32_c3_nadajnik_paddle.ino`.
2. Select board `XIAO_ESP32-C3`.
3. Flash the firmware to your paddle controller.

---

## 🌐 Receiver Web Dashboard

When the receiver is powered via MiSTer FPGA, it broadcasts a Wi-Fi Access Point:
* **SSID:** `MiSTer-Retro-RX`
* **Password:** `atari1234`
* **Web UI:** Navigate to `http://atari-rx.local` or `http://192.168.4.1` in your browser to check packet delivery rates, active slots, and adjust the receiver's CPU clock frequency (80MHz / 160MHz / 240MHz).

---

## 🤝 Acknowledgments & Disclaimer
Firmware developed with assistance from AI coding tools. This is a personal hobbyist project created for retro-gaming enthusiasts. Feel free to fork, modify, and improve!
