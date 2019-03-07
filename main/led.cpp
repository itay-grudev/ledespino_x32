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
        CRGB colors[] = {
            CRGB::Red,
            CRGB::Yellow,
            CRGB( 235, 173, 42 ), // True yellow
            CRGB::Teal,
            CRGB::Blue,
            CRGB::Purple,
        };
        uint8_t i;
        double r, g, b;
        double dr, dg, db; // Color component steps
better_fade_start:
        better_fade_interrupt = false;

        r = leds[0].r;
        g = leds[0].g;
        b = leds[0].b;
        i = uint8_t(0) - 1;
        // TODO: find closest target color based on active color delta

        goto better_fade_start_color_init;

        while( true ){
            if( better_fade_interrupt ) goto better_fade_start;

            // Color target achieved
            if(
                (uint8_t)r == colors[i].r &&
                (uint8_t)g == colors[i].g &&
                (uint8_t)b == colors[i].b
            ) {
better_fade_start_color_init:
                // Loop the current color
                if( ++i >= sizeof( colors ) / sizeof( colors[0] ) - 1 ) i = 0;
                // TODO dr, dg, db
                if( r < colors[i].r ) dr = 0.5;
                else if( r > colors[i].r ) dr = -0.5;
                else dr = 0;
                if( g < colors[i].g ) dg = 0.5;
                else if( g > colors[i].g ) dg = -0.5;
                else dg = 0;
                if( b < colors[i].b ) db = 0.5;
                else if( b > colors[i].b ) db = -0.5;
                else db = 0;
            }

            // Move current color
            // 255 - 254.5 = 0.5 >= 0.5
            if( fabs( r - colors[i].r ) >= fabs( dr ) ) r += dr;
            if( fabs( g - colors[i].g ) >= fabs( dg ) ) g += dg;
            if( fabs( b - colors[i].b ) >= fabs( db ) ) b += db;

            // Display current color
            leds[0].r = r;
            leds[0].g = g;
            leds[0].b = b;
            FastLED.show();
            vTaskDelay( 10 / portTICK_PERIOD_MS );
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
                xTaskCreatePinnedToCore( &d->better_fade_task, "better_fade_task", 2000, NULL, 31, &d->better_fade_task_handle, 1 );
            }
            break;
    }

    active_mode = index;
}
