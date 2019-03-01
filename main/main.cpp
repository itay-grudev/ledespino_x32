/**
 * Ledespino_x32 is an advanced LED strip controller based on the ESP32 WiFi
 * enabled IoT microcontroller.
 * Copyright: Itay Grudev 2019
 * License: GNU GPLv3
 */

// NOTES: VALIDATE SSID DOES NOT EXCEED 31 chars excluding \0 (32 in total)
// NOTES: VALIDATE PASS DOES NOT EXCEED 63 chars excluding \0 (64 in total)

#include <mdns.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_event_loop.h>
#include <esp_event_loop.h>
#include <Arduino.h>

extern "C" {
    void app_main(void);
}

#include "led.hpp"
#include "nvs.hpp"
#include "constants.hpp"
#include "http_response_handlers.hpp"

// API call return code. Global for convinience only.
esp_err_t err;

// Current WiFi configuration
wifi_config_t wifi_config = {};

// System NVS
nvs_handle system_nvs;

uint8_t hex2bin( const char * );

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
    char *hostname = NULL, *device_name = NULL;
    err = nvs_get_set_default_str( system_nvs, "hostname", hostname, CONFIG_DEFAULT_HOSTNAME );
    ESP_ERROR_CHECK( err );
    err = nvs_get_set_default_str( system_nvs, "device_name", device_name, CONFIG_DEFAULT_DEVICE_NAME );

    ESP_LOGW( "SYS", "Hostname: %s", hostname );

    // Set the mDNS hostname
    ESP_ERROR_CHECK(
        mdns_hostname_set( hostname )
    );

    // Set the default instance name
    ESP_ERROR_CHECK(
        mdns_instance_name_set( device_name )
    );

    /**
     * WiFi Initialisation
     */
    WiFiMode mode;

    #ifdef CONFIG_DEFAULT_WIFI_MODE_AP
        err = nvs_get_set_default_u8( system_nvs, "wifi_mode", (uint8_t*)&mode, WiFiMode::AccessPoint );
    #else
        #ifdef CONFIG_DEFAULT_WIFI_MODE_STA
            err = nvs_get_set_default_u8( system_nvs, "wifi_mode", (uint8_t*)&mode, WiFiMode::Station );
        #endif
    #endif
    ESP_ERROR_CHECK( err );

    // Initialise the underlying TCP/IP stack
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
    err = nvs_get_set_default_str( system_nvs, "wifi_ssid", wifi_ssid, CONFIG_WIFI_SSID );
    ESP_ERROR_CHECK( err );

    // Get the WiFi password from the NVS
    err = nvs_get_set_default_str( system_nvs, "wifi_pass", wifi_pass, CONFIG_WIFI_PASSWORD );
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
    LED::setup();
    LED::run();
    //
    // Serial.begin(115200);
    //
    // Serial.println("binary color ready");
    // String command;
    // while( Serial.available() > 0 ) {
    //     char byte;
    //     Serial.readBytes(&byte, 1);
    //     if( byte == '\n' )
    //     {
    //         if ( command[0] == 'b' ) {
    //             Serial.println("binary color");
    //             CRGB color;
    //             color.r = hex2bin( command.substring(1, 3).c_str() );
    //             color.g = hex2bin( command.substring(3, 5).c_str() );
    //             color.b = hex2bin( command.substring(5, 7).c_str() );
    //         }
    //
    //         // Discard the current command
    //         command = "";
    //     } else {
    //         command += byte;
    //         // Discard commands longer than 255 symbols
    //         if( command.length() > 255 )
    //             command = "";
    //     }
    // }
}

uint8_t hex2bin( const char *s )
{
  int ret = 0;
  int i;
  for( i=0; i<2; i++ )
  {
    char c = *s++;
    int n=0;
    if( '0'<=c && c<='9' )
      n = c-'0';
    else if( 'a'<=c && c<='f' )
      n = 10 + c-'a';
    else if( 'A'<=c && c<='F' )
      n = 10 + c-'A';
    ret = n + ret*16;
  }
  return ret;
}
