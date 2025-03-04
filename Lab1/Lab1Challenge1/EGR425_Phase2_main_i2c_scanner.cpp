#include <M5Unified.h>
#include <Adafruit_VCNL4040.h>

///////////////////////////////////////////////////////////////
// Variables
///////////////////////////////////////////////////////////////
Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();
int SDA_PIN = 32; // 21 internal; 32 port A
int SCL_PIN = 33; // 22 internal; 33 port A
int proximity;

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup()
{
    // Init device
    M5.begin();
    Wire.begin(SDA_PIN, SCL_PIN, 400000);
    Serial.begin(115200);

    // Start the speaker
    M5.Speaker.begin();
    M5.Speaker.setVolume(255);

    // Use the Adafruit library to initialize the sensor over I2C
    Serial.println("Adafruit VCNL4040 Config demo");
    if (!vcnl4040.begin(0x60))
    {
        Serial.println("Couldn't find VCNL4040 chip");
        while (1) ; // Program ends in infinite loop...
    }
    Serial.println("Found VCNL4040 chip\n");
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    int proximity = vcnl4040.getProximity();
    Serial.printf("Proximity: %d\n", proximity);

    // Map proximity (0-700) to frequency (200-2000Hz)
    int frequency = map(proximity, 0, 700, 200, 2000);
    frequency = constrain(frequency, 200, 2000);

    // Map proximity (0-700) to volume (100-255))
    int volume = map(proximity, 0, 700, 100, 1);
    volume = constrain(frequency, 100, 255);
    M5.Speaker.setVolume(volume);
    
    // Scale vibration intensity (0 - 255)
    int vibrationStrength = map(proximity, 0, 700, 100, 255);
    vibrationStrength = constrain(vibrationStrength, 100, 255);    
    M5.Power.setVibration(vibrationStrength);
    Serial.printf("Vibration Strength: %d\n", vibrationStrength);

    delay(100);

    Serial.printf("Playing Frequency: %d Hz\n", frequency);
    M5.Speaker.tone(frequency, 50);
}