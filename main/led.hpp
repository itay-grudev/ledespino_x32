#ifndef LED_H
#define LED_H

#include <nvs.h>
#include <FastLED.h>

class LEDPrivate;

class LED {
public:
    static void setup();
    static void set( CRGB );
    static void run();

    static uint8_t active_mode;
    static uint8_t active_color;

    static nvs_handle led_nvs;
private:
    static LEDPrivate *d;
};

#endif // LED_H


#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

class LEDController {
public:
};

#endif // LED_CONTROLLER_H
