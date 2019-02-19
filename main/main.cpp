/**
 * Ledespino_x32 is an advanced LED strip controller based on the ESP32 WiFi
 * enabled IoT microcontroller.
 * Copyright: Itay Grudev 2019
 * License: GNU GPLv3
 */

// NOTES: VALIDATE SSID DOES NOT EXCEED 31 chars excluding \0 (32 in total)
// NOTES: VALIDATE PASS DOES NOT EXCEED 63 chars excluding \0 (64 in total)

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <mdns.h>

extern "C" {
    void app_main(void);
}

#include "nvs.hpp"
#include "constants.hpp"
#include "http_response_handlers.hpp"

// API call return code. Global for convinience only.
esp_err_t err;

// Current WiFi configuration
wifi_config_t wifi_config = {};

// System NVS
nvs_handle system_nvs;

// LED NVS
nvs_handle led_nvs;

// Effect stages
uint8_t mode = 0;

// Front End Initial colours
const char white[3]  = {255, 255, 255};
const char yellow[3] = {255, 255, 0};
const char green[3] = {0, 232, 66};
const char blue[3] = {0, 174, 255};
const char red[3] = {255, 0, 66};
const char teal[3] = {0, 128, 128};
const char lilac[3] = {128, 0, 128};
const char orange[3] = {255, 127, 0};

httpd_handle_t start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_HTTP_SERVER_PORT;

    // Start the httpd server
    ESP_LOGW( "SYS", "Starting HTTP server on port: '%d'", config.server_port );
    httpd_handle_t server;

    mdns_service_add( NULL, "_http", "_tcp", config.server_port, NULL, 0 );

    if( httpd_start( &server, &config ) == ESP_OK) {
            // Set URI handlers
            ESP_LOGI( "SYS", "Registering URI handlers" );
            httpd_register_uri_handler( server, &http_root_uri );
            httpd_register_uri_handler( server, &http_status_uri );
            httpd_register_uri_handler( server, &http_set_uri );
            return server;
    }

    ESP_LOGW( "SYS", "Error starting HTTP server!" );

    return NULL;
}

static esp_err_t event_loop_handler( void *ctx, system_event_t *event ){
    httpd_handle_t *server = (httpd_handle_t *)ctx;

    switch( event->event_id ){
        case SYSTEM_EVENT_STA_START:
            ESP_LOGW( "SYS", "WiFi Mode: STA" );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGW( "SYS", "Connected to: %s", wifi_config.sta.ssid );
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGW(
                "SYS", "IP Address: '%s'",
                ip4addr_ntoa( &event->event_info.got_ip.ip_info.ip )
            );

            // Start the web server
            if( *server == NULL ) {
                *server = start_webserver();
            }
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGW( "SYS", "WiFi Disconnected. Attempting reconnect." );
            ESP_ERROR_CHECK( esp_wifi_connect() );

            // Stop the webserver
            if( *server ) {
                httpd_stop( *server );
                *server = NULL;
            }
            break;
        default:
            break;
    }

    return ESP_OK;
}

