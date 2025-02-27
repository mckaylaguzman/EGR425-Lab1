#include <M5Core2.h>
#include <Adafruit_seesaw.h>
#include <BLEDevice.h>
#include <BLE2902.h>

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
static BLERemoteCharacteristic *bleRemoteCharacteristic;
static BLEAdvertisedDevice *bleRemoteServer;
static boolean doConnect = false;
static boolean doScan = false;
bool deviceConnected = false;

// Gamepad Variables
Adafruit_seesaw gamepad;
#define BUTTON_START 16
#define BUTTON_SELECT 0
int blueX = 150, blueY = 100, blueSpeed = 1;
bool showGameScreen = false;  // Flag to control screen transition

///////////////////////////////////////////////////////////////
// BLE UUIDs
///////////////////////////////////////////////////////////////
static BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b"); 
static BLEUUID CHARACTERISTIC_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// BLE Broadcast Name
static String BLE_BROADCAST_NAME = "Mckaylas M5Core2024";

///////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor);
void sendGamepadData();

///////////////////////////////////////////////////////////////
// BLE Client Callback Methods (Handles Server Notifications)
///////////////////////////////////////////////////////////////
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
    Serial.printf("Notify callback for characteristic %s of data length %d\n", pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
    Serial.printf("\tData: %s\n", (char *)pData);
}

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods (Handles Connection/Disconnection)
///////////////////////////////////////////////////////////////
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient *pclient) {
        deviceConnected = true;
        Serial.println("Device connected...");
    }

    void onDisconnect(BLEClient *pclient) {
        deviceConnected = false;
        Serial.println("Device disconnected...");
        showGameScreen = true; // Ensure game screen still shows even if disconnected
    }
};

///////////////////////////////////////////////////////////////
// Connect to BLE Server
///////////////////////////////////////////////////////////////
bool connectToServer() {
    Serial.printf("Forming a connection to %s\n", bleRemoteServer->getName().c_str());
    BLEClient *bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks(new MyClientCallback());

    if (!bleClient->connect(bleRemoteServer)) {
        Serial.printf("FAILED to connect to server (%s)\n", bleRemoteServer->getName().c_str());
        showGameScreen = true;  // If connection fails, still show the game screen
        return false;
    }
    
    Serial.printf("Connected to server (%s)\n", bleRemoteServer->getName().c_str());

    BLERemoteService *bleRemoteService = bleClient->getService(SERVICE_UUID);
    if (bleRemoteService == nullptr) {
        Serial.printf("Failed to find our service UUID: %s\n", SERVICE_UUID.toString().c_str());
        bleClient->disconnect();
        showGameScreen = true;
        return false;
    }
    
    Serial.printf("Found our service UUID: %s\n", SERVICE_UUID.toString().c_str());

    bleRemoteCharacteristic = bleRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (bleRemoteCharacteristic == nullptr) {
        Serial.printf("Failed to find our characteristic UUID: %s\n", CHARACTERISTIC_UUID.toString().c_str());
        bleClient->disconnect();
        showGameScreen = true;
        return false;
    }
    
    Serial.printf("Found our characteristic UUID: %s\n", CHARACTERISTIC_UUID.toString().c_str());

    if (bleRemoteCharacteristic->canNotify())
        bleRemoteCharacteristic->registerForNotify(notifyCallback);

    showGameScreen = true; // After successful connection, show game screen
    return true;
}

///////////////////////////////////////////////////////////////
// Scan for BLE servers and find the first one that advertises
// the service we are looking for.
///////////////////////////////////////////////////////////////
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("BLE Advertised Device found:");
        Serial.printf("\tName: %s\n", advertisedDevice.getName().c_str());

        if (advertisedDevice.haveServiceUUID() && 
            advertisedDevice.isAdvertisingService(SERVICE_UUID) && 
            advertisedDevice.getName() == BLE_BROADCAST_NAME.c_str()) {
            BLEDevice::getScan()->stop();
            bleRemoteServer = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }
    }     
};        

///////////////////////////////////////////////////////////////
// Setup Function
///////////////////////////////////////////////////////////////
void setup() {
    M5.begin();
    M5.Lcd.setTextSize(3);
    drawScreenTextWithBackground("Scanning for BLE server...", TFT_BLUE);

    BLEDevice::init("");

    BLEScan *pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(0, false);

    // Initialize Gamepad
    if (!gamepad.begin(0x50)) {
        Serial.println("ERROR! Gamepad not found.");
        while (1);
    }
    gamepad.pinMode(BUTTON_START, INPUT_PULLUP);
    gamepad.pinMode(BUTTON_SELECT, INPUT_PULLUP);
}

///////////////////////////////////////////////////////////////
// Main Loop
///////////////////////////////////////////////////////////////
void loop() {
    if (doConnect) {
        if (connectToServer()) {
            Serial.println("Connected to BLE Server.");
            drawScreenTextWithBackground("Connected to BLE server!", TFT_GREEN);
            doConnect = false;
            delay(1000);
        } else {
            Serial.println("Failed to connect to server.");
            drawScreenTextWithBackground("FAILED to connect to BLE server.", TFT_RED);
            delay(1000);
        }
    }

    if (deviceConnected || showGameScreen) {
        sendGamepadData();
    } else if (doScan) {
        drawScreenTextWithBackground("Disconnected... re-scanning for BLE server...", TFT_ORANGE);
        BLEDevice::getScan()->start(0);
    }

    delay(50); // Smooth refresh
}

///////////////////////////////////////////////////////////////
// Send Gamepad Data to BLE Server
///////////////////////////////////////////////////////////////
void sendGamepadData() {
    uint32_t buttons = gamepad.digitalReadBulk(0xFFFF);
    int joyX = 1023 - gamepad.analogRead(14);
    int joyY = 1023 - gamepad.analogRead(15);

    float normX = (joyX - 512) / 512.0;
    float normY = (joyY - 512) / 512.0;

    if (abs(normX) > 0.2) blueX += (normX > 0 ? blueSpeed : -blueSpeed);
    if (abs(normY) > 0.2) blueY -= (normY > 0 ? blueSpeed : -blueSpeed);

    if (!(buttons & (1UL << BUTTON_START))) {
        blueSpeed = (blueSpeed % 5) + 1;
        Serial.printf("Speed changed to: %d\n", blueSpeed);
        delay(300);
    }

    if (!(buttons & (1UL << BUTTON_SELECT))) {
        blueX = random(0, 315);
        blueY = random(0, 235);
        Serial.printf("Warped to: (%d, %d)\n", blueX, blueY);
        delay(300);
    }

    blueX = constrain(blueX, 0, 315);
    blueY = constrain(blueY, 0, 235);

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillRect(blueX, blueY, 5, 5, BLUE);

    String position = String(blueX) + "-" + String(blueY);
    bleRemoteCharacteristic->writeValue(position.c_str(), position.length());
    Serial.printf("Sent position: %s\n", position.c_str());
}

///////////////////////////////////////////////////////////////
// Screen Helper Function
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println(text);
}
