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
#include <nvs_flash.h>
#include <esp_http_server.h>

extern "C" {
    void app_main(void);
}

#include "constants.hpp"

// API call return code. Global for convinience only.
esp_err_t err;

// Current WiFi configuration
wifi_config_t wifi_config = {};

// System NVS
nvs_handle system_nvs;

// LED NVS
nvs_handle led_nvs;

// Effect stages
int mode = 0;

// Front End Initial colours
const char white[3]  = {255, 255, 255};
const char yellow[3] = {255, 255, 0};
const char green[3] = {0, 232, 66};
const char blue[3] = {0, 174, 255};
const char red[3] = {255, 0, 66};
const char teal[3] = {0, 128, 128};
const char lilac[3] = {128, 0, 128};
const char orange[3] = {255, 127, 0};

/**
 * GET /
 * Root path response handler
 */
esp_err_t http_root_handler( httpd_req_t *req )
{
    ESP_LOGI( "HTTPD", "/" );
    extern const unsigned char index_start[] asm( "_binary_index_html_start" );
    extern const unsigned char index_end[]   asm( "_binary_index_html_end" );
    const size_t index_size = (index_end - index_start);
    httpd_resp_set_type( req, "text/html" );
    httpd_resp_send( req, (char *)index_start, index_size );
    return ESP_OK;
}
httpd_uri http_root_uri = {
    "/",
    HTTP_GET,
    http_root_handler,
    NULL
};

/**
 * GET /status
 * Status path response handler
 */
const char json[] = "{\"a\": \"%d\", \"c\": [\"%02x%02x%02x\", \"%02x%02x%02x\", \"%02x%02x%02x\", \"%02x%02x%02x\", \"%02x%02x%02x\", \"%02x%02x%02x\", \"%02x%02x%02x\", \"%02x%02x%02x\"], \"m\": \"%d\", \"n\": \"%s\", \"s\": \"%s\", \"w\": \"%d\"}";
esp_err_t http_status_handler( httpd_req_t *req )
{
    ESP_LOGI( "HTTPD", "/status" );

    char temp[sizeof(json) + 8 * 6 + 129];

    // Currently active color
    uint8_t a;
    err = nvs_get_u8( led_nvs, "a", &a );
    ESP_ERROR_CHECK( err );

    // Stores the colours loaded from NVS
    uint8_t c[8][3];

    // Get colours from NVS
    size_t rgb_size;
    char key[2] = "0";
    for( uint8_t i = 0; i < 8; ++i ){
        key[0] = (char)(i + 48);
        err = nvs_get_blob( led_nvs, key, &c[i], &rgb_size );
        ESP_ERROR_CHECK( err );
    }

    // Load the device name from NVS
    size_t device_name_length;
    err = nvs_get_str( led_nvs, "device_name", NULL, &device_name_length );
    ESP_ERROR_CHECK( err );
    char *device_name = (char*)malloc( device_name_length );
    err = nvs_get_str( led_nvs, "device_name", device_name, &device_name_length );
    ESP_ERROR_CHECK( err );

    snprintf( temp, sizeof(json) + 8 * 6 + 129, json, a, c[0][0], c[0][1], c[0][2], c[1][0], c[1][1], c[1][2], c[2][0], c[2][1], c[2][2], c[3][0], c[3][1], c[3][2], c[4][0], c[4][1], c[4][2], c[5][0], c[5][1], c[5][2], c[6][0], c[6][1], c[6][2], c[7][0], c[7][1], c[7][2], mode, device_name, wifi_config.sta.ssid, 1);

    httpd_resp_set_type( req, "text/json" );
    httpd_resp_sendstr( req, temp );
    return ESP_OK;
}
httpd_uri http_status_uri = {
    "/status",
    HTTP_GET,
    http_status_handler,
    NULL
};

httpd_handle_t start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGW( "SYS", "Starting HTTP server on port: '%d'", config.server_port );
    httpd_handle_t server;

    if( httpd_start( &server, &config ) == ESP_OK) {
            // Set URI handlers
            ESP_LOGI( "SYS", "Registering URI handlers" );
            httpd_register_uri_handler( server, &http_root_uri );
            httpd_register_uri_handler( server, &http_status_uri );
            // httpd_register_uri_handler( server, &http_set_uri ); // TODO:
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
     * WiFi Initialisation
     */
    WiFiMode mode;
    err = nvs_get_u8( system_nvs, "wifimode", (uint8_t*)&mode );
    if( err == ESP_ERR_NVS_NOT_FOUND ){
        // Initialise the mode if not set (first boot)
        #ifdef CONFIG_DEFAULT_WIFI_MODE_AP
            mode = WiFiMode::AccessPoint;
        #else
            #ifdef CONFIG_DEFAULT_WIFI_MODE_STA
                mode = WiFiMode::Station;
            #endif
        #endif
        err = nvs_set_u8( system_nvs, "wifi_mode", mode );
    }
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

    char *wifi_ssid, *wifi_pass;
    size_t wifi_ssid_length, wifi_pass_length;

    // Get the WiFi SSID from the NVS
    err = nvs_get_str( system_nvs, "wifi_ssid", NULL, &wifi_ssid_length );
    if( err == ESP_ERR_NVS_NOT_FOUND ){
        // Initialise the SSID with the default if not set
        wifi_ssid = (char*)CONFIG_WIFI_SSID;
        err = nvs_set_str( system_nvs, "wifi_ssid", wifi_ssid );
    } else {
        ESP_ERROR_CHECK( err );
        wifi_ssid = (char*)malloc( wifi_ssid_length );
        err = nvs_get_str( system_nvs, "wifi_ssid", wifi_ssid, &wifi_ssid_length );
    }
    ESP_ERROR_CHECK( err );

    // Get the WiFi password from the NVS
    err = nvs_get_str( system_nvs, "wifi_pass", NULL, &wifi_pass_length );
    if( err == ESP_ERR_NVS_NOT_FOUND ){
        // Initialise the password with the default if not set
        wifi_pass = (char*)CONFIG_WIFI_PASSWORD;
        err = nvs_set_str( system_nvs, "wifi_pass", wifi_pass );
    } else {
        ESP_ERROR_CHECK( err );
        wifi_pass = (char*)malloc( wifi_pass_length );
        err = nvs_get_str( system_nvs, "wifi_pass", wifi_pass, &wifi_pass_length );
    }
    ESP_ERROR_CHECK( err );

    switch( mode ){
        case WiFiMode::AccessPoint:
            ESP_LOGW( "SYS", "WiFi Mode: AP" );
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

    // Hold back commiting changes to NVS to minimise flash wear
    err = nvs_commit( system_nvs );
    ESP_ERROR_CHECK( err );

    // Load the LED NVS namespace
    err = nvs_open( "led", NVS_READWRITE, &led_nvs );
    ESP_ERROR_CHECK( err );

    // Initialise default colours
    uint8_t a;
    err = nvs_get_u8( led_nvs, "a", &a );
    if( err == ESP_ERR_NVS_NOT_FOUND ){
        ESP_LOGW( "LED", "Initialising default colours." );

        a = 0;
        err = nvs_set_u8( led_nvs, "a", a );
        ESP_ERROR_CHECK( err );

        err = nvs_set_str( led_nvs, "device_name", CONFIG_DEFAULT_DEVICE_NAME );
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