void app_main()
{
    ESP_LOGW( "SYS", "Starting up." );
    /**
     * Non-volatile storage (NVS) initialisation
     */
    err = nvs_flash_init();

    // Recoverable NVS problems
    switch( err ){
        case ESP_ERR_NVS_NO_FREE_PAGES:
        case ESP_ERR_NVS_NEW_VERSION_FOUND:
            // Erase and re-initialise the NVS partition
            ESP_ERROR_CHECK( nvs_flash_erase() );
            err = nvs_flash_init();
    }

    ESP_ERROR_CHECK( err );

    // Load the system NVS namespace
    err = nvs_open( "sys", NVS_READWRITE, &system_nvs );
    ESP_ERROR_CHECK( err );

    /**
     * mDNS Initialisation
     */
    ESP_ERROR_CHECK( mdns_init() );

    // Load the device hostname and instanc name
    char *hostname = NULL, *instance_name = NULL;
    err = nvs_get_set_str_default( system_nvs, "hostname", hostname, CONFIG_DEFAULT_HOSTNAME );
    ESP_ERROR_CHECK( err );
    err = nvs_get_set_str_default( system_nvs, "instance_name", instance_name, CONFIG_DEFAULT_INSTANCE_NAME );

    ESP_LOGW( "SYS", "Hostname: %s", hostname );

    // Set the mDNS hostname
    ESP_ERROR_CHECK(
        mdns_hostname_set( hostname )
    );

    // Set the default instance name
    ESP_ERROR_CHECK(
        mdns_instance_name_set( instance_name )
    );

    /**
     * WiFi Initialisation
     */
    WiFiMode mode;

    #ifdef CONFIG_DEFAULT_WIFI_MODE_AP
        err = nvs_get_set_u8_default( system_nvs, "wifi_mode", (uint8_t*)&mode, WiFiMode::AccessPoint );
    #else
        #ifdef CONFIG_DEFAULT_WIFI_MODE_STA
            err = nvs_get_set_u8_default( system_nvs, "wifi_mode", (uint8_t*)&mode, WiFiMode::Station );
        #endif
    #endif
    ESP_ERROR_CHECK( err );

    // Initialize the underlying TCP/IP stack
    tcpip_adapter_init();

    // Initialise main event loop and register the event loop handler
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(
        esp_event_loop_init( event_loop_handler, &server )
    );

    // Get the default WiFi configuration
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    // Initialise the WiFi and allocate resources for the WiFi driver
    ESP_ERROR_CHECK(
        esp_wifi_init( &wifi_init_config )
    );

    char *wifi_ssid = NULL, *wifi_pass = NULL;

    // Get the WiFi SSID from the NVS
    err = nvs_get_set_str_default( system_nvs, "wifi_ssid", wifi_ssid, CONFIG_WIFI_SSID );
    ESP_ERROR_CHECK( err );

    // Get the WiFi password from the NVS
    err = nvs_get_set_str_default( system_nvs, "wifi_pass", wifi_pass, CONFIG_WIFI_PASSWORD );
    ESP_ERROR_CHECK( err );

    switch( mode ){
        case WiFiMode::AccessPoint:
            ESP_LOGW( "SYS", "WiFi Mode: AP" );
            // TODO:
            break;
        case WiFiMode::Station:
            strcpy( (char*)wifi_config.sta.ssid, wifi_ssid );
            strcpy( (char*)wifi_config.sta.password, wifi_pass );

            ESP_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_STA ) );
            ESP_ERROR_CHECK(
                esp_wifi_set_config( ESP_IF_WIFI_STA, &wifi_config )
            );
            ESP_ERROR_CHECK( esp_wifi_start() );
            break;
    }

    // Hold back commiting changes to NVS until startup is complete to minimise
    // flash wear
    err = nvs_commit( system_nvs );
    ESP_ERROR_CHECK( err );

    /**
     * LED Program
     */
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

        err = nvs_set_str( led_nvs, "name", CONFIG_DEFAULT_DEVICE_NAME );
        ESP_ERROR_CHECK( err );

        char key[2] = "0";
        for( uint8_t i = 0; i < 8; ++i ){
            key[0] = (char)(i + 48);
            switch( i ) {
                case 0: err = nvs_set_blob( led_nvs, key, white, 3 ); break;
                case 1: err = nvs_set_blob( led_nvs, key, green, 3 ); break;
                case 2: err = nvs_set_blob( led_nvs, key, yellow, 3 ); break;
                case 3: err = nvs_set_blob( led_nvs, key, blue, 3 ); break;
                case 4: err = nvs_set_blob( led_nvs, key, red, 3 ); break;
                case 5: err = nvs_set_blob( led_nvs, key, teal, 3 ); break;
                case 6: err = nvs_set_blob( led_nvs, key, lilac, 3 ); break;
                case 7: err = nvs_set_blob( led_nvs, key, orange, 3 ); break;
            }
            ESP_ERROR_CHECK( err );
        }
    }

    err = nvs_commit( led_nvs );
    ESP_ERROR_CHECK( err );
}
