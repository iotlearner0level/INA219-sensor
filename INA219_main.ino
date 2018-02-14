///
/// @mainpage	INA219_main
///
/// @details	Basic implementation of the INA219
/// @n
/// @n
/// @n @a	Developed with [embedXcode](http://embedXcode.weebly.com)
///
/// @author	Rei VILO
/// @author	embedXcode.weebly.com
/// @date	Jun 06, 2013
/// @version	206
///
/// @copyright	© Rei VILO, 2013
/// @copyright	CC = BY NC SA
/// http://creativecommons.org/licenses/by-nc-sa/3.0/
///
/// @n  You are free:
/// * to Share — to copy, distribute and transmit the work
/// * to Remix — to adapt the work
///
/// @n  Under the following conditions:
/// *   Attribution — You must attribute the work in the manner specified by the author or licensor (but not in any way that suggests that they endorse you or your use of the work).
/// *   Noncommercial — You may not use this work for commercial purposes.
/// *   Share Alike — If you alter, transform, or build upon this work, you may distribute the resulting work only under the same or similar license to this one.
///
///
/// @see	ReadMe.txt for references
///


///
/// @file	INA219_main.ino
/// @brief	Main sketch
///
/// @details	Basic implementation of the INA219
/// @n @a	Developed with [embedXcode](http://embedXcode.weebly.com)
///
/// @author	Rei VILO
/// @author	embedXcode.weebly.com
/// @date	Jun 06, 2013
/// @version	206
///
/// @copyright	© Rei VILO, 2013
/// @copyright	CC = BY NC SA
///
/// @see	ReadMe.txt for references
/// @n
///


// Core library for code-sense
#if defined(WIRING) // Wiring specific
#include "Wiring.h"
#elif defined(MAPLE_IDE) // Maple specific
#include "WProgram.h"
#elif defined(MPIDE) // chipKIT specific
#include "WProgram.h"
#elif defined(DIGISPARK) // Digispark specific
#include "Arduino.h"
#elif defined(ENERGIA) // LaunchPad, FraunchPad and StellarPad specific
#include "Energia.h"
#elif defined(CORE_TEENSY) // Teensy specific
#include "WProgram.h"
#elif defined(ARDUINO) && (ARDUINO >= 100) // Arduino 1.0 and 1.5 specific
#include "Arduino.h"
#elif defined(ARDUINO) && (ARDUINO < 100) // Arduino 23 specific
#include "WProgram.h"
#else // error
#error Platform not defined
#endif


// Include application, user and local libraries
#ifndef EMBEDXCODE
#include "Wire.h"
#endif

#include "INA219_library.h"


///
/// @brief  Constants and variables
///
/// @{
INA219 myINA219;
uint16_t valueV, valueA, valueW, minV, minA, minW, maxV, maxA, maxW;
uint8_t fsm = 6;
bool unit = true;
uint32_t chrono1, chrono2, chrono3;
/// @}

///
/// @brief	Update minimum and maximum
/// @param	value value, uint16_t
/// @param	min minimum, uint16_t
/// @param	max maximum, uint16_t
/// @param	reset force reset of min and max with min=max=value
///
void setMinMax(uint16_t value, uint16_t &min, uint16_t &max, bool reset=false)
{
    if (reset) {
        min = value;
        max = value;
    } else {
        if (value < min) min = value;
        if (value > max) max = value;
    }
}

///
/// @brief	Convert number into text
/// @param	number value, uint16_t
/// @param	unit LOW=FALSE=mU 00000, HIGH=TRUE=U 0.000
/// @param	size length of final text
/// @return	formated text
///
String ui16toa(int32_t number, bool unit, uint8_t size) {
    String text = "";
    uint8_t i;
    
    if (unit == LOW) {
        text = String(number);
        
    } else {
        i = 0;
        while ((number != 0) || (i<4)) {
            text = String("0123456789")[number % 10] + text;
            number /= 10;
            i++;
            if (i==3) text = "." + text;
        }
    }
    if (text.length() > size) text = text.substring(0, size);
    while (text.length() < size) text = " " + text;
    
    return text;
}

///
/// @brief	Setup
///
// Add setup code
void setup() {
    Serial.begin(9600);
    Wire.begin();
    delay(100);
    
    Serial.println("\n\nV.A.W with INA219+MSP320");
    Serial.println("----");
    Serial.println("PUSH2 for commands\n");
    
    myINA219.begin();
    
    pinMode(PUSH2, INPUT_PULLUP);
}

///
/// @brief	Loop
///
// Add loop code
void loop() {
    
    myINA219.get();
    
    valueV = myINA219.mV();
    valueA = myINA219.mA();
    valueW = myINA219.mW();
    
    setMinMax(valueV, minV, maxV, (fsm == 6));
    setMinMax(valueA, minA, maxA, (fsm == 6));
    setMinMax(valueW, minW, maxW, (fsm == 6));
    
    switch (fsm) {
        case 0:
        case 1:
        case 2:
            Serial.print(ui16toa(valueV, unit, 5)); // mV valueV
            Serial.print("\t");
            Serial.print(ui16toa(minV, unit, 5)); // mV minV
            Serial.print("\t");
            Serial.print(ui16toa(maxV, unit, 5)); // mV maxV
            Serial.print("\t:\t");
            
            Serial.print(ui16toa(valueA, unit, 5)); // mA valueA
            Serial.print("\t");
            Serial.print(ui16toa(minA, unit, 5)); // mA minA
            Serial.print("\t");
            Serial.print(ui16toa(maxA, unit, 5)); // mA maxA
            Serial.print("\t:\t");
            
            Serial.print(ui16toa(valueW, unit, 5)); // mW valueW
            Serial.print("\t");
            Serial.print(ui16toa(minW, unit, 5)); // mW minW
            Serial.print("\t");
            Serial.print(ui16toa(maxW, unit, 5)); // mW maxW
            Serial.println("\t");
            break;
            
        case 5:
            unit = !unit;
            // 5 continues with 6
            
        case 6:
            if (unit) {
                Serial.println("    V\t  min\t  max\t:\t    A\t  min\t  max\t:\t    W\t  min\t  max");
            } else {
                Serial.println("   mV\t  min\t  max\t:\t   mA\t  min\t  max\t:\t   mW\t  min\t  max");
            }
            break;
            
        case 7:
            Serial.println("====");
            Serial.end();
            while (true);
            break;
            
        default:
            break;
    }
    
    fsm += 1;
    fsm %= 3;
    
    if (!digitalRead(PUSH2)) {
        fsm = 4;
        chrono1 = millis() + 1000;
        chrono2 = chrono1 + 1000;
        chrono3 = chrono2 + 1000;
        
        while (!digitalRead(PUSH2)) {
            
            if (millis() > chrono3) {
                if (fsm == 7) {
                    Serial.print("> CANCEL ");
                    fsm = 8;
                }
            } else if (millis() > chrono2) {
                if (fsm == 6) {
                    Serial.print("> OFF ");
                    fsm = 7;
                }
            } else if (millis() > chrono1) {
                if (fsm == 5) {
                    Serial.print("> RESET ");
                    fsm = 6;
                }
            } else {
                if (fsm == 4) {
                    Serial.print("> Unit/mUnit ");
                    fsm = 5;
                }
            }
        }
        Serial.println("> OK ");
    }
    
    delay(1000);
}
