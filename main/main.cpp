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

extern "C" {
    void app_main(void);
}

#include "led.hpp"
#include "functions.hpp"
#include "constants.hpp"
#include "http_server.hpp"

// API call return code. Global for convinience only.
esp_err_t err;

// Current WiFi mode
WiFiMode wifi_mode;

// Current WiFi configuration
wifi_config_t wifi_config = {};

// System NVS
nvs_handle system_nvs;

static esp_err_t wifi_event_handler( void *ctx, system_event_t *event ){
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
            HTTPServer::start();
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGW( "SYS", "WiFi Disconnected. Attempting reconnect." );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            HTTPServer::stop();
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
     * Non-volatile storage (NVS) initialization
     */
    err = nvs_flash_init();

    // Recoverable NVS problems
    switch( err ){
        case ESP_ERR_NVS_NO_FREE_PAGES:
        case ESP_ERR_NVS_NEW_VERSION_FOUND:
            // Erase and re-initialize the NVS partition
            ESP_ERROR_CHECK( nvs_flash_erase() );
            err = nvs_flash_init();
    }

    ESP_ERROR_CHECK( err );

    // Load the system NVS namespace
    err = nvs_open( "sys", NVS_READWRITE, &system_nvs );
    ESP_ERROR_CHECK( err );

    /**
     * mDNS Initialization
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
     * WiFi Initialization
     */
    #ifdef CONFIG_DEFAULT_WIFI_MODE_AP
        err = nvs_get_set_default_u8( system_nvs, "wifi_mode", (uint8_t*)&wifi_mode, WiFiMode::AccessPoint );
    #else
        #ifdef CONFIG_DEFAULT_WIFI_MODE_STA
            err = nvs_get_set_default_u8( system_nvs, "wifi_mode", (uint8_t*)&wifi_mode, WiFiMode::Station );
        #endif
    #endif
    ESP_ERROR_CHECK( err );

    // Initialize the underlying TCP/IP stack
    tcpip_adapter_init();

    // Initialize main event loop and register the event loop handler
    ESP_ERROR_CHECK(
        esp_event_loop_init( wifi_event_handler, NULL )
    );

    // Get the default WiFi configuration
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    // Initialize the WiFi and allocate resources for the WiFi driver
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

    switch( wifi_mode ){
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
}
