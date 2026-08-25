// Atari 2600 Wireless Paddle Controller - Transmitter (XIAO ESP32-C3)
// Modes: ESP-NOW (Unicast po Handshake P3P) / NimBLE HID
// Reset parowania ESP-NOW: Przytrzymaj FIRE przez 5 sekund przy wybudzaniu.

#include <esp_netif.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <ResponsiveAnalogRead.h>

// ==============================================================================
// SELEKCJA KONFIGURACJI HARDWARE'OWEJ
// ==============================================================================
#define PADDLE_ID             1  

#if PADDLE_ID == 1
  #define POT_PIN             D0
  #define FIRE_PIN            D1
  #define FIRE_GPIO_NUM       3     
#elif PADDLE_ID == 2
  #define POT_PIN             D1
  #define FIRE_PIN            D2    
  #define FIRE_GPIO_NUM       4     
#endif

// ==============================================================================
// POZOSTAŁE DEFINICJE I KONFIGURACJA
// ==============================================================================
#define MODE_ESPNOW           0
#define MODE_BLE              1
#define DEV_TYPE_PADDLE       0
#define DEV_TYPE_SPINNER      1
#define DEV_TYPE_PAIR_REQ     0xAA  

#define MODE_SWITCH_HOLD_MS   10000 
#define INACTIVITY_TIMEOUT_MS 120000 
#define POT_MIN_START         280   
#define POT_MAX_END           3880   
#define WIFI_CHANNEL          6
#define SLEEP_MOVEMENT_THR    150   

Preferences prefs;
ResponsiveAnalogRead analogPot(POT_PIN, true);

const char *gp_serial = (PADDLE_ID == 1) ? "MiSTer-S1 Spinner P1" : "MiSTer-S1 Spinner P2";

uint8_t activeMode = MODE_ESPNOW;
unsigned long lastActivityTime = 0;
unsigned long lastEspNowSendTime = 0;
unsigned long fireHoldStart = 0;
int lastSleepCheckPot = -1; 

bool isPaired = false;
uint8_t targetRxMac[6] = {0};
uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct __attribute__((packed)) {
  uint8_t deviceType;      
  int16_t value;           
  uint8_t buttons;         
  uint8_t sleepTimeoutSec; 
} struct_message;

struct_message paddleData;
esp_now_peer_info_t peerInfo;

const uint8_t hidReportDescriptor[] = {
  0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0xa1, 0x00, 
  0x05, 0x09, 0x19, 0x01, 0x29, 0x04, 0x15, 0x00, 
  0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x81, 0x02, 
  0x05, 0x01, 0x09, 0x37, 0x15, 0x80, 0x25, 0x7F, 
  0x95, 0x01, 0x75, 0x08, 0x81, 0x06, 0x09, 0x38, 
  0x15, 0x00, 0x26, 0xFF, 0x00, 0x95, 0x01, 0x75, 
  0x08, 0x81, 0x02, 0xc0, 0xc0 
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
  void onDisconnect(NimBLEServer* pServer) {
    bleConnected = false;
    NimBLEDevice::startAdvertising();
  }
};

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
  esp_deep_sleep_enable_gpio_wakeup(1ULL << FIRE_GPIO_NUM, ESP_GPIO_WAKEUP_GPIO_LOW);
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
    NimBLEUUID((uint16_t)0x2A50), NIMBLE_PROPERTY::READ
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

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (!isPaired && len == 7 && incomingData[0] == 0x55) {
    memcpy(targetRxMac, incomingData + 1, 6);
    isPaired = true;
    
    prefs.begin("paddle_conf", false);
    prefs.putBytes("rx_mac", targetRxMac, 6);
    prefs.end();

    if (esp_now_is_peer_exist(broadcastMac)) esp_now_del_peer(broadcastMac);
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, targetRxMac, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.ifidx = WIFI_IF_STA; // Gwarancja dla nadajnika
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }
}

