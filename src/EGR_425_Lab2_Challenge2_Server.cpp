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
bool deviceConnected = false;
bool previouslyConnected = false;
bool showMessage = true;  // Show connection message first

// Gamepad Variables
Adafruit_seesaw gamepad;
#define BUTTON_START 16
#define BUTTON_SELECT 0

// Server's Blue Dot (Local)
int dotX = 150, dotY = 100;
int dotSpeed = 1;

// Opponent's Red Dot (Client-controlled)
int redX = -1, redY = -1;  // Start with invalid values
int lastReceivedRedX = -1, lastReceivedRedY = -1;  // Track last position

///////////////////////////////////////////////////////////////
// BLE UUIDs
///////////////////////////////////////////////////////////////
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// BLE Broadcast Name
static String BLE_BROADCAST_NAME = "Mckaylas M5Core2024";

///////////////////////////////////////////////////////////////
// Function Declarations
///////////////////////////////////////////////////////////////
void broadcastBleServer();
void drawScreenTextWithBackground(String text, int backgroundColor);

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods
///////////////////////////////////////////////////////////////
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        previouslyConnected = true;
        showMessage = true;  // Reset flag to show the message when connecting
        Serial.println("Device connected...");

        // Notify client to transition to game screen
        bleCharacteristic->setValue("CONNECTED");
        bleCharacteristic->notify();
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected...");
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
    drawScreenTextWithBackground("Broadcasting as BLE server:\n\n" + BLE_BROADCAST_NAME, TFT_BLUE);

    // Initialize Gamepad
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
    if (deviceConnected) {
        // Read joystick input for the server's movement
        uint32_t buttons = gamepad.digitalReadBulk(0xFFFF);
        int joyX = 1023 - gamepad.analogRead(14);
        int joyY = 1023 - gamepad.analogRead(15);

        float normX = (joyX - 512) / 512.0;
        float normY = (joyY - 512) / 512.0;

        if (abs(normX) > 0.2) dotX += (normX > 0 ? dotSpeed : -dotSpeed);
        if (abs(normY) > 0.2) dotY -= (normY > 0 ? dotSpeed : -dotSpeed);

        dotX = constrain(dotX, 0, 315);
        dotY = constrain(dotY, 0, 235);

        // ✅ Read opponent's position from BLE
        std::string readValue = bleCharacteristic->getValue();
        if (!readValue.empty() && readValue.find('-') != std::string::npos) {
            int commaIndex = String(readValue.c_str()).indexOf('-');
            if (commaIndex > 0) {
                int tempRedX = String(readValue.c_str()).substring(0, commaIndex).toInt();
                int tempRedY = String(readValue.c_str()).substring(commaIndex + 1).toInt();

                // ✅ FIX: Ensure red dot is NOT mistakenly set to our own blue dot position
                if (!(tempRedX == dotX && tempRedY == dotY)) {  
                    redX = tempRedX;
                    redY = tempRedY;
                }
            }
        }
        redX = constrain(redX, 0, 315);
        redY = constrain(redY, 0, 235);

        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.fillRect(dotX, dotY, 5, 5, BLUE); // Local Server Blue Dot
        M5.Lcd.fillRect(redX, redY, 5, 5, RED);  // Opponent's Red Dot

        String positionUpdate = String(dotX) + "-" + String(dotY);
        bleCharacteristic->setValue(positionUpdate.c_str());
        bleCharacteristic->notify();
    } else if (previouslyConnected) {
        drawScreenTextWithBackground("Disconnected. Restart M5 to reconnect.", TFT_RED);
    }

    delay(30); // Smooth movement update rate
}




///////////////////////////////////////////////////////////////
// BLE Server Broadcast Function
///////////////////////////////////////////////////////////////
void broadcastBleServer() {    
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new MyServerCallbacks());
    bleService = bleServer->createService(SERVICE_UUID);
    bleCharacteristic = bleService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );
    bleCharacteristic->setValue("Hello BLE World!");
    bleService->start();

    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(SERVICE_UUID);
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined... You can connect now!");
}

///////////////////////////////////////////////////////////////
// Screen Display Function
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println(text);
}
