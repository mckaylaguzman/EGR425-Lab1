#include <M5Unified.h>
#include <Adafruit_seesaw.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLE2902.h>

Adafruit_seesaw gamepad;
BLEClient *bleClient;
BLERemoteCharacteristic *dotPositionCharacteristic;
bool deviceConnected = false;

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define BUTTON_START 16
#define BUTTON_SELECT 0

int blueX = 150, blueY = 100, blueSpeed = 1;
int redX = 0, redY = 0;

class MyClientCallbacks : public BLEClientCallbacks {
    void onConnect(BLEClient *pClient) {
        deviceConnected = true;
        Serial.println("Connected to server.");
    }

    void onDisconnect(BLEClient *pClient) {
        deviceConnected = false;
        Serial.println("Disconnected from server.");
    }
};

// Receive dot position from server
void notifyCallback(BLERemoteCharacteristic *pCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
    String receivedData = String((char *)pData);
    int separatorIndex = receivedData.indexOf('-');
    if (separatorIndex != -1) {
        redX = receivedData.substring(0, separatorIndex).toInt();
        redY = receivedData.substring(separatorIndex + 1).toInt();
        Serial.printf("Received position: %d, %d\n", redX, redY);
    }
}

void setup() {
    M5.begin();
    M5.Lcd.fillScreen(BLACK);
    Serial.begin(115200);

    // Initialize Gamepad
    if (!gamepad.begin(0x50)) {
        Serial.println("ERROR! Gamepad not found.");
        while (1);
    }

    gamepad.pinMode(BUTTON_START, INPUT_PULLUP);
    gamepad.pinMode(BUTTON_SELECT, INPUT_PULLUP);

    // Initialize BLE Client
    BLEDevice::init("M5Core2 Client");
    bleClient = BLEDevice::createClient();
    bleClient->setClientCallbacks(new MyClientCallbacks());

    // Scan and connect to server
    BLEScan *pBLEScan = BLEDevice::getScan();
    BLEScanResults foundDevices = pBLEScan->start(5);

    for (int i = 0; i < foundDevices.getCount(); i++) {
        BLEAdvertisedDevice device = foundDevices.getDevice(i);
        if (device.getName() == "M5Core2 Server") {
            Serial.println("Connecting to M5Core2 Server...");
            bleClient->connect(device.getAddress());

            BLERemoteService *remoteService = bleClient->getService(SERVICE_UUID);
            if (remoteService) {
                dotPositionCharacteristic = remoteService->getCharacteristic(CHARACTERISTIC_UUID);
                if (dotPositionCharacteristic && dotPositionCharacteristic->canNotify()) {
                    dotPositionCharacteristic->registerForNotify(notifyCallback);
                }
            }
            break;
        }
    }
}

void loop() {
    uint32_t buttons = gamepad.digitalReadBulk(0xFFFF);

    // Read joystick movement
    int joyX = 1023 - gamepad.analogRead(14);
    int joyY = 1023 - gamepad.analogRead(15);

    float normX = (joyX - 512) / 512.0;
    float normY = (joyY - 512) / 512.0;

    // Move local blue dot
    if (abs(normX) > 0.2) blueX += (normX > 0 ? blueSpeed : -blueSpeed);
    if (abs(normY) > 0.2) blueY -= (normY > 0 ? blueSpeed : -blueSpeed);

    // Start Button: Adjust speed (1 to 5)
    if (!(buttons & (1UL << BUTTON_START))) {
        blueSpeed = (blueSpeed % 5) + 1;
        Serial.printf("Speed changed to: %d\n", blueSpeed);
        delay(300);
    }

    // Select Button: Warp dot
    if (!(buttons & (1UL << BUTTON_SELECT))) {
        blueX = random(0, 315);
        blueY = random(0, 235);
        Serial.printf("Warped to: (%d, %d)\n", blueX, blueY);
        delay(300);
    }

    // Keep dot within bounds
    blueX = constrain(blueX, 0, 315);
    blueY = constrain(blueY, 0, 235);

    // Display both dots
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillRect(blueX, blueY, 5, 5, BLUE);  // Local blue dot
    M5.Lcd.fillRect(redX, redY, 5, 5, RED);    // Server's red dot

    // Send position to server
    if (deviceConnected) {
        String position = String(blueX) + "-" + String(blueY);
        dotPositionCharacteristic->writeValue(position.c_str());
    }

    delay(50);
}
