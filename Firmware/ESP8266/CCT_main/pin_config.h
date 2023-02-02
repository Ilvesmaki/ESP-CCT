#ifndef __PIN_CONFIG__H__
#define __PIN_CONFIG__H__

#define CW_LED_PIN 4
#define WW_LED_PIN 12
#define BUTTON_PIN 5            /* Active LOW */
#define CONFIG_BUTTON_PIN 14    /* Active LOW */
#define PIR_PIN 13              /* Active HIGH */

#define ANALOG_MAX_VAL 1023

void setupPins()
{
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(CW_LED_PIN, OUTPUT);
    pinMode(WW_LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);
    pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

    analogWriteRange(ANALOG_MAX_VAL);
}

#endif  //!__PIN_CONFIG__H__