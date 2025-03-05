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
bool gameOverFlag = false;  // Flag to track if the game is over

// Gamepad Variables
Adafruit_seesaw gamepad;
#define BUTTON_START 16
#define BUTTON_SELECT 0

// Client's Blue Dot (Local)
int blueX = 150, blueY = 100, blueSpeed = 1;
bool showGameScreen = false;  // Flag to control screen transition

// Server's Red Dot (Remote)
int redX = -1, redY = -1;  // Red dot starts as invalid

// Game timing
unsigned long gameStartTime = 0;
float gameTimeElapsed = 0.0;

// Collision threshold
const int COLLISION_DISTANCE = 10;

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
void gameOver(float timeElapsed = -1.0);  // Game Over function
void checkCollision();

///////////////////////////////////////////////////////////////
// BLE Client Callback Methods (Handles Server Notifications)
///////////////////////////////////////////////////////////////
static void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
    Serial.printf("Notify callback for characteristic %s of data length %d\n", 
                  pBLERemoteCharacteristic->getUUID().toString().c_str(), length);
    Serial.printf("\tData: %s\n", (char *)pData);

    String receivedData = String((char *)pData);

    if (receivedData == "CONNECTED") {
        showGameScreen = true;
        gameStartTime = millis(); // Start timing when connection is established
        Serial.println("Switching to game screen.");
    } 
    else if (receivedData.startsWith("GAMEOVER")) {
        // Parse server's game over time if available
        int dashIndex = receivedData.indexOf('-');
        if (dashIndex > 0) {
            float serverTime = receivedData.substring(dashIndex + 1).toFloat();
            gameOver(serverTime);
        } else {
            gameOver();
        }
    }
    else {
        int dashIndex = receivedData.indexOf('-');
        if (dashIndex > 0) {
            redX = receivedData.substring(0, dashIndex).toInt();
            redY = receivedData.substring(dashIndex + 1).toInt();
            
            // Check for collision after updating position
            if (!gameOverFlag) {
                checkCollision();
            }
        }
    }
}

///////////////////////////////////////////////////////////////
// Check for collision between dots
///////////////////////////////////////////////////////////////
void checkCollision() {
    // Only check collision if we have valid coordinates for red dot
    if (redX >= 0 && redY >= 0 && !gameOverFlag) {
        int dx = abs(blueX - redX);
        int dy = abs(blueY - redY);
        
        if (dx < COLLISION_DISTANCE && dy < COLLISION_DISTANCE) {
            gameOver();
        }
    }
}

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods (Handles Connection/Disconnection)
///////////////////////////////////////////////////////////////
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient *pclient) {
        deviceConnected = true;
        gameOverFlag = false;
        gameStartTime = millis();
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
        showGameScreen = true;
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

    showGameScreen = true;
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

    // Assign random positions for the blue dot
    blueX = random(50, 250);
    blueY = random(50, 200);

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
    if (gameOverFlag) return; // Stop the game loop if game over

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

    if (deviceConnected && showGameScreen && !gameOverFlag) {
        sendGamepadData();
        
        // Update game time
        gameTimeElapsed = (millis() - gameStartTime) / 1000.0;
    } else if (doScan && !gameOverFlag) {
        drawScreenTextWithBackground("Disconnected... re-scanning for BLE server...", TFT_ORANGE);
        BLEDevice::getScan()->start(0);
    }

    delay(30); // Smooth movement update rate
}

///////////////////////////////////////////////////////////////
// Send Gamepad Data to BLE Server
///////////////////////////////////////////////////////////////
void sendGamepadData() {
    if (gameOverFlag) return;

    uint32_t buttons = gamepad.digitalReadBulk(0xFFFF);
    int joyX = 1023 - gamepad.analogRead(14);
    int joyY = 1023 - gamepad.analogRead(15);

    float normX = (joyX - 512) / 512.0;
    float normY = (joyY - 512) / 512.0;

    if (abs(normX) > 0.2) blueX += (normX > 0 ? blueSpeed : -blueSpeed);
    if (abs(normY) > 0.2) blueY -= (normY > 0 ? blueSpeed : -blueSpeed);

    blueX = constrain(blueX, 0, 315);
    blueY = constrain(blueY, 0, 235);

    // Check for collision
    checkCollision();

    // Draw game screen
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillRect(blueX, blueY, 5, 5, BLUE);
    
    // Only draw red dot if valid coordinates received
    if (redX >= 0 && redY >= 0) {
        M5.Lcd.fillRect(redX, redY, 5, 5, RED);
    }
    
    // Show game time
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.setTextSize(1);
    M5.Lcd.printf("Time: %.2fs", gameTimeElapsed);
    M5.Lcd.setTextSize(3);

    // Send position to server
    String position = String(blueX) + "-" + String(blueY);
    if (bleRemoteCharacteristic && bleRemoteCharacteristic->canWrite()) {
        bleRemoteCharacteristic->writeValue(position.c_str(), position.length());
    }
}

///////////////////////////////////////////////////////////////
// Game Over Function
///////////////////////////////////////////////////////////////
void gameOver(float serverTime) {
    gameOverFlag = true;
    
    // Calculate time if not provided by server
    if (serverTime < 0) {
        gameTimeElapsed = (millis() - gameStartTime) / 1000.0;
    } else {
        // Use server's time if provided (for consistency)
        gameTimeElapsed = serverTime;
    }
    
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(3);
    
    M5.Lcd.setCursor(50, 100);
    M5.Lcd.print("GAME OVER");
    
    M5.Lcd.setCursor(50, 150);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("Time: %.2f seconds", gameTimeElapsed);
}

///////////////////////////////////////////////////////////////
// Screen Display Function
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println(text);
}