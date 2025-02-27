#include <M5Core2.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

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

// Dot position variables
int dotX = 150, dotY = 100;
int dotSpeed = 1;

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
}

///////////////////////////////////////////////////////////////
// Main Loop
///////////////////////////////////////////////////////////////
void loop() {
    if (deviceConnected) {
        std::string readValue = bleCharacteristic->getValue();
        Serial.printf("The new characteristic value as a STRING is: %s\n", readValue.c_str());
        String str = readValue.c_str();

        if (showMessage) {
            // Display the "Read from BLE client..." message for 1 second
            drawScreenTextWithBackground("Read from BLE client:\n\n" + str, TFT_GREEN);
            delay(1000);  // Hold message on screen for 1 second
            showMessage = false;  // Move to next screen after showing once
        } else {
            // Switch to dot movement display
            displayMovingDots();
        }
    } else if (previouslyConnected) {
        drawScreenTextWithBackground("Disconnected. Reset M5 device to reinitialize BLE.", TFT_RED);
        timer = 0;
    }

    delay(100);  // Adjust refresh rate for smooth transitions
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
