#include <M5Unified.h>
#include <Adafruit_seesaw.h>

// Create the Gamepad Object
Adafruit_seesaw gamepad;

// Function Prototype (so loop can recognize it)
void gameOver();

//////////////////////////////////////
//  Gamepad Setup
//////////////////////////////////////
#define BUTTON_X 6
#define BUTTON_Y 2
#define BUTTON_A 5
#define BUTTON_B 1
#define BUTTON_SELECT 0
#define BUTTON_START 16

uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

//////////////////////////////////////
//  Gamepad Variables
//////////////////////////////////////
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 240;

int blueX = 50, blueY = 50, blueSpeed = 1; // Blue dot (joystick)
int redX = 250, redY = 150, redSpeed = 1; // Red dot (D-pad)
unsigned long startTime;
const int COLLISION_DISTANCE = 10;

//////////////////////////////////////
//  Setup Function
//////////////////////////////////////
void setup() {
    M5.begin();
    M5.Lcd.fillScreen(2);

    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("Gamepad QT Example!");

    // Initialize Gamepad QT
    if (!gamepad.begin(0x50)){
        Serial.println("ERROR! Gamepad not found");
        M5.Lcd.setTextColor(RED);
        M5.Lcd.setCursor(50, 100);
        M5.Lcd.print("Gamepad NOT found!");
        while (1);
    }

    Serial.println("Gamepad QT detected!");
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.setCursor(50, 100);
    M5.Lcd.print("Gamepad Connected!");

    // Verify correct firmware
    uint32_t version = ((gamepad.getVersion() >> 16) & 0xFFFF);
    if (version != 5743) {
        Serial.print("Wrong firmware? Version: ");
        Serial.println(version);
        while(1);
    }

    Serial.println("Found Product 5743");
    gamepad.pinModeBulk(button_mask, INPUT_PULLUP);
    gamepad.setGPIOInterrupts(button_mask, 1);

    // Start game timer
    startTime = millis();
}

//////////////////////////////////////
//  Main Game Loop
//////////////////////////////////////
void loop() {
    delay(10); // Reduce CPU usage

    uint32_t buttons = gamepad.digitalReadBulk(button_mask);
    int joyX = 1023 - gamepad.analogRead(14);
    int joyY = 1023 - gamepad.analogRead(15);

    float normX = (joyX - 512) / 512.0;
    float normY = (joyY - 512) / 512.0;

    if (abs(normX) > 0.2) blueX += (normX > 0 ? blueSpeed : -blueSpeed);
    if (abs(normY) > 0.2) blueY += (normY > 0 ? blueSpeed : -blueSpeed);

    if (!(buttons & (1UL << BUTTON_X))) redY -= redSpeed; // Up
    if (!(buttons & (1UL << BUTTON_B))) redY += redSpeed; // Down
    if (!(buttons & (1UL << BUTTON_Y))) redX -= redSpeed; // Left
    if (!(buttons & (1UL << BUTTON_A))) redX += redSpeed; // Right

    if (!(buttons & (1UL << BUTTON_START))) {
        blueSpeed = (blueSpeed % 5) + 1;
        delay(200);
    }

    blueX = constrain(blueX, 0, SCREEN_WIDTH -2);
    blueX = constrain(blueY, 0, SCREEN_WIDTH -2);
    blueX = constrain(redX, 0, SCREEN_WIDTH -2);
    blueX = constrain(redY, 0, SCREEN_WIDTH -2);

    int dotSize = 5;

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.fillRect(blueX, blueY, dotSize, dotSize, BLUE);
    M5.Lcd.fillRect(redX, redY, dotSize, dotSize, RED);

    int dx = abs(blueX - redX);
    int dy = abs(blueY - redY);
    if (dx < COLLISION_DISTANCE && dy < COLLISION_DISTANCE) {
        gameOver();
    }
}

//////////////////////////////////////
//  Game Over Function
//////////////////////////////////////
void gameOver() {
    unsigned long elapsedTime = millis() - startTime;
    float timeInSeconds = elapsedTime / 1000.0;

    M5.Lcd.fillScreen(RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(50, 100);
    M5.Lcd.setTextSize(3);
    M5.Lcd.print("GAME OVER");

    M5.Lcd.setCursor(50, 150);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("Time: %.2f sec", timeInSeconds);

    while(1); // Stop execution 
}