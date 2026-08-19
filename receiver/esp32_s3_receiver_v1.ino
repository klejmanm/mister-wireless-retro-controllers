/*
 * Wireless Retro Controller Receiver - Dynamic ESP-NOW HOST (XIAO ESP32-S3)
 * Target Hardware: Seeed Studio XIAO ESP32-S3
 * USB HID Profile: Native Daemonbite Mirror Descriptor ("MiSTer-S1 Spinner")
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_event.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#include "hid_descriptor.h"
#include "web_page.h"

// ==============================================================================
// CONFIGURATION & CONSTANTS
// ==============================================================================
#define WIFI_CHANNEL           6                    // Stały kanał Wi-Fi dla SoftAP i ESP-NOW
#define AP_SSID                "MiSTer-Retro-RX"    // Nazwa sieci Wi-Fi SoftAP
#define AP_PASS                "atari1234"          // Hasło do sieci Wi-Fi SoftAP
#define HTTP_PORT              80                   // Port serwera diagnostycznego WWW

#define USB_PRODUCT_NAME       "MiSTer-S1 Spinner"  // Nazwa urządzenia HID wymagana przez MiSTer FPGA
#define USB_MANUFACTURER_NAME  "Daemonbite"         // Nazwa producenta USB

#define DEVICE_TIMEOUT_MS      3000                 // Czas (ms) bez pakietu, po którym nadajnik jest OFFLINE
#define STATS_CALC_INTERVAL_MS 1000                 // Okno czasowe (ms) do obliczania Hz (USB Output Rate)

#define DEV_TYPE_PADDLE        0
#define DEV_TYPE_SPINNER       1

#define SLOT_PADDLE_1          0
#define SLOT_PADDLE_2          1
#define SLOT_SPINNER           2

// ==============================================================================
// 1. STANY SYSTEMU & GLOBALNE ZMIENNE
// ==============================================================================
volatile bool usbMounted = false;
volatile uint32_t usbReportCounterWindow = 0;
volatile uint16_t usbReportsPerSecond = 0;
unsigned long lastHzCalculationTime = 0;

uint32_t currentCpuFreq = 240; // Domyślny, nadpisywany z NVS
Preferences prefs;

USBHID HID;
DaemonbiteHIDDevice USBDeviceHID;
WebServer server(HTTP_PORT);

struct DeviceState {
  bool bound;
  uint8_t mac[6];
  uint16_t rawValue;
  uint8_t buttons;
  unsigned long lastSeen;
  uint32_t packetCount;
};

volatile DeviceState slots[3] = {
  {false, {0}, 0, 0, 0, 0}, 
  {false, {0}, 0, 0, 0, 0}, 
  {false, {0}, 0, 0, 0, 0}  
};

uint8_t currentHidButtons = 0;
uint8_t currentHidPaddle = 128;

// ==============================================================================
// 2. POMOCNICZE FUNKCJE WEWNĘTRZNE
// ==============================================================================
bool matchMac(const uint8_t* mac1, const uint8_t* mac2) {
  return memcmp(mac1, mac2, 6) == 0;
}

String macToString(const uint8_t* mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", 
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

static void usbEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == ARDUINO_USB_EVENTS) {
    switch (event_id) {
      case ARDUINO_USB_STARTED_EVENT:
      case ARDUINO_USB_RESUME_EVENT:
        usbMounted = true;
        break;
      case ARDUINO_USB_STOPPED_EVENT:
      case ARDUINO_USB_SUSPEND_EVENT:
        usbMounted = false;
        break;
      default:
        break;
    }
  }
}

// ==============================================================================
// 3. HANDLERY SERWERA HTTP
// ==============================================================================
void handleRoot() {
  server.send(200, "text/html", HTML_DIAGNOSTIC);
}

void handleJSON() {
  unsigned long now = millis();
  String json = "{";
  json += "\"host_mac\":\"" + WiFi.softAPmacAddress() + "\",";
  json += "\"cpu_freq\":" + String(currentCpuFreq) + ",";
  json += "\"usb_mounted\":" + String(usbMounted ? "true" : "false") + ",";
  json += "\"usb_hz\":" + String(usbReportsPerSecond) + ",";
  json += "\"slots\":[";
  for (int i = 0; i < 3; i++) {
    bool online = slots[i].bound && ((now - slots[i].lastSeen) < DEVICE_TIMEOUT_MS);
    json += "{";
    json += "\"slot\":" + String(i) + ",";
    json += "\"bound\":" + String(slots[i].bound ? "true" : "false") + ",";
    json += "\"mac\":\"" + macToString((const uint8_t*)slots[i].mac) + "\",";
    json += "\"online\":" + String(online ? "true" : "false") + ",";
    json += "\"value\":" + String(slots[i].rawValue) + ",";
    json += "\"buttons\":" + String(slots[i].buttons) + ",";
    json += "\"packets\":" + String(slots[i].packetCount);
    json += "}";
    if (i < 2) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleReset() {
  for (int i = 0; i < 3; i++) {
    slots[i].bound = false;
    memset((void*)slots[i].mac, 0, 6);
    slots[i].packetCount = 0;
  }
  server.send(200, "text/plain", "OK");
}

void handleSetFreq() {
  if (server.hasArg("val")) {
    uint32_t freq = server.arg("val").toInt();
    if (freq == 240 || freq == 160 || freq == 80) {
      prefs.putUInt("cpu_freq", freq);
    }
  }
  server.send(200, "text/plain", "OK");
  delay(200);
  ESP.restart(); // Miękki restart, by zastosować nowy zegar
}

// ==============================================================================
// 4. OBSŁUGA ODBIORU PAKETÓW ESP-NOW
// ==============================================================================
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_message)) return;

  struct_message msg;
  memcpy(&msg, incomingData, sizeof(msg));

  const uint8_t* srcMac = recv_info->src_addr;
  int assignedSlot = -1;

  for (int i = 0; i < 3; i++) {
    if (slots[i].bound && matchMac((const uint8_t*)slots[i].mac, srcMac)) {
      assignedSlot = i;
      break;
    }
  }

  if (assignedSlot == -1) {
    if (msg.deviceType == DEV_TYPE_PADDLE) {
      if (!slots[SLOT_PADDLE_1].bound) assignedSlot = SLOT_PADDLE_1;
      else if (!slots[SLOT_PADDLE_2].bound) assignedSlot = SLOT_PADDLE_2;
    } else if (msg.deviceType == DEV_TYPE_SPINNER) {
      if (!slots[SLOT_SPINNER].bound) assignedSlot = SLOT_SPINNER;
    }

    if (assignedSlot != -1) {
      slots[assignedSlot].bound = true;
      memcpy((void*)slots[assignedSlot].mac, srcMac, 6);
    } else {
      return; 
    }
  }

  slots[assignedSlot].rawValue = msg.value;
  slots[assignedSlot].buttons = msg.buttons;
  slots[assignedSlot].lastSeen = millis();
  slots[assignedSlot].packetCount++;

  int8_t spinnerDelta = 0;

  if (assignedSlot == SLOT_PADDLE_1) {
    currentHidPaddle = map(constrain(msg.value, 0, 4095), 0, 4095, 0, 255);
    if (msg.buttons & 0x01) currentHidButtons |= 0x01;
    else currentHidButtons &= ~0x01;

  } else if (assignedSlot == SLOT_PADDLE_2) {
    currentHidPaddle = map(constrain(msg.value, 0, 4095), 0, 4095, 0, 255);
    if (msg.buttons & 0x01) currentHidButtons |= 0x02;
    else currentHidButtons &= ~0x02;

  } else if (assignedSlot == SLOT_SPINNER) {
    spinnerDelta = (int8_t)constrain(msg.value, -128, 127);
    if (msg.buttons & 0x01) currentHidButtons |= 0x04;
    else currentHidButtons &= ~0x04;
  }

  USBDeviceHID.sendReport(currentHidButtons, spinnerDelta, currentHidPaddle);
}

// ==============================================================================
// 5. SETUP & MAIN LOOP
// ==============================================================================
void setup() {
  // Odczyt zapisanego taktowania z NVS (domyślnie 240 MHz)
  prefs.begin("sys_conf", false);
  currentCpuFreq = prefs.getUInt("cpu_freq", 240);
  if (currentCpuFreq != 240 && currentCpuFreq != 160 && currentCpuFreq != 80) currentCpuFreq = 240;
  
  setCpuFrequencyMhz(currentCpuFreq);

  // 1. Inicjalizacja SoftAP
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(AP_SSID, AP_PASS, WIFI_CHANNEL, 0);

  // Inicjalizacja mDNS (dostęp pod http://atari-rx.local)
  if (MDNS.begin("atari-rx")) {
    MDNS.addService("http", "tcp", HTTP_PORT);
  }

  // 2. Start ESP-NOW
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memset(peerInfo.peer_addr, 0xFF, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  // 3. Start Serwera Diagnostycznego
  server.on("/", handleRoot);
  server.on("/json", handleJSON);
  server.on("/reset", handleReset);
  server.on("/setfreq", handleSetFreq);
  server.begin();

  // 4. Rejestracja eventów USB + Start Stosu USB HID
  USB.onEvent(usbEventCallback);
  USB.productName(USB_PRODUCT_NAME);
  USB.manufacturerName(USB_MANUFACTURER_NAME);
  HID.begin();
  USB.begin();

  lastHzCalculationTime = millis();
}

void loop() {
  server.handleClient();

  // Przelicznik częstotliwości USB Output Rate (Hz)
  unsigned long now = millis();
  if (now - lastHzCalculationTime >= STATS_CALC_INTERVAL_MS) {
    usbReportsPerSecond = usbReportCounterWindow;
    usbReportCounterWindow = 0;
    lastHzCalculationTime = now;
  }

  delay(2);
}
