#include <M5Core2.h>
#include <Adafruit_seesaw.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLE2902.h>

// BLE Definitions
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_SERVER_NAME "M5Core2 Gamepad Server"

// Gamepad Variables
Adafruit_seesaw gamepad;
BLEClient *bleClient;
BLERemoteCharacteristic *dotPositionCharacteristic;
bool deviceConnected = false;

#define BUTTON_START 16
#define BUTTON_SELECT 0

int blueX = 150, blueY = 100, blueSpeed = 2;  
int redX = 0, redY = 0;  

class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient *pClient) {
    deviceConnected = true;
    Serial.println("Connected to BLE server.");
  }
  void onDisconnect(BLEClient *pClient) {
    deviceConnected = false;
    Serial.println("Disconnected from BLE server.");
  }
};

// Receive server dot position
void notifyCallback(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  String receivedData = String((char *)pData);
  int separatorIndex = receivedData.indexOf('-');
  if (separatorIndex != -1) {
    redX = receivedData.substring(0, separatorIndex).toInt();
    redY = receivedData.substring(separatorIndex + 1).toInt();
  }
}

void setup() {
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  Serial.begin(115200);

  BLEDevice::init("M5Core2 Client");
  bleClient = BLEDevice::createClient();
  bleClient->setClientCallbacks(new MyClientCallbacks());

  BLEScan *pBLEScan = BLEDevice::getScan();
  BLEScanResults foundDevices = pBLEScan->start(5);

  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    if (device.getName() == BLE_SERVER_NAME) {
      bleClient->connect(device.getAddress());
      BLERemoteService *remoteService = bleClient->getService(SERVICE_UUID);
      dotPositionCharacteristic = remoteService->getCharacteristic(CHARACTERISTIC_UUID);
      dotPositionCharacteristic->registerForNotify(notifyCallback);
      break;
    }
  }
}

void loop() {
  // Send position to server
  if (deviceConnected) {
    String position = String(blueX) + "-" + String(blueY);
    dotPositionCharacteristic->writeValue(position.c_str());
  }

  delay(50);
}
