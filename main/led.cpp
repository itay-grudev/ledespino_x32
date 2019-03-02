#include "led.hpp"

#include <esp_ipc.h>

class LEDPrivate {
public:
    static CRGB *leds;

    static TaskHandle_t hsv_fade_task_handle;
    static bool hsv_fade_interrupt;
    static void hsv_fade_task( void* pvParameters ){
        CHSV hsv = CHSV( 0, 255, 255 );

hsv_fade_start:
        hsv_fade_interrupt = false;

        // Move to the red
        while( leds[0].r < 255 || leds[0].g > 0 || leds[0].b > 0 ){
            if( leds[0].r < 255 ) ++leds[0].r;
            if( leds[0].g > 0 ) --leds[0].g;
            if( leds[0].b > 0 ) --leds[0].b;
            FastLED.show();
            vTaskDelay( 20 / portTICK_PERIOD_MS );
        }

        // Animate HSV
        while( true ){
            for( hsv.h = 0 ; hsv.h < 192 ; ++hsv.h ){
                if( hsv_fade_interrupt ) goto hsv_fade_start;
                hsv2rgb_raw( hsv, leds[0] );
                // ESP_LOGI("FADE", "%d %d %d", leds[0].r, leds[0].g, leds[0].b);
                FastLED.show();
                vTaskDelay( 90 / portTICK_PERIOD_MS );
            }
        }
    }

    static TaskHandle_t better_fade_task_handle;
    static bool better_fade_interrupt;
    static void better_fade_task( void* pvParameters ){
        CHSV hsv = CHSV( 0, 255, 255 );

better_fade_start:
        better_fade_interrupt = false;

        // Move to the red
        while( leds[0].r < 255 || leds[0].g > 0 || leds[0].b > 0 ){
            if( leds[0].r < 255 ) ++leds[0].r;
            if( leds[0].g > 0 ) --leds[0].g;
            if( leds[0].b > 0 ) --leds[0].b;
            FastLED.show();
            vTaskDelay( 20 / portTICK_PERIOD_MS );
        }

        // Animate HSV
        while( true ){
            for( hsv.h = 0 ;; ++hsv.h ){
                if( better_fade_interrupt ) goto better_fade_start;
                hsv2rgb_rainbow( hsv, leds[0] );
                // ESP_LOGI("FADE", "%d %d %d", leds[0].r, leds[0].g, leds[0].b);
                FastLED.show();
                vTaskDelay( 90 / portTICK_PERIOD_MS );
            }
        }
    }
};

uint8_t LED::active_mode = 0;
uint8_t LED::active_color = 0;
nvs_handle LED::led_nvs = NULL;
CRGB* LEDPrivate::leds = NULL;

bool LEDPrivate::hsv_fade_interrupt = false;
TaskHandle_t LEDPrivate::hsv_fade_task_handle = NULL;
bool LEDPrivate::better_fade_interrupt = false;
TaskHandle_t LEDPrivate::better_fade_task_handle = NULL;

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
    FastLED.setCorrection( CRGB( 255, 210, 220 ) );
}

void LED::showColor( CRGB color ){
    d->leds[0] = color;
    FastLED.show();
}

void LED::setActiveColor( uint8_t i, bool save ){
    esp_err_t err;
    size_t rgb_size = 3;
    char key[2] = "0";
    key[0] = (char)(i + 48);
    CRGB rgb;
    err = nvs_get_blob( LED::led_nvs, key, &rgb, &rgb_size );
    ESP_ERROR_CHECK( err );
    active_color = i;
    showColor( rgb );
}

void LED::setColor( uint8_t i, CRGB color ){
    showColor( color );
}

void LED::setActiveMode( uint8_t index ){
    if( d->hsv_fade_task_handle )
        vTaskSuspend( d->hsv_fade_task_handle );
    if( d->better_fade_task_handle )
        vTaskSuspend( d->better_fade_task_handle );
    switch( index ){
        case 0:
            showColor( CRGB::Black );
            break;
        case 1:
            setActiveColor( active_color, false );
            break;
        case 2:
            if( d->hsv_fade_task_handle ){
                if( active_mode != index ) d->hsv_fade_interrupt = true;
                vTaskResume( d->hsv_fade_task_handle );
            } else {
                xTaskCreatePinnedToCore( &d->hsv_fade_task, "hsv_fade_task", 500, NULL, 31, &d->hsv_fade_task_handle, 1 );
            }
            break;
        case 3:
            if( d->better_fade_task_handle ){
                if( active_mode != index ) d->better_fade_interrupt = true;
                vTaskResume( d->better_fade_task_handle );
            } else {
                xTaskCreatePinnedToCore( &d->better_fade_task, "better_fade_task", 500, NULL, 31, &d->better_fade_task_handle, 1 );
            }
            break;
    }

    active_mode = index;
}
