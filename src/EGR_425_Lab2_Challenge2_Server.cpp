#include <M5Core2.h>
#include <Adafruit_seesaw.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool gameOverFlag = false;  // Flag to track if the game is over

// Gamepad Variables
Adafruit_seesaw gamepad;
#define BUTTON_START 16
#define BUTTON_SELECT 0

// Button state tracking
bool startButtonPressed = false;
bool selectButtonPressed = false;

// Server's Red Dot (Local)
int redX = 150, redY = 100, redSpeed = 1;

// Client's Blue Dot (Remote)
int blueX = -1, blueY = -1;  // Blue dot starts as invalid
bool blueDotInitialized = false;  // Flag to track if we've received valid data for blue dot

// Game timing
unsigned long gameStartTime = 0;
float gameTimeElapsed = 0.0;

// Collision threshold
const int COLLISION_DISTANCE = 10;

// Debug flags
bool debugMode = false;  // Set to true to display debug info

///////////////////////////////////////////////////////////////
// BLE UUIDs
///////////////////////////////////////////////////////////////
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// BLE Broadcast Name
#define BLE_BROADCAST_NAME "Mckaylas M5Core2024"

///////////////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor);
void sendGamepadData();
void gameOver();
void checkCollision();

///////////////////////////////////////////////////////////////
// BLE Server Callback
///////////////////////////////////////////////////////////////
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      gameOverFlag = false;
      blueDotInitialized = false;  // Reset this flag on new connection
      gameStartTime = millis();
      
      // Send a connected message to the client
      if (pCharacteristic) {
        pCharacteristic->setValue("CONNECTED");
        pCharacteristic->notify();
      }
      
      Serial.println("Device connected...");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected...");
    }
};

///////////////////////////////////////////////////////////////
// BLE Characteristic Callback
///////////////////////////////////////////////////////////////
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      
      if (value.length() > 0) {
        Serial.printf("Received Value: %s\n", value.c_str());
        
        String receivedData = String(value.c_str());
        int dashIndex = receivedData.indexOf('-');
        
        if (dashIndex > 0) {
          // Parse the position data
          int newBlueX = receivedData.substring(0, dashIndex).toInt();
          int newBlueY = receivedData.substring(dashIndex + 1).toInt();
          
          // Only update if values are valid (non-negative and within screen bounds)
          if (newBlueX >= 0 && newBlueY >= 0 && newBlueX < 320 && newBlueY < 240) {
            blueX = newBlueX;
            blueY = newBlueY;
            
            // Mark that we have valid data for the blue dot
            if (!blueDotInitialized) {
              blueDotInitialized = true;
              
              // Initialize red dot in a different area to avoid immediate collision
              if (abs(redX - blueX) < 50 && abs(redY - blueY) < 50) {
                redX = (blueX < 160) ? random(200, 300) : random(20, 120);
                redY = (blueY < 120) ? random(150, 220) : random(20, 90);
              }
            }
            
            // Check for collision after updating position, but only if we've had valid data for a while
            static unsigned long firstDataTime = 0;
            if (!firstDataTime) firstDataTime = millis();
            
            // Wait 2 seconds after first receiving data before allowing collisions
            if (!gameOverFlag && blueDotInitialized && (millis() - firstDataTime > 2000)) {
              checkCollision();
            }
          }
        }
      }
    }
};

///////////////////////////////////////////////////////////////
// Check for collision between dots
///////////////////////////////////////////////////////////////
void checkCollision() {
    // Only check collision if we have valid coordinates for blue dot
    if (blueX >= 0 && blueY >= 0 && !gameOverFlag && blueDotInitialized) {
        int dx = abs(redX - blueX);
        int dy = abs(redY - blueY);
        
        if (debugMode) {
            Serial.printf("Distance: dx=%d, dy=%d, threshold=%d\n", dx, dy, COLLISION_DISTANCE);
        }
        
        // Only trigger collision if dots are very close
        if (dx < COLLISION_DISTANCE && dy < COLLISION_DISTANCE) {
            if (debugMode) {
                Serial.println("COLLISION DETECTED");
            }
            gameOver();
        }
    }
}

///////////////////////////////////////////////////////////////
// Setup Function
///////////////////////////////////////////////////////////////
void setup() {
    M5.begin();
    M5.Lcd.setTextSize(3);
    drawScreenTextWithBackground("Starting BLE Server...", TFT_BLUE);

    // Initialize random seed
    randomSeed(analogRead(0));
    
    // Create the BLE Device
    BLEDevice::init(BLE_BROADCAST_NAME);

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ   |
                        BLECharacteristic::PROPERTY_WRITE  |
                        BLECharacteristic::PROPERTY_NOTIFY |
                        BLECharacteristic::PROPERTY_INDICATE
                      );

    // Add callback for receiving data from client
    pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

    // Create a BLE Descriptor (needed for notifications)
    pCharacteristic->addDescriptor(new BLE2902());

    // Start the service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // iPhone connection issue workaround
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.println("BLE Server started, waiting for connections...");
    
    // Display server status
    drawScreenTextWithBackground("BLE Server Ready\nWaiting for client...", TFT_GREEN);

    // Initialize Gamepad
    if (!gamepad.begin(0x50)) {
        Serial.println("ERROR! Gamepad not found.");
        drawScreenTextWithBackground("ERROR! Gamepad not found", TFT_RED);
        while (1);
    }
    gamepad.pinMode(BUTTON_START, INPUT_PULLUP);
    gamepad.pinMode(BUTTON_SELECT, INPUT_PULLUP);
    
    // Assign random positions for the red dot
    redX = random(50, 250);
    redY = random(50, 200);
}

