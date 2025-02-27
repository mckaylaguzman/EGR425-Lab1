#include <M5Core2.h>
#include <Adafruit_seesaw.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

// BLE Definitions
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// BLE Variables
BLEServer *bleServer;
BLEService *bleService;
BLECharacteristic *bleCharacteristic;
bool deviceConnected = false;

// Gamepad Variables
Adafruit_seesaw gamepad;
#define BUTTON_START 16
#define BUTTON_SELECT 0

int blueX = 150, blueY = 100, blueSpeed = 2;  // Server's dot (local)
int redX = 0, redY = 0;  // Client's dot (received via BLE)

// Display feedback function
void drawScreenTextWithBackground(String text, int backgroundColor) {
  M5.Lcd.fillScreen(backgroundColor);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println(text);
}

// BLE Server Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("Device connected...");
    drawScreenTextWithBackground("Device connected!", TFT_GREEN);
  }
  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected...");
    drawScreenTextWithBackground("Disconnected.\nWaiting for client...", TFT_RED);
  }
};

// Initialize BLE
void initializeBLE() {
  Serial.println("Starting BLE...");
  drawScreenTextWithBackground("Initializing BLE...", TFT_CYAN);
  BLEDevice::init("M5Core2 Gamepad Server");

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new MyServerCallbacks());

  bleService = bleServer->createService(SERVICE_UUID);
  bleCharacteristic = bleService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY
  );
  bleCharacteristic->addDescriptor(new BLE2902());
  bleCharacteristic->setValue("Hello BLE World!");

  bleService->start();
  BLEAdvertising *bleAdvertising = BLEDevice::getAdvertising();
  bleAdvertising->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();

  Serial.println("BLE Server broadcasting...");
  drawScreenTextWithBackground("Broadcasting BLE Server...", TFT_BLUE);
}

// Initialize gamepad hardware
void initializeGamepad() {
  if (!gamepad.begin(0x50)) {
    Serial.println("ERROR! Gamepad not found.");
    drawScreenTextWithBackground("ERROR!\nGamepad not found.", TFT_RED);
    while (1);
  }
  gamepad.pinMode(BUTTON_START, INPUT_PULLUP);
  gamepad.pinMode(BUTTON_SELECT, INPUT_PULLUP);
}

void setup() {
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  Serial.begin(115200);

  initializeBLE();
  initializeGamepad();
}

void loop() {
  uint32_t buttons = gamepad.digitalReadBulk(0xFFFF);

  // Read joystick movement
  int joyX = 1023 - gamepad.analogRead(14);
  int joyY = 1023 - gamepad.analogRead(15);
  float normX = (joyX - 512) / 512.0;
  float normY = (joyY - 512) / 512.0;

  // Move Blue Dot Based on Speed
  if (abs(normX) > 0.2) blueX += (normX > 0 ? blueSpeed : -blueSpeed);
  if (abs(normY) > 0.2) blueY -= (normY > 0 ? blueSpeed : -blueSpeed);

  // Start Button: Adjust Speed (1 to 5)
  if (!(buttons & (1UL << BUTTON_START))) {
    blueSpeed = (blueSpeed % 5) + 1;
    Serial.printf("Speed changed to: %d\n", blueSpeed);
    delay(300);
  }

  // Select Button: Warp to Random Location
  if (!(buttons & (1UL << BUTTON_SELECT))) {
    blueX = random(0, 315);
    blueY = random(0, 235);
    Serial.printf("Warped to: (%d, %d)\n", blueX, blueY);
    delay(300);
  }

  // Receive client dot position
  if (deviceConnected) {
    std::string receivedData = bleCharacteristic->getValue();
    int separatorIndex = receivedData.find('-');
    if (separatorIndex != std::string::npos) {
      redX = std::stoi(receivedData.substr(0, separatorIndex));
      redY = std::stoi(receivedData.substr(separatorIndex + 1));
    }
  }

  // Display Local Blue Dot and Remote Red Dot
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.fillRect(blueX, blueY, 5, 5, BLUE);  // Local dot
  M5.Lcd.fillRect(redX, redY, 5, 5, RED);    // Client dot

  // Send local position to client
  if (deviceConnected) {
    String position = String(blueX) + "-" + String(blueY);
    bleCharacteristic->setValue(position.c_str());
    bleCharacteristic->notify();
  }

  delay(50);
}
