// dimmerLed.cpp
#include "dimmerLed.h"

DimmerLed::DimmerLed() {}

void DimmerLed::setup(uint8_t ledPWMpin)
{
    dimLedValues.ledPWMpin = ledPWMpin;
    // if PIN not usable for PWM return
    if (ledPWMpin == 255 || ledPWMpin <= 1 || ledPWMpin == 3 || (ledPWMpin >= 6 && ledPWMpin <= 11) || ledPWMpin == 24 || ledPWMpin >= 34)
    {
        Serial.println("[pin " + String(dimLedValues.ledPWMpin) + "] is not usable for PWM");
        dimLedValues.ledPWMpin = 255;
        return;
    }
    pinMode(dimLedValues.ledPWMpin, OUTPUT);
    analogWriteResolution(12);
    analogWrite(dimLedValues.ledPWMpin, 0);

    Serial.println("DIMMERLED\t[pin " + String(dimLedValues.ledPWMpin) + "]");
}

// loop has to be called every 1 ms
void DimmerLed::loop()
{

    // every 0.1 second
    // if (loopCounter % 10 == 0)
    //    setBrightnessAuto();

    // every 0.5 second
    // if (loopCounter % 50 == 0)
    // {
    // drawScreen(version, time); // draw every 0.5 second
    // Serial.println("Displaying screen");
    // }

    // according to stepwith without delay
    if (dimLedValues.dimValueStepDelay == 0)
    {
        dimLedValues.dimValueRaw = calculateValue(dimLedValues.dimValueTarget, true);
    }
    // according to stepwith with delay
    else if (loopCounter % dimLedValues.dimValueStepDelay == 0)
    {
        uint16_t dimValueRaw = dimLedValues.dimValueRaw;
        uint16_t dimValueRawTarget = calculateValue(dimLedValues.dimValueTarget, true);

        if (dimValueRaw != dimValueRawTarget)
        {
            // Serial.println("[pin " + dimLedValues.ledPWMpin + "] change dimming .... to " + String(dimValueRawTarget) + " %" + " from " + String(dimValueRaw) + " % == ");
            if (dimValueRaw < dimValueRawTarget)
            {
                dimValueRaw += dimLedValues.dimValueStep;
                // Serial.print("[pin " + dimLedValues.ledPWMpin + "] new percentage++: " + String(dimValueRaw));
                if (dimValueRaw > dimValueRawTarget)
                    dimValueRaw = dimValueRawTarget;
            }
            else
            {
                dimValueRaw -= dimLedValues.dimValueStep;
                // Serial.print("[pin " + dimLedValues.ledPWMpin + "] new percentage--: " + String(dimValueRaw));
                if (dimValueRaw < dimValueRawTarget)
                    dimValueRaw = dimValueRawTarget;
            }
            // set new value
            dimLedValues.dimValueRaw = dimValueRaw;
            // Serial.println("-> new raw value: " + String(dimLedValues.dimValueRaw));
            if (dimLedValues.ledPWMpin != 255) // if pin is valid
                analogWrite(dimLedValues.ledPWMpin, dimLedValues.dimValueRaw);
            dimLedValues.inTransition = true;
        }
        else
        {
            dimLedValues.dimValueRawOld = dimValueRawTarget;
            dimLedValues.inTransition = false;
        }
    }

    // every 5 seconds
    // if (loopCounter % 500 == 0)

    // every 10 seconds
    // if (loopCounter % 1000 == 0)
    // {
    // checkNightMode();
    // Serial.println("[pin " + dimLedValues.ledPWMpin + "] current brightness: " + String(brightness));
    // }

    loopCounter++;
    if (loopCounter > 1200)
        loopCounter = 0; // after 1 minute restart

    // set main switch according to target value
    if (dimLedValues.dimValueRaw == 0)
        dimLedValues.mainSwitch = false;
    else
        dimLedValues.mainSwitch = true;
}

void DimmerLed::setDimValue(uint8_t dimValue)
{
    dimLedValues.dimValueRawOld = dimLedValues.dimValueRaw;
    dimLedValues.dimValueTarget = dimValue;
    dimLedValues.dimTargetStartTimestamp = millis();
}

void DimmerLed::setConfigValues(uint8_t dimValueStep, uint8_t dimValueStepDelay, uint16_t dimValueRangeLow, uint16_t dimValueRangeHigh)
{
    dimLedValues.dimValueStep = dimValueStep;
    dimLedValues.dimValueStepDelay = dimValueStepDelay;
    dimLedValues.dimValueRangeLow = dimValueRangeLow;
    // if not configured correctly set to default
    if (dimValueRangeHigh <= dimValueRangeLow || dimValueRangeHigh > 1023)
    {
        dimLedValues.dimValueRangeLow = 0;
        dimLedValues.dimValueRangeHigh = 1023;
    }
    else
    {
        dimLedValues.dimValueRangeLow = dimValueRangeLow;
        dimLedValues.dimValueRangeHigh = dimValueRangeHigh;
    }
}

uint8_t DimmerLed::getDimValue()
{
    dimLedValues.dimValue = calculateValue(dimLedValues.dimValueRaw, false);
    return dimLedValues.dimValue;
}

ledDimmerStruct DimmerLed::getDimmerValues()
{
    dimLedValues.dimValue = calculateValue(dimLedValues.dimValueRaw, false);
    return dimLedValues;
}

uint16_t DimmerLed::calculateValue(uint16_t inValue, boolean isRaw)
{
    if (isRaw)
    {
        // Serial.print("[pin " + dimLedValues.ledPWMpin + "]  - to raw value from: " + String(inValue));
        return map(inValue, 0, 100, dimLedValues.dimValueRangeLow, dimLedValues.dimValueRangeHigh);
    }
    else
    {
        uint16_t valuePercentRound = map(inValue, dimLedValues.dimValueRangeLow, dimLedValues.dimValueRangeHigh, 0, 1000);
        // Serial.println("[pin " + dimLedValues.ledPWMpin + "] raw to perc: " + String(valuePercentRound) + " got: " + String(inValue));
        valuePercentRound = round(float(valuePercentRound) / 10);
        return valuePercentRound;
    }
}