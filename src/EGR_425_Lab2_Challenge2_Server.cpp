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
int timer = 0;
bool showMessage = true;  // Flag to show BLE message before switching to dot display

// Gamepad Variables
Adafruit_seesaw gamepad;
#define BUTTON_START 16
#define BUTTON_SELECT 0

// Dot position variables
int dotX = 150, dotY = 100;
int dotSpeed = 1;

// Opponent's Red Dot Position
int redX = 150, redY = 100;  // Default starting position

///////////////////////////////////////////////////////////////
// BLE UUIDs
///////////////////////////////////////////////////////////////
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// BLE Broadcast Name
static String BLE_BROADCAST_NAME = "Mckaylas M5Core2024";

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods
///////////////////////////////////////////////////////////////
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        previouslyConnected = true;
        showMessage = true;  // Reset flag when a new client connects
        Serial.println("Device connected...");

        // Send initial signal to client to transition to game screen
        bleCharacteristic->setValue("CONNECTED");
        bleCharacteristic->notify();  // <---- Add this line
    }

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected...");
    }
};

///////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////
void broadcastBleServer();
void drawScreenTextWithBackground(String text, int backgroundColor);
void displayMovingDots();

///////////////////////////////////////////////////////////////
// Setup Function
///////////////////////////////////////////////////////////////
void setup() {
    M5.begin();
    M5.Lcd.setTextSize(3);

    // Initialize M5Core2 as a BLE server
    Serial.print("Starting BLE...");
    BLEDevice::init(BLE_BROADCAST_NAME.c_str());

    // Broadcast the BLE server
    drawScreenTextWithBackground("Initializing BLE...", TFT_CYAN);
    broadcastBleServer();
    drawScreenTextWithBackground("Broadcasting as BLE server named:\n\n" + BLE_BROADCAST_NAME, TFT_BLUE);

    // Initialize Gamepad
    if (!gamepad.begin(0x50)) {
        Serial.println("ERROR Gamepad not found");
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
        std::string readValue = bleCharacteristic->getValue();
        Serial.printf("The new characteristic value as a STRING is: %s\n", readValue.c_str());

        // Only update red dot if we actually received data
        if (!readValue.empty()) {
            int commaIndex = String(readValue.c_str()).indexOf('-');
            if (commaIndex > 0) {
                redX = String(readValue.c_str()).substring(0, commaIndex).toInt();
                redY = String(readValue.c_str()).substring(commaIndex + 1).toInt();
            }
        }

        // Read joystick input
        uint32_t buttons = gamepad.digitalReadBulk(0xFFFF);
        int joyX = 1023 - gamepad.analogRead(14);
        int joyY = 1023 - gamepad.analogRead(15);

        float normX = (joyX - 512) / 512.0;
        float normY = (joyY - 512) / 512.0;

        if (abs(normX) > 0.2) dotX += (normX > 0 ? dotSpeed : -dotSpeed);
        if (abs(normY) > 0.2) dotY -= (normY > 0 ? dotSpeed : -dotSpeed);

        dotX = constrain(dotX, 0, 315);
        dotY = constrain(dotY, 0, 235);

        // Draw both dots
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.fillRect(dotX, dotY, 5, 5, BLUE); // Local Blue Dot
        M5.Lcd.fillRect(redX, redY, 5, 5, RED);  // Opponent's Red Dot

        // Send position to client
        String positionUpdate = String(dotX) + "-" + String(dotY);
        bleCharacteristic->setValue(positionUpdate.c_str());
        bleCharacteristic->notify();
    }

    delay(50); // Smooth update rate
}

///////////////////////////////////////////////////////////////
// Colors the background and then writes the text on top
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println(text);
}

///////////////////////////////////////////////////////////////
// Moving Dots Display
///////////////////////////////////////////////////////////////
void displayMovingDots() {
    // Simulate random dot movement
    dotX += (random(0, 2) ? dotSpeed : -dotSpeed);
    dotY += (random(0, 2) ? dotSpeed : -dotSpeed);

    // Keep dot within screen bounds
    dotX = constrain(dotX, 0, 315);
    dotY = constrain(dotY, 0, 235);

    // Display moving dots
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillRect(dotX, dotY, 5, 5, BLUE);
    delay(50);  // Adjust refresh rate for smooth movement
}

///////////////////////////////////////////////////////////////
// BLE Server Broadcast Function
///////////////////////////////////////////////////////////////
void broadcastBleServer() {    
    // Initializing the server, a service and a characteristic 
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
    bleCharacteristic->setValue("Hello BLE World from Dr. Dan!");
    bleService->start();

    // Start advertising BLE service
    BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
    bleAdvertising->addServiceUUID(SERVICE_UUID);
    bleAdvertising->setScanResponse(true);
    bleAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined...you can connect with your phone!"); 
}
