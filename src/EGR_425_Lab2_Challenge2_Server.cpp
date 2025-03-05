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
bool gameOverFlag = false; // Flag to track if game is over

// Gamepad Variables
Adafruit_seesaw gamepad;
#define BUTTON_START 16
#define BUTTON_SELECT 0

// Button debounce variables
unsigned long lastStartButtonPress = 0;
unsigned long lastSelectButtonPress = 0;
const int debounceDelay = 300;  // Milliseconds for button debounce

// Server's Blue Dot (Local)
int dotX = 150, dotY = 100;
int dotSpeed = 1;  // Starting speed
const int MAX_DOT_SPEED = 5;

// Opponent's Red Dot (Client-controlled)
int redX = -1, redY = -1;  // Start with invalid values

// Game timing
unsigned long gameStartTime = 0;
float gameTimeElapsed = 0.0;

// Collision threshold
const int COLLISION_DISTANCE = 10;

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
void gameOver();
void checkCollision();
void warpToRandomLocation();

///////////////////////////////////////////////////////////////
// BLE Server Callback Methods
///////////////////////////////////////////////////////////////
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
        previouslyConnected = true;
        showMessage = true;  // Reset flag to show the message when connecting
        gameOverFlag = false; // Reset game over state
        Serial.println("Device connected...");

        // Initialize game start time
        gameStartTime = millis();

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
// BLE Characteristic Callback Methods
///////////////////////////////////////////////////////////////
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        
        if (value.length() > 0) {
            Serial.println("Received from client: " + String(value.c_str()));
            
            // Parse X-Y coordinates from client
            String receivedData = String(value.c_str());
            int dashIndex = receivedData.indexOf('-');
            
            if (dashIndex > 0) {
                int tempRedX = receivedData.substring(0, dashIndex).toInt();
                int tempRedY = receivedData.substring(dashIndex + 1).toInt();
                
                // Update red dot position
                redX = tempRedX;
                redY = tempRedY;
                
                // Check for collision after updating position
                checkCollision();
            }
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

    // Initialize random seed for warp function
    randomSeed(analogRead(0));

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
    // If game is over, don't update the game state anymore
    // Just check for restart button input
    if (gameOverFlag) {
        // Check if both Start and Select buttons are pressed to restart
        if (!gamepad.digitalRead(BUTTON_START) && !gamepad.digitalRead(BUTTON_SELECT)) {
            // Reset the game
            gameOverFlag = false;
            if (deviceConnected) {
                gameStartTime = millis();
                redX = -1;
                redY = -1;
                dotSpeed = 1;  // Reset speed when restarting game
            }
        }
        
        // Don't proceed with the rest of the loop
        delay(100);
        return;
    }

    if (deviceConnected) {
        // Check for START button press to increase movement speed
        if (!gamepad.digitalRead(BUTTON_START) && (millis() - lastStartButtonPress > debounceDelay)) {
            lastStartButtonPress = millis();
            
            // Increment speed and wrap around if it exceeds max
            dotSpeed = (dotSpeed % MAX_DOT_SPEED) + 1;
            
            Serial.print("Speed changed to: ");
            Serial.println(dotSpeed);
        }
        
        // Check for SELECT button press to warp to random location
        if (!gamepad.digitalRead(BUTTON_SELECT) && (millis() - lastSelectButtonPress > debounceDelay)) {
            lastSelectButtonPress = millis();
            warpToRandomLocation();
        }
        
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

        // Check for collision
        checkCollision();

        // Update game time if still playing
        if (!gameOverFlag) {
            gameTimeElapsed = (millis() - gameStartTime) / 1000.0;
        }

        // Draw the game screen
        M5.Lcd.fillScreen(BLACK);
        M5.Lcd.fillRect(dotX, dotY, 5, 5, BLUE); // Local Server Blue Dot
        
        // Only draw red dot if valid coordinates received
        if (redX >= 0 && redY >= 0) {
            M5.Lcd.fillRect(redX, redY, 5, 5, RED);  // Opponent's Red Dot
        }
        
        // Show game time and current speed
        M5.Lcd.setCursor(5, 5);
        M5.Lcd.setTextSize(1);
        M5.Lcd.printf("Time: %.2fs   Speed: %d", gameTimeElapsed, dotSpeed);
        M5.Lcd.setTextSize(3);

        // Send our position to client
        String positionUpdate = String(dotX) + "-" + String(dotY);
        bleCharacteristic->setValue(positionUpdate.c_str());
        bleCharacteristic->notify();
    } else if (previouslyConnected && !gameOverFlag) {
        drawScreenTextWithBackground("Disconnected. Restart M5 to reconnect.", TFT_RED);
    }

    delay(30); // Smooth movement update rate
}

///////////////////////////////////////////////////////////////
// Warp to Random Location Function
///////////////////////////////////////////////////////////////
void warpToRandomLocation() {
    // Generate random position within the screen bounds
    dotX = random(5, 310);
    dotY = random(5, 230);
    
    Serial.print("Warped to position: ");
    Serial.print(dotX);
    Serial.print(", ");
    Serial.println(dotY);
}

///////////////////////////////////////////////////////////////
// Check for collision between dots
///////////////////////////////////////////////////////////////
void checkCollision() {
    // Only check collision if we have valid coordinates for red dot
    if (redX >= 0 && redY >= 0 && !gameOverFlag) {
        int dx = abs(dotX - redX);
        int dy = abs(dotY - redY);
        
        if (dx < COLLISION_DISTANCE && dy < COLLISION_DISTANCE) {
            gameOver();
        }
    }
}

///////////////////////////////////////////////////////////////
// Game Over Function
///////////////////////////////////////////////////////////////
void gameOver() {
    gameOverFlag = true;
    gameTimeElapsed = (millis() - gameStartTime) / 1000.0;
    
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(3);
    
    M5.Lcd.setCursor(50, 100);
    M5.Lcd.print("GAME OVER");
    
    M5.Lcd.setCursor(50, 150);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("Time: %.2f seconds", gameTimeElapsed);
    
    // Add instructions for restarting
    M5.Lcd.setCursor(20, 200);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("Press START+SELECT together to play again");
    
    // Optional: Send game over notification to client
    String gameOverMsg = "GAMEOVER-" + String(gameTimeElapsed, 2);
    bleCharacteristic->setValue(gameOverMsg.c_str());
    bleCharacteristic->notify();
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
    bleCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
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