#include <M5Unified.h>
#include <Adafruit_seesaw.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

Adafruit_seesaw gamepad;
BLEServer *bleServer;
BLECharacteristic *dotPositionCharacteristic;

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define BUTTON_START 16
#define BUTTON_SELECT 0

int blueX = 150, blueY = 100, blueSpeed = 1;

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

    // Initialize BLE Server
    BLEDevice::init("M5Core2 Server");
    bleServer = BLEDevice::createServer();
    BLEService *bleService = bleServer->createService(SERVICE_UUID);

    // Create BLE Characteristic
    dotPositionCharacteristic = bleService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    dotPositionCharacteristic->addDescriptor(new BLE2902());

    bleService->start();
    BLEDevice::startAdvertising();
    Serial.println("BLE Server is advertising...");
}

void loop() {
    uint32_t buttons = gamepad.digitalReadBulk(0xFFFF);

    // Read joystick movement
    int joyX = 1023 - gamepad.analogRead(14);
    int joyY = 1023 - gamepad.analogRead(15);

    float normX = (joyX - 512) / 512.0;
    float normY = (joyY - 512) / 512.0;

    // 🟢 **Move Blue Dot Based on Speed**
    if (abs(normX) > 0.2) blueX += (normX > 0 ? blueSpeed : -blueSpeed); // Multiply by speed
    if (abs(normY) > 0.2) blueY -= (normY > 0 ? blueSpeed : -blueSpeed); // Multiply by speed

    // 🟠 **Start Button: Adjust Speed (1 to 5)**
    if (!(buttons & (1UL << BUTTON_START))) {
        blueSpeed = (blueSpeed % 5) + 1;  // Cycle speed 1 to 5
        Serial.printf("Speed changed to: %d\n", blueSpeed);
        delay(300);  // Debounce
    }

    // 🔴 **Select Button: Warp to Random Location**
    if (!(buttons & (1UL << BUTTON_SELECT))) {
        blueX = random(0, 315);
        blueY = random(0, 235);
        Serial.printf("Warped to: (%d, %d)\n", blueX, blueY);
        delay(300);  // Debounce
    }

    // Keep dot within screen bounds
    blueX = constrain(blueX, 0, 315);
    blueY = constrain(blueY, 0, 235);

    // Display Blue Dot
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillRect(blueX, blueY, 5, 5, BLUE);

    // Send position to client
    String position = String(blueX) + "-" + String(blueY);
    dotPositionCharacteristic->setValue(position.c_str());
    dotPositionCharacteristic->notify();

    delay(50);  // Smooth refresh
}
