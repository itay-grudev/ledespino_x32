#ifndef LED_H
#define LED_H

#define FASTLED_ALLOW_INTERRUPTS 0

#include <nvs.h>
#include <FastLED.h>

class LEDPrivate;

class LED {
public:
    static void setup();
    static void showColor( CRGB );
    static void setActiveColor( uint8_t, bool = true );
    static void setActiveMode( uint8_t );
    static void setColor( uint8_t, CRGB );

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