void setup() {
  analogReadResolution(12);
  pinMode(FIRE_PIN, INPUT_PULLUP);
  analogPot.setAnalogResolution(4095);
  analogPot.setSnapMultiplier(0.25);     
  analogPot.setActivityThreshold(30.0);  
  analogPot.enableEdgeSnap();

  if (digitalRead(FIRE_PIN) == LOW) {
    unsigned long startHold = millis();
    while (digitalRead(FIRE_PIN) == LOW) {
      if (millis() - startHold > 5000) {
        prefs.begin("paddle_conf", false);
        prefs.remove("rx_mac");
        prefs.end();
        break;
      }
      delay(10);
    }
  }

  prefs.begin("paddle_conf", true);
  activeMode = prefs.getUInt("mode", MODE_ESPNOW);
  if (prefs.getBytes("rx_mac", targetRxMac, 6) == 6) {
    isPaired = true;
  }
  prefs.end();

  if (activeMode != MODE_ESPNOW && activeMode != MODE_BLE) activeMode = MODE_ESPNOW; 
  setCpuFrequencyMhz(80); 

  if (activeMode == MODE_ESPNOW) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_11dBm);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() == ESP_OK) {
      esp_now_register_recv_cb(OnDataRecv);
      memset(&peerInfo, 0, sizeof(peerInfo));
      memcpy(peerInfo.peer_addr, isPaired ? targetRxMac : broadcastMac, 6);
      peerInfo.channel = WIFI_CHANNEL;
      peerInfo.ifidx = WIFI_IF_STA;
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);
    }
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    setupBLEHID();
  }

  lastActivityTime = millis();
}

void loop() {
  analogPot.update();
  uint16_t filteredRaw = analogPot.getValue();
  bool currentFire = (digitalRead(FIRE_PIN) == LOW);

  uint16_t activePot = constrain(filteredRaw, POT_MIN_START, POT_MAX_END);
  uint16_t currentPot = map(activePot, POT_MIN_START, POT_MAX_END, 0, 4095);

  if (lastSleepCheckPot == -1) lastSleepCheckPot = currentPot;

  if (abs((int)currentPot - lastSleepCheckPot) > SLEEP_MOVEMENT_THR || currentFire) {
    lastActivityTime = millis();
    lastSleepCheckPot = currentPot;
  }

  unsigned long elapsed = millis() - lastActivityTime;
  uint8_t remainingSec = (elapsed < INACTIVITY_TIMEOUT_MS) ? (uint8_t)((INACTIVITY_TIMEOUT_MS - elapsed) / 1000) : 0;

  paddleData.deviceType = isPaired ? DEV_TYPE_PADDLE : DEV_TYPE_PAIR_REQ;
  paddleData.value = currentPot;
  paddleData.buttons = currentFire ? 0x01 : 0x00;
  paddleData.sleepTimeoutSec = remainingSec;

  if (currentFire) {
    if (fireHoldStart == 0) fireHoldStart = millis();
    else if (millis() - fireHoldStart >= MODE_SWITCH_HOLD_MS) {
      uint8_t newMode = (activeMode == MODE_ESPNOW) ? MODE_BLE : MODE_ESPNOW;
      prefs.begin("paddle_conf", false);
      prefs.putUInt("mode", newMode);
      prefs.end();
      delay(500);
      ESP.restart();
    }
  } else {
    fireHoldStart = 0;
  }

  if (activeMode == MODE_ESPNOW) {
    if (millis() - lastEspNowSendTime > 15) {
      uint8_t* destMac = isPaired ? targetRxMac : broadcastMac;
      esp_now_send(destMac, (uint8_t *) &paddleData, sizeof(paddleData));
      lastEspNowSendTime = millis();
    }
  } else if (activeMode == MODE_BLE) {
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

  if (elapsed >= INACTIVITY_TIMEOUT_MS) goToDeepSleep();
}