// dimmerLed.h
#ifndef DIMMERLED_H
#define DIMMERLED_H

#include <Arduino.h>

#ifndef ledDimmerStruct
struct ledDimmerStruct {
    boolean mainSwitch = false;
    uint16_t dimValueRaw = 0;               // 0-1024
    uint16_t dimValueRawOld = 0;            // 0-1024
    uint8_t dimValue = 0;                   // in percent 0-100
    uint8_t dimValueOld = 0;                // in percent 0-100 
    uint8_t dimValueTarget = 0;             // in percent 0-100
    uint8_t dimValueStep = 1;               // in percent 0-100
    uint8_t dimValueStepDelay = 3;          // in ms
    unsigned long dimTargetStartTimestamp = 0;    // in ms
    unsigned long dimValueStepTimerOld = 0; // in ms
    
    boolean inTransition = false;

    uint16_t dimValueRangeLow = 0;           // 0-1023
    uint16_t dimValueRangeHigh = 1023;       // 0-1023
    uint8_t ledPWMpin = 255;                 // init with invalid value
};
#endif

// Define the DimmerLed class
class DimmerLed {
    public:
        DimmerLed();
        void setup(uint8_t ledPWMpin);
        void setDimValue(uint8_t dimValue);
        void setConfigValues(uint8_t dimValueStep, uint8_t dimValueStepDelay, uint16_t dimValueRangeLow, uint16_t dimValueRangeHigh);
        uint8_t getDimValue();
        ledDimmerStruct getDimmerValues();
        void loop();

    private:
        uint16_t calculateValue(uint16_t inValue, boolean isRaw);
        struct ledDimmerStruct dimLedValues;
        uint16_t loopCounter = 0;
};

#endif // DIMMERLED_H
