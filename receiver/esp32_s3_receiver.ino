/*
 * Wireless Retro Controller Receiver - Single Host (XIAO ESP32-S3)
 * Handshake P3P + NVS + AutoReset Lock + Fixed CPU Freq Handler
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#include "hid_descriptor.h"
#include "web_page.h"

// ==============================================================================
#define WIFI_CHANNEL           6
#define AP_PASS                "atari1234"
#define HTTP_PORT              80

#define USB_PRODUCT_NAME       "MiSTer-S1 Spinner"
#define USB_MANUFACTURER_NAME  "Daemonbite"

#define DEVICE_TIMEOUT_MS      3000
#define STATS_CALC_INTERVAL_MS 1000
#define ESP_NOW_PACKET_SIZE    5

#define DEV_TYPE_PAIR_REQ      0xAA

volatile bool usbMounted = false;
volatile uint32_t usbReportCounterWindow = 0;
volatile uint16_t usbReportsPerSecond = 0;
unsigned long lastHzCalculationTime = 0;

uint32_t currentCpuFreq = 240;
Preferences prefs;

USBHID HID;
DaemonbiteHIDDevice USBDeviceHID;
WebServer server(HTTP_PORT);

bool isBound = false;
uint8_t boundMac[6] = {0};
uint16_t lastRawValue = 0;
uint8_t lastButtons = 0;
uint8_t lastSleepTimeoutSec = 0;
uint32_t packetCount = 0;
unsigned long lastSeenTime = 0;

String dynamicSSID = "";
String dynamicMDNS = "atari-rx";

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
    if (event_id == ARDUINO_USB_STARTED_EVENT || event_id == ARDUINO_USB_RESUME_EVENT) usbMounted = true;
    else if (event_id == ARDUINO_USB_STOPPED_EVENT || event_id == ARDUINO_USB_SUSPEND_EVENT) usbMounted = false;
  }
}

void handleRoot() { server.send(200, "text/html", HTML_DIAGNOSTIC); }

void handleJSON() {
  unsigned long now = millis();
  bool online = isBound && ((now - lastSeenTime) < DEVICE_TIMEOUT_MS);

  String json = "{";
  json += "\"host_mac\":\"" + WiFi.softAPmacAddress() + "\",";
  json += "\"mdns_name\":\"" + dynamicMDNS + ".local\",";
  json += "\"cpu_freq\":" + String(currentCpuFreq) + ",";
  json += "\"usb_mounted\":" + String(usbMounted ? "true" : "false") + ",";
  json += "\"usb_hz\":" + String(usbReportsPerSecond) + ",";
  json += "\"bound\":" + String(isBound ? "true" : "false") + ",";
  json += "\"online\":" + String(online ? "true" : "false") + ",";
  json += "\"mac\":\"" + macToString(boundMac) + "\",";
  json += "\"value\":" + String(lastRawValue) + ",";
  json += "\"buttons\":" + String(lastButtons) + ",";
  json += "\"sleep_timeout_sec\":" + String(lastSleepTimeoutSec) + ",";
  json += "\"packets\":" + String(packetCount);
  json += "}";
  server.send(200, "application/json", json);
}

void handleReset() {
  isBound = false;
  memset(boundMac, 0, 6);
  packetCount = 0;
  lastSleepTimeoutSec = 0;
  prefs.begin("rx_conf", false);
  prefs.remove("tx_mac");
  prefs.end();
  server.send(200, "text/plain", "OK");
}

void handleSetFreq() {
  if (server.hasArg("val")) {
    uint32_t freq = server.arg("val").toInt();
    if (freq == 240 || freq == 160 || freq == 80) {
      prefs.begin("sys_conf", false);
      prefs.putUInt("cpu_freq", freq);
      prefs.end();
      server.send(200, "text/plain", "OK");
      delay(100);
      ESP.restart();
      return;
    }
  }
  server.send(400, "text/plain", "BAD ARGS");
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len != ESP_NOW_PACKET_SIZE) return;

  struct_message msg;
  memcpy(&msg, incomingData, sizeof(msg));
  uint8_t sleepTimeoutSec = incomingData[4];

  const uint8_t* srcMac = recv_info->src_addr;
  unsigned long now = millis();

  if (msg.deviceType == DEV_TYPE_PAIR_REQ) {
    if (!isBound || (now - lastSeenTime > DEVICE_TIMEOUT_MS) || matchMac(boundMac, srcMac)) {
      isBound = true;
      memcpy(boundMac, srcMac, 6);
      lastSeenTime = now;
      
      prefs.begin("rx_conf", false);
      prefs.putBytes("tx_mac", boundMac, 6);
      prefs.end();

      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, srcMac, 6);
      peerInfo.channel = WIFI_CHANNEL;
      peerInfo.ifidx = WIFI_IF_AP;
      peerInfo.encrypt = false;
      if (!esp_now_is_peer_exist(srcMac)) esp_now_add_peer(&peerInfo);

      uint8_t ackPkt[7];
      ackPkt[0] = 0x55;
      uint8_t myMac[6];
      esp_read_mac(myMac, ESP_MAC_WIFI_SOFTAP);
      memcpy(ackPkt + 1, myMac, 6);
      esp_now_send(srcMac, ackPkt, 7);
    }
    return;
  }

  if (!isBound || matchMac(boundMac, srcMac)) {
    if (!isBound) {
      isBound = true;
      memcpy(boundMac, srcMac, 6);
      prefs.begin("rx_conf", false);
      prefs.putBytes("tx_mac", boundMac, 6);
      prefs.end();
      
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, srcMac, 6);
      peerInfo.channel = WIFI_CHANNEL;
      peerInfo.ifidx = WIFI_IF_AP;
      peerInfo.encrypt = false;
      if (!esp_now_is_peer_exist(srcMac)) esp_now_add_peer(&peerInfo);
    }

    lastRawValue = msg.value;
    lastButtons = msg.buttons;
    lastSleepTimeoutSec = sleepTimeoutSec;
    lastSeenTime = now;
    packetCount++;

    uint8_t hidPaddle = map(constrain(msg.value, 0, 4095), 0, 4095, 0, 255);
    uint8_t hidButtons = (msg.buttons & 0x01) ? 0x01 : 0x00;
    int8_t spinnerDelta = (msg.deviceType == 1) ? (int8_t)constrain(msg.value, -128, 127) : 0;

    USBDeviceHID.sendReport(hidButtons, spinnerDelta, hidPaddle);
  }
}

void setup() {
  prefs.begin("sys_conf", true);
  currentCpuFreq = prefs.getUInt("cpu_freq", 240);
  prefs.end();

  if (currentCpuFreq != 240 && currentCpuFreq != 160 && currentCpuFreq != 80) {
    currentCpuFreq = 240;
  }
  setCpuFrequencyMhz(currentCpuFreq);

  prefs.begin("rx_conf", true);
  if (prefs.getBytes("tx_mac", boundMac, 6) == 6) {
    isBound = true;
  }
  prefs.end();

  uint8_t baseMac[6];
  esp_read_mac(baseMac, ESP_MAC_WIFI_SOFTAP);
  
  char buf[5];
  snprintf(buf, sizeof(buf), "%02X%02X", baseMac[4], baseMac[5]);
  dynamicSSID = "MiSTer-RX-" + String(buf);
  dynamicMDNS = "atari-rx";

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(dynamicSSID.c_str(), AP_PASS, WIFI_CHANNEL, 0);

  if (MDNS.begin(dynamicMDNS.c_str())) {
    MDNS.addService("http", "tcp", HTTP_PORT);
  }

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnDataRecv);
  }

  server.on("/", handleRoot);
  server.on("/json", handleJSON);
  server.on("/reset", handleReset);
  server.on("/setfreq", handleSetFreq);
  server.begin();

  USB.onEvent(usbEventCallback);
  USB.productName(USB_PRODUCT_NAME);
  USB.manufacturerName(USB_MANUFACTURER_NAME);
  HID.begin();
  USB.begin();

  lastHzCalculationTime = millis();
}

void loop() {
  server.handleClient();
  
  unsigned long now = millis();
  if (now - lastHzCalculationTime >= STATS_CALC_INTERVAL_MS) {
    usbReportsPerSecond = usbReportCounterWindow;
    usbReportCounterWindow = 0;
    lastHzCalculationTime = now;
  }
  
  delay(2);
}