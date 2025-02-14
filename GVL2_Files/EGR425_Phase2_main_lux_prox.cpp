// #include <M5Core2.h>
// #include <Adafruit_VCNL4040.h>

// ///////////////////////////////////////////////////////////////
// // Variables
// ///////////////////////////////////////////////////////////////
// Adafruit_VCNL4040 vcnl4040 = Adafruit_VCNL4040();

// ///////////////////////////////////////////////////////////////
// // Put your setup code here, to run once
// ///////////////////////////////////////////////////////////////
// void setup() {

//     // Init device
//     M5.begin();

//     // Use the Adafruit library to initialize the sensor over I2C
//     Serial.println("Adafruit VCNL4040 Config demo");
//     if (!vcnl4040.begin())
//     {
//         Serial.println("Couldn't find VCNL4040 chip");
//         while (1) ; // Program ends in infinite loop...
//     }
//     Serial.println("Found VCNL4040 chip\n");
// }

// ///////////////////////////////////////////////////////////////
// // Put your main code here, to run repeatedly
// ///////////////////////////////////////////////////////////////
// void loop()
// {
//     // Library calls to get the sensor readings over I2C
//     Serial.printf("Proximity: %d\n", vcnl4040.getProximity());
//     Serial.printf("Ambient light: %d\n", vcnl4040.getLux());
//     Serial.printf("Raw white light: %d\n\n", vcnl4040.getWhiteLight());
//     delay(500);
// }
