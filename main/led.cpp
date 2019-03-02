#include <FastLED.h>

#include "led.hpp"

class LEDPrivate {
public:
    CRGB *leds;
};

uint8_t LED::mode = 0;
uint8_t LED::active_color = 0;
nvs_handle LED::led_nvs = NULL;
LEDPrivate* LED::d = new LEDPrivate;

/**
 * LED Program Initialisation
 */
void LED::setup(){
    esp_err_t err;

    // Load the LED NVS namespace
    err = nvs_open( "led", NVS_READWRITE, &led_nvs );
    ESP_ERROR_CHECK( err );

    // Initialise default colours and modes
    uint8_t a;
    err = nvs_get_u8( led_nvs, "a", &a );
    if( err == ESP_ERR_NVS_NOT_FOUND ){
        ESP_LOGW( "LED", "Initialising default colours." );

        a = 0;
        err = nvs_set_u8( led_nvs, "a", a );
        ESP_ERROR_CHECK( err );

        char key[2] = "0";
        for( uint8_t i = 0; i < 8; ++i ){
            key[0] = (char)(i + 48);
            switch( i ) {
                case 0: err = nvs_set_blob( led_nvs, key, (void*)CRGB::White, 3 ); break;
                case 1: err = nvs_set_blob( led_nvs, key, (void*)CRGB::Green, 3 ); break;
                case 2: err = nvs_set_blob( led_nvs, key, (void*)CRGB::Yellow, 3 ); break;
                case 3: err = nvs_set_blob( led_nvs, key, (void*)CRGB::Blue, 3 ); break;
                case 4: err = nvs_set_blob( led_nvs, key, (void*)CRGB::Red, 3 ); break;
                case 5: err = nvs_set_blob( led_nvs, key, (void*)CRGB::Teal, 3 ); break;
                case 6: err = nvs_set_blob( led_nvs, key, (void*)CRGB::Purple, 3 ); break;
                case 7: err = nvs_set_blob( led_nvs, key, (void*)CRGB::Orange, 3 ); break;
            }
            ESP_ERROR_CHECK( err );
        }
    }

    err = nvs_commit( led_nvs );
    ESP_ERROR_CHECK( err );

    d->leds = new CRGB[1];
    // FastLED.addLeds<P9813, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<P9813, 12, 14, RGB>( d->leds, 1 );
}

void LED::set( CRGB color ){
    d->leds[0] = color;
    FastLED.show();
}

void LED::run(){


    // int h;
    // while( true ){
    //     for( h = 0; h < 256; ++h ){
    //         d->leds[0] = CHSV( h, 255, 255);
    //         FastLED.show();
    //         vTaskDelay( 60 / portTICK_PERIOD_MS );;
    //     }
    // }
}