///////////////////////////////////////////////////////////////
// Main Loop
///////////////////////////////////////////////////////////////
void loop() {
    if (gameOverFlag) {
        // If in game over state, just check for reset button (START)
        bool startCurrent = !gamepad.digitalRead(BUTTON_START);
        if (startCurrent && !startButtonPressed) {
            // Reset the game
            gameOverFlag = false;
            blueDotInitialized = false;
            redX = random(50, 250);
            redY = random(50, 200);
            redSpeed = 1;
            blueX = -1;
            blueY = -1;
            gameStartTime = millis();
            
            // Send CONNECTED to tell client to reset too
            if (deviceConnected && pCharacteristic) {
                pCharacteristic->setValue("CONNECTED");
                pCharacteristic->notify();
            }
        }
        startButtonPressed = startCurrent;
        delay(30);
        return;
    }

    // Handle connection/disconnection events
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("Client connected");
        delay(500); // Give time for connection to stabilize
    }
    
    if (!deviceConnected && oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        Serial.println("Client disconnected");
        // Restart advertising to allow reconnection
        pServer->startAdvertising();
        delay(500);
    }

    if (deviceConnected) {
        sendGamepadData();
        
        // Update game time
        gameTimeElapsed = (millis() - gameStartTime) / 1000.0;
    }

    delay(30); // Smooth movement update rate
}

///////////////////////////////////////////////////////////////
// Send Gamepad Data to BLE Client
///////////////////////////////////////////////////////////////
void sendGamepadData() {
    if (gameOverFlag) return;

    uint32_t buttons = gamepad.digitalReadBulk(0xFFFF);
    int joyX = 1023 - gamepad.analogRead(14);
    int joyY = 1023 - gamepad.analogRead(15);

    // Check Start button - Increase speed, wrap around from 5 to 1
    bool startCurrent = !gamepad.digitalRead(BUTTON_START); // Button is active LOW (pressed = 0)
    if (startCurrent && !startButtonPressed) {
        // Start button was just pressed (rising edge)
        redSpeed++;
        if (redSpeed > 5) redSpeed = 1;
        
        // Display speed briefly
        M5.Lcd.fillRect(0, 0, 100, 30, BLACK);
        M5.Lcd.setCursor(5, 5);
        M5.Lcd.setTextSize(2);
        M5.Lcd.printf("Speed: %d", redSpeed);
        M5.Lcd.setTextSize(3);
    }
    startButtonPressed = startCurrent;

    // Check Select button - Warp to random position
    bool selectCurrent = !gamepad.digitalRead(BUTTON_SELECT); // Button is active LOW
    if (selectCurrent && !selectButtonPressed) {
        // Select button was just pressed (rising edge)
        redX = random(10, 310);
        redY = random(10, 230);
    }
    selectButtonPressed = selectCurrent;

    float normX = (joyX - 512) / 512.0;
    float normY = (joyY - 512) / 512.0;

    if (abs(normX) > 0.2) redX += (normX > 0 ? redSpeed : -redSpeed);
    if (abs(normY) > 0.2) redY -= (normY > 0 ? redSpeed : -redSpeed);

    redX = constrain(redX, 0, 315);
    redY = constrain(redY, 0, 235);

    // Check for collision
    checkCollision();

    // Draw game screen
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillRect(redX, redY, 5, 5, RED);
    
    // Only draw blue dot if valid coordinates received
    if (blueX >= 0 && blueY >= 0) {
        M5.Lcd.fillRect(blueX, blueY, 5, 5, BLUE);
    }
    
    // Show game time and current speed
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.setTextSize(1);
    M5.Lcd.printf("Time: %.2fs  Speed: %d", gameTimeElapsed, redSpeed);
    M5.Lcd.setTextSize(3);

    // Send position to client
    String position = String(redX) + "-" + String(redY);
    if (pCharacteristic) {
        pCharacteristic->setValue(position.c_str());
        pCharacteristic->notify();
    }
}

///////////////////////////////////////////////////////////////
// Game Over Function
///////////////////////////////////////////////////////////////
void gameOver() {
    gameOverFlag = true;
    
    // Calculate final time
    gameTimeElapsed = (millis() - gameStartTime) / 1000.0;
    
    // Send game over to client with final time
    if (deviceConnected && pCharacteristic) {
        String gameOverMessage = "GAMEOVER-" + String(gameTimeElapsed);
        pCharacteristic->setValue(gameOverMessage.c_str());
        pCharacteristic->notify();
    }
    
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(3);
    
    M5.Lcd.setCursor(50, 100);
    M5.Lcd.print("GAME OVER");
    
    M5.Lcd.setCursor(50, 150);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("Time: %.2f seconds", gameTimeElapsed);
    
    M5.Lcd.setCursor(20, 200);
    M5.Lcd.setTextSize(1);
    M5.Lcd.print("Press START to play again");
}

///////////////////////////////////////////////////////////////
// Screen Display Function
///////////////////////////////////////////////////////////////
void drawScreenTextWithBackground(String text, int backgroundColor) {
    M5.Lcd.fillScreen(backgroundColor);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println(text);
}