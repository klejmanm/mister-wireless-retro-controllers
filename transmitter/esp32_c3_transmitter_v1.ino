/*
 * Atari 2600 Wireless Paddle Controller (XIAO ESP32-C3)
 *
 * Target Hardware: Seeed Studio XIAO ESP32-C3
 * ESP-NOW Mode: Ultra-low latency ~15ms (Fixed Channel 6)
 * BLE Mode: Native Daemonbite Mirror Descriptor
 */

#include <esp_netif.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ResponsiveAnalogRead.h>

#define DEBUG 0

#if DEBUG
  #define DEBUG_BEGIN(baud) Serial.begin(baud)
  #define DEBUG_PRINT(x)    Serial.print(x)
  #define DEBUG_PRINTLN(x)  Serial.println(x)
#else
  #define DEBUG_BEGIN(baud)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

#define POT_PIN D0        
#define FIRE_PIN D1       

#define MODE_ESPNOW 0
#define MODE_BLE    1

#define DEV_TYPE_PADDLE  0
#define DEV_TYPE_SPINNER 1

#define BOOT_CONFIG_HOLD_MS   5000  
#define INGAME_CONFIG_HOLD_MS 5000  

#define POT_MIN_START   280   
#define POT_MAX_END     3880   

// DANE SIECI ODBIORNIKA (HOSTA) DO WEBSERVERA
const char* ssid = "MiSTer-Retro-RX";
const char* password = "atari1234"; 

Preferences prefs;

const char *gp_serial = "MiSTer-S1 Spinner";

// ==============================================================================
// NATIVE BLE HID DESCRIPTOR
// ==============================================================================
const uint8_t hidReportDescriptor[] = {
  0x05, 0x01,                       // USAGE_PAGE (Generic Desktop)
  0x09, 0x04,                       // USAGE (Joystick)
  0xa1, 0x01,                       // COLLECTION (Application)
    0xa1, 0x00,                     // COLLECTION (Physical)
    
      0x05, 0x09,                   // USAGE_PAGE (Button)
      0x19, 0x01,                   // USAGE_MINIMUM (Button 1)
      0x29, 0x04,                   // USAGE_MAXIMUM (Button 4)
      0x15, 0x00,                   // LOGICAL_MINIMUM (0)
      0x25, 0x01,                   // LOGICAL_MAXIMUM (1)
      0x95, 0x08,                   // REPORT_COUNT (8)
      0x75, 0x01,                   // REPORT_SIZE (1)
      0x81, 0x02,                   // INPUT (Data,Var,Abs)
    
      0x05, 0x01,                   // USAGE_PAGE (Generic Desktop)

      0x09, 0x37,                   // USAGE (Dial)
      0x15, 0x80,                   // LOGICAL_MINIMUM (-128)
      0x25, 0x7F,                   // LOGICAL_MAXIMUM (127)
      0x95, 0x01,                   // REPORT_COUNT (1)
      0x75, 0x08,                   // REPORT_SIZE (8)
      0x81, 0x06,                   // INPUT (Data,Var,Rel)

      0x09, 0x38,                   // USAGE (Wheel)
      0x15, 0x00,                   // LOGICAL_MINIMUM (0)
      0x26, 0xFF, 0x00,             // LOGICAL_MAXIMUM (255)
      0x95, 0x01,                   // REPORT_COUNT (1)
      0x75, 0x08,                   // REPORT_SIZE (8)
      0x81, 0x02,                   // INPUT (Data,Var,Abs)

    0xc0,                           // END_COLLECTION
  0xc0                              // END_COLLECTION 
};

struct GamepadReport {
  uint8_t buttons; 
  int8_t  spinner;
  uint8_t paddle; 
} __attribute__((packed));

NimBLEHIDDevice* hidServer = nullptr;
NimBLECharacteristic* inputReport = nullptr;
NimBLEServer* pBleServer = nullptr;
bool bleConnected = false;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) { bleConnected = true; }
  void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) { bleConnected = true; }
  void onDisconnect(NimBLEServer* pServer) {
    bleConnected = false;
    NimBLEDevice::startAdvertising();
  }
  void onDisconnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
    bleConnected = false;
    NimBLEDevice::startAdvertising();
  }
  void onDisconnect(NimBLEServer* pServer, int reason) {
    bleConnected = false;
    NimBLEDevice::startAdvertising();
  }
};

WebServer server(80);

ResponsiveAnalogRead analogPot(POT_PIN, true);

// ==============================================================================
// ZUNIFIKOWANA RAMKA ESP-NOW (DOKŁADNIE 6 BAJTÓW)
// ==============================================================================
typedef struct __attribute__((packed)) {
  uint8_t deviceType; // 0 = Paddle, 1 = Spinner
  int16_t value;      // 0-4095 (Absolutny ADC dla Paddle)
  uint8_t buttons;    // Bitmaska przycisków (Bit 0 = FIRE)
} struct_message;

struct_message paddleData;
esp_now_peer_info_t peerInfo;

// Adres rozgłoszeniowy Broadcast (FF:FF:FF:FF:FF:FF) dla natychmiastowej komunikacji
uint8_t receiverAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 

uint8_t activeMode = MODE_ESPNOW;
bool isConfigMode = false;

volatile bool isReceiverConnected = false;
volatile unsigned long lastSuccessfulDelivery = 0;

unsigned long lastActivityTime = 0;
unsigned long lastEspNowSendTime = 0;

const unsigned long GAMING_TIMEOUT_MS = 120000; 
const unsigned long CONFIG_TIMEOUT_MS = 180000; 

int lastPotValue = -1;
int lastActivityPot = -1;
const int POT_NOISE_THRESHOLD = 2;          
const int SLEEP_MOVEMENT_THRESHOLD = 200;   
String txMacAddress = "";

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    isReceiverConnected = true;
    lastSuccessfulDelivery = millis();
  } else {
    if (millis() - lastSuccessfulDelivery > 1200) {
      isReceiverConnected = false;
    }
  }
}

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Atari 2600 Paddle Config</title>
  <style>
    body { font-family: monospace; background: #121212; color: #00ff66; text-align: center; padding: 15px; }
    .card { background: #1e1e1e; border: 2px solid #00ff66; border-radius: 12px; padding: 20px; max-width: 400px; margin: auto; margin-bottom: 15px; }
    .val { font-size: 2em; font-weight: bold; margin: 5px 0; color: #ffffff; }
    .btn { display: inline-block; padding: 10px 15px; font-size: 0.9em; border-radius: 8px; font-weight: bold; cursor: pointer; border: none; margin: 5px; }
    .btn-mode { background: #00aaff; color: #000; }
    .btn-apply { background: #00ff66; color: #000; font-size: 1.05em; padding: 12px 22px; width: 90%; margin-top: 15px; }
    .active { background: #ff0055; color: #fff; }
    .inactive { background: #333; color: #888; }
    .status-box { border-color: #00aaff; }
    .mac-txt { color: #aaaaaa; font-size: 0.9em; margin-top: 4px; }
    .mac-highlight { color: #ffcc00; font-weight: bold; }
    .sleep-box { color: #ffcc00; font-size: 1em; font-weight: bold; margin-top: 15px; border-top: 1px dashed #333; padding-top: 10px; }
    progress { width: 100%; height: 20px; margin-top: 10px; }
  </style>
</head>
<body>
  <div class="card status-box">
    <h3>PADDLE CONFIGURATION</h3>
    <p>Current Mode: <span id="modeName" class="mac-highlight">--</span></p>
    <button class="btn btn-mode" onclick="setMode(0)">ESP-NOW Mode</button>
    <button class="btn btn-mode" onclick="setMode(1)">Bluetooth BLE Mode</button>
    <hr style="border-color: #333; margin: 15px 0;">
    <button class="btn btn-apply" onclick="rebootDevice()">Exit Config & Reboot</button>
  </div>
  <div class="card">
    <h2>ATARI PADDLE TESTER</h2>
    <div class="mac-txt" style="margin-bottom: 15px;">Transmitter MAC: <span class="mac-highlight">%TX_MAC%</span></div>
    <p>POTENTIOMETER (12-bit ADC)</p>
    <div class="val" id="potVal">0</div>
    <progress id="potBar" value="0" max="4095"></progress>
    <br><br>
    <p>FIRE BUTTON</p>
    <div id="fireBtn" class="btn inactive">RELEASED</div>
    <div class="sleep-box">SLEEP IN: <span id="sleepTimer">--</span> s</div>
  </div>
  <script>
    function setMode(m) {
      if(confirm("Change mode and reboot?")) {
        fetch('/setmode?mode=' + m).then(() => {
          alert("Mode changed! Rebooting device...");
          setTimeout(() => { location.reload(); }, 3000);
        });
      }
    }
    function rebootDevice() {
      fetch('/reboot').then(() => {
        alert("Rebooting device...");
      });
    }
    setInterval(function() {
      fetch('/data').then(response => response.json()).then(data => {
        document.getElementById("potVal").innerText = data.pot;
        document.getElementById("potBar").value = data.pot;
        document.getElementById("modeName").innerText = (data.mode == 0) ? "ESP-NOW (Receiver)" : "Bluetooth BLE HID";
        let btn = document.getElementById("fireBtn");
        if(data.fire) {
          btn.innerText = " [ PRESSED ] ";
          btn.className = "btn active";
        } else {
          btn.innerText = " RELEASED ";
          btn.className = "btn inactive";
        }
        document.getElementById("sleepTimer").innerText = data.sleep_in;
      });
    }, 150);
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  String html = String(HTML_PAGE);
  html.replace("%TX_MAC%", txMacAddress);
  server.send(200, "text/html", html);
}

void handleData() {
  unsigned long activeTimeout = isConfigMode ? CONFIG_TIMEOUT_MS : GAMING_TIMEOUT_MS;
  long remainingMs = (long)activeTimeout - (long)(millis() - lastActivityTime);
  int remainingSec = (remainingMs > 0) ? (remainingMs / 1000) : 0;

  String json = "{";
  json += "\"pot\":" + String(paddleData.value) + ",";
  json += "\"fire\":" + String((paddleData.buttons & 0x01) ? 1 : 0) + ",";
  json += "\"mode\":" + String(activeMode) + ",";
  json += "\"sleep_in\":" + String(remainingSec);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetMode() {
  if (server.hasArg("mode")) {
    uint8_t newMode = server.arg("mode").toInt();
    prefs.begin("paddle_conf", false);
    prefs.putUInt("mode", newMode);
    prefs.end();
    server.send(200, "text/plain", "OK");
    delay(200);
    ESP.restart();
  }
}

void handleReboot() {
  server.send(200, "text/plain", "OK");
  delay(200);
  ESP.restart();
}

void startWebServer() {
  if (isConfigMode) return;
  isConfigMode = true;
  setCpuFrequencyMhz(160); 
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  int wifi_timeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_timeout < 40) {
    delay(250);
    wifi_timeout++;
  }
  txMacAddress = WiFi.macAddress();
  if (WiFi.status() == WL_CONNECTED) {
    MDNS.begin("atari-paddle");
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/setmode", handleSetMode);
    server.on("/reboot", handleReboot);
    server.begin();
  }
  lastActivityTime = millis();
}

void goToDeepSleep() {
  if (activeMode == MODE_BLE) {
    if (pBleServer != nullptr && pBleServer->getConnectedCount() > 0) {
      pBleServer->disconnect(0);
      delay(300);
    }
    NimBLEDevice::deinit(true);
  } else {
    esp_now_deinit();
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(200);

  esp_deep_sleep_enable_gpio_wakeup(1ULL << GPIO_NUM_3, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

void setupBLEHID() {
  NimBLEDevice::init(gp_serial);
  
  NimBLEDevice::setSecurityAuth(true, false, true); 
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  
  NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
  NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

  pBleServer = NimBLEDevice::createServer();
  pBleServer->setCallbacks(new ServerCallbacks());

  hidServer = new NimBLEHIDDevice(pBleServer);
  inputReport = hidServer->getInputReport(1);

  hidServer->setManufacturer("Atari Retro");
  hidServer->setHidInfo(0x00, 0x01);

  NimBLECharacteristic* pnpChar = hidServer->getDeviceInfoService()->createCharacteristic(
    NimBLEUUID((uint16_t)0x2A50), 
    NIMBLE_PROPERTY::READ
  );
  uint8_t pnpVal[] = {0x02, 0x02, 0xE5, 0xAB, 0xBB, 0x10, 0x01};
  pnpChar->setValue(pnpVal, sizeof(pnpVal));

  hidServer->setReportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
  hidServer->startServices();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName(gp_serial);            
  pAdvertising->setAppearance(0x03c4); 
  pAdvertising->addServiceUUID(hidServer->getHidService()->getUUID());
  pAdvertising->enableScanResponse(true);         
  pAdvertising->start();
}

void setup() {
  DEBUG_BEGIN(115200);
  delay(500);

  analogReadResolution(12);
  pinMode(FIRE_PIN, INPUT_PULLUP);

  analogPot.setAnalogResolution(4095);
  analogPot.setSnapMultiplier(0.25);     
  analogPot.setActivityThreshold(12.0);  
  analogPot.enableEdgeSnap();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason != ESP_SLEEP_WAKEUP_GPIO) {
    unsigned long holdStart = millis();
    while (digitalRead(FIRE_PIN) == LOW) {
      if (millis() - holdStart >= BOOT_CONFIG_HOLD_MS) {
        isConfigMode = true;
        break;
      }
      delay(50);
    }
  }

  prefs.begin("paddle_conf", true);
  activeMode = prefs.getUInt("mode", MODE_ESPNOW);
  prefs.end();

  if (activeMode != MODE_ESPNOW && activeMode != MODE_BLE) {
    activeMode = MODE_ESPNOW; 
  }

  if (isConfigMode) {
    startWebServer();
  } else {
    setCpuFrequencyMhz(80); 
    if (activeMode == MODE_ESPNOW) {
      // CZYSTY ER-NOW W TRYBIE STA NA KANALE 6
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);
      WiFi.setTxPower(WIFI_POWER_11dBm);

      esp_wifi_set_promiscuous(true);
      esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(false);

      if (esp_now_init() == ESP_OK) {
        esp_now_register_send_cb(OnDataSent);

        memset(&peerInfo, 0, sizeof(peerInfo));
        memcpy(peerInfo.peer_addr, receiverAddress, 6); // Broadcast FF:FF:FF:FF:FF:FF
        peerInfo.channel = 6;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
      }
    } else {
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);
      setupBLEHID();
    }
  }

  lastActivityTime = millis();
}

void loop() {
  analogPot.update();
  uint16_t filteredRaw = analogPot.getValue();
  bool currentFire = (digitalRead(FIRE_PIN) == LOW);

  uint16_t activePot = constrain(filteredRaw, POT_MIN_START, POT_MAX_END);
  uint16_t currentPot = map(activePot, POT_MIN_START, POT_MAX_END, 0, 4095);

  paddleData.deviceType = DEV_TYPE_PADDLE;
  paddleData.value = currentPot;
  paddleData.buttons = currentFire ? 0x01 : 0x00;

  static unsigned long fireHoldInGameStart = 0;
  if (currentFire) {
    if (fireHoldInGameStart == 0) {
      fireHoldInGameStart = millis();
    } else if (!isConfigMode && (millis() - fireHoldInGameStart >= INGAME_CONFIG_HOLD_MS)) {
      startWebServer();
      fireHoldInGameStart = 0;
    }
  } else {
    fireHoldInGameStart = 0;
  }

  if (isConfigMode && WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  if (!isConfigMode && activeMode == MODE_ESPNOW) {
    if (millis() - lastEspNowSendTime > 15) {
      esp_now_send(receiverAddress, (uint8_t *) &paddleData, sizeof(paddleData));
      lastEspNowSendTime = millis();
    }
  } else if (!isConfigMode && activeMode == MODE_BLE) {
    bool isConnected = bleConnected || (pBleServer != nullptr && pBleServer->getConnectedCount() > 0);
    
    if (isConnected && inputReport != nullptr) {
      uint8_t currentPaddle = map(activePot, POT_MIN_START, POT_MAX_END, 0, 255);
      uint8_t currentButtons = currentFire ? 0x01 : 0x00;
      
      static uint8_t lastSentPaddle = 0xFF;
      static uint8_t lastSentButtons = 0xFF;

      if (currentPaddle != lastSentPaddle || currentButtons != lastSentButtons) {
        GamepadReport report;
        report.buttons = currentButtons;
        report.spinner = 0;              
        report.paddle = currentPaddle;   

        inputReport->setValue((uint8_t*)&report, sizeof(report));
        inputReport->notify();
        
        lastSentPaddle = currentPaddle;
        lastSentButtons = currentButtons;
      }
    }
    delay(5);
  }

  if (abs((int)currentPot - lastPotValue) > POT_NOISE_THRESHOLD || currentFire) {
    lastPotValue = currentPot;
  }

  if (lastActivityPot == -1) lastActivityPot = currentPot;

  if (abs((int)currentPot - lastActivityPot) > SLEEP_MOVEMENT_THRESHOLD || currentFire) {
    lastActivityPot = currentPot;
    lastActivityTime = millis();
  }

  unsigned long currentTimeout = isConfigMode ? CONFIG_TIMEOUT_MS : GAMING_TIMEOUT_MS;
  if (millis() - lastActivityTime > currentTimeout) {
    goToDeepSleep();
  }
}
