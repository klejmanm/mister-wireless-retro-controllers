# Wireless Retro Controllers for MiSTer FPGA (CX30 Paddle & Spinner Mod)

A non-destructive wireless modification for classic Atari 2600 CX30 Paddles and retro spinners, designed specifically for **MiSTer FPGA**. This project takes inspiration from and works wonderfully with modernized builds like the [Atari 2600 CX30+ Paddles](https://misterfpga.org/viewtopic.php?t=7149).

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

## 🔌 Wiring Scheme: Paddle (Atari CX30) to XIAO ESP32-C3

If you are wiring a classic Atari CX30 potentiometer and fire button to the Seeed Studio XIAO ESP32-C3 transmitter, use the following connection layout:

### 1. Potentiometer (CX30 Analog Dial)
The vintage potentiometer has 3 pins. Connect them as follows:
* **Pin 1 (Left / GND side):** Connected to **GND** on XIAO ESP32-C3.
* **Pin 2 (Middle / Wiper - Ślizgacz):** Connected to an Analog/ADC Pin, e.g., **A0 (GPIO 2)** on XIAO ESP32-C3.
* **Pin 3 (Right / 3.3V side):** Connected to **3.3V** on XIAO ESP32-C3.

> *Note: If turning the dial in-game decreases values when it should increase (or vice versa), you can simply swap the wires connected to Pin 1 and Pin 3.*

### 2. Fire Button
* **Terminal 1:** Connected to a Digital Pin configured with internal pull-up, e.g., **D1 (GPIO 3)** on XIAO ESP32-C3.
* **Terminal 2:** Connected to **GND**.

---

## 🛠️ Hardware Requirements

1. **Receiver Dongle:** 
   * [Seeed Studio XIAO ESP32-S3](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32-S3-p-5627.html)
2. **Transmitter (Paddle):** 
   * [Seeed Studio XIAO ESP32-C3](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32-C3-p-5431.html)
   * LiPo Battery & charging circuit
   * Original Atari CX30 Paddle controller (or compatible [CX30+ build](https://misterfpga.org/viewtopic.php?t=7149))
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
<img width="3000" height="4000" alt="PXL_20260818_215311284" src="https://github.com/user-attachments/assets/664d8246-c8e0-49ca-9c74-41843877ada2" />


## 🤝 Acknowledgments & Disclaimer
Firmware developed with assistance from AI coding tools. This is a personal hobbyist project created for retro-gaming enthusiasts. Feel free to fork, modify, and improve!
