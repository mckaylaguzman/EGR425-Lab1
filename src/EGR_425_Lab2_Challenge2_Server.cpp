#include <M5Core2.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Adafruit_seesaw.h>

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
BLEServer *bleServer;
BLEService *bleService;
BLECharacteristic *bleCharacteristic;
BLECharacteristic *clientPositionCharacteristic;
bool deviceConnected = false;
bool previouslyConnected = false;
bool showMessage = true;  // Show connection message first

// Gamepad Variables
Adafruit_seesaw gamepad;
#define BUTTON_START 16
#define BUTTON_SELECT 0

// Server's Blue Dot
int dotX = 150, dotY = 100;
int dotSpeed = 1;

// Opponent's Red Dot (Client-controlled)
int redX = -1, redY = -1;  
unsigned long startTime, gameTime;
bool gameOver = false;

///////////////////////////////////////////////////////////////
// BLE UUIDs
///////////////////////////////////////////////////////////////
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CLIENT_POSITION_UUID "7b1c5f64-2d3c-4d91-8ea7-bb643b7c5a1f" // Client writes its position here

static String BLE_BROADCAST_NAME = "Mckaylas M5Core2024";

///////////////////////////////////////////////////////////////
// Function Declarations
///////////////////////////////////////////////////////////////
void broadcastBleServer();
void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(10, 50);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.println(text);
}

///////////////////////////////////////////////////////////////
// BLE Server Callbacks
///////////////////////////////////////////////////////////////
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        previouslyConnected = true;
        showMessage = true;
        Serial.println("Device connected...");
        startTime = millis(); // Start timer
        bleCharacteristic->setValue("CONNECTED");
        bleCharacteristic->notify();
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }
};

// BLE Characteristic Callback for receiving Client's dot position
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.empty()) return;

        int separatorIndex = value.find('-');
        if (separatorIndex != std::string::npos) {
            int tempRedX = atoi(value.substr(0, separatorIndex).c_str());
            int tempRedY = atoi(value.substr(separatorIndex + 1).c_str());

            redX = constrain(tempRedX, 0, 315);
            redY = constrain(tempRedY, 0, 235);
        }
    }
};

///////////////////////////////////////////////////////////////
// Setup Function
///////////////////////////////////////////////////////////////
void setup() {
    M5.begin();
    M5.Lcd.setTextSize(3);
    M5.Lcd.fillScreen(BLACK);

    Serial.println("Starting BLE...");
    BLEDevice::init(BLE_BROADCAST_NAME.c_str());

    broadcastBleServer();
    drawScreenTextWithBackground("Broadcasting BLE server:\n\n" + BLE_BROADCAST_NAME, TFT_BLUE);

    if (!gamepad.begin(0x50)) {
        Serial.println("ERROR: Gamepad not found");
        while (1);
    }
    gamepad.pinMode(BUTTON_START, INPUT_PULLUP);
    gamepad.pinMode(BUTTON_SELECT, INPUT_PULLUP);
}

///////////////////////////////////////////////////////////////
// Main Loop
///////////////////////////////////////////////////////////////
void loop() {
    if (deviceConnected && !gameOver) {
        // Read joystick input for the server's movement
        int joyX = 1023 - gamepad.analogRead(14);
        int joyY = 1023 - gamepad.analogRead(15);

        float normX = (joyX - 512) / 512.0;
        float normY = (joyY - 512) / 512.0;

        if (abs(normX) > 0.2) dotX += (normX > 0 ? dotSpeed : -dotSpeed);
        if (abs(normY) > 0.2) dotY -= (normY > 0 ? dotSpeed : -dotSpeed);

        dotX = constrain(dotX, 0, 315);
        dotY = constrain(dotY, 0, 235);

        // Update Client with Server's position
        String positionUpdate = String(dotX) + "-" + String(dotY);
        bleCharacteristic->setValue(positionUpdate.c_str());
        bleCharacteristic->notify();

        // Draw both dots
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.fillRect(dotX, dotY, 5, 5, BLUE);
        M5.Lcd.fillRect(redX, redY, 5, 5, RED);

        // Check for collision
        if (abs(dotX - redX) < 5 && abs(dotY - redY) < 5) {
            gameOver = true;
            gameTime = (millis() - startTime) / 1000;
            drawScreenTextWithBackground("Game Over!\nTime: " + String(gameTime) + "s", TFT_RED);
        }
    }
    delay(30);
}

///////////////////////////////////////////////////////////////
// BLE Server Setup
///////////////////////////////////////////////////////////////
void broadcastBleServer() {    
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new MyServerCallbacks());

    bleService = bleServer->createService(SERVICE_UUID);
    
    // Server's dot position characteristic
    bleCharacteristic = bleService->createCharacteristic(
        CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );

    // Client's dot position characteristic
    clientPositionCharacteristic = bleService->createCharacteristic(
        CLIENT_POSITION_UUID, BLECharacteristic::PROPERTY_WRITE
    );

    clientPositionCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    bleService->start();
    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(SERVICE_UUID);
    BLEDevice::startAdvertising();
}
