#include "http_server.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <lwip/api.h>

#include <mdns.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_event_loop.h>
#include <nvs_flash.h>
#include <driver/ledc.h>
#include <cJSON.h>

#include <string.h>

#include <websocket_server.h>

#include "led.hpp"
#include "constants.hpp"
#include "functions.hpp"

extern WiFiMode wifi_mode;
extern wifi_config_t wifi_config;
extern nvs_handle system_nvs;

class HTTPServerPrivate {
public:
    static QueueHandle_t client_queue;
    const static int client_queue_size = 10;

    /**
     * Handles inital client connections and stores them in a queue
     */
    static void http_listen_task( void* pvParameters ){
        const static char* TAG = "HTTP";

        struct netconn *connection, *new_connection;
        static err_t err;
        client_queue = xQueueCreate( client_queue_size, sizeof(struct netconn*) );

        connection = netconn_new( NETCONN_TCP );
        netconn_bind( connection, NULL, 80);
        netconn_listen( connection );
        ESP_LOGI( TAG, "Listening on port 80." );

        // Register the mDNS service
        mdns_service_add( NULL, "_http", "_tcp", 80, NULL, 0 );

        do {
            err = netconn_accept( connection, &new_connection );
            // ESP_LOGI( TAG, "new client" );
            if(err == ERR_OK) {
                xQueueSendToBack( client_queue, &new_connection, portMAX_DELAY );
                //http_serve(new_connection);
            }
        } while(err == ERR_OK);

        netconn_close( connection );
        netconn_delete( connection );

        // De-register the mDNS service
        // mdns_service_remove( "_http", "_tcp" );

        ESP_LOGE( TAG, "Failure! Rebooting..." );
        esp_restart();
    }

    /**
     * Receives clients from the queue and serves them
     */
    static void http_connection_task( void* pvParameters ){
        // const static char* TAG = "HTTP CONN";
        // ESP_LOGI( TAG, "started" );

        struct netconn* connection;
        while( true ){
            xQueueReceive( client_queue, &connection, portMAX_DELAY );
            if( !connection ) continue;
            http_server( connection );
        }
        vTaskDelete( NULL );
    }

    static void http_server( struct netconn *connection ){
        const static char* TAG = "HTTP";

        const static char HTML_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
        const static char ERROR_HEADER[] = "HTTP/1.1 404 Not Found\nContent-type: text/html\n\n";
        // const static char JS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
        // const static char CSS_HEADER[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
        // const static char PNG_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
        // const static char ICO_HEADER[] = "HTTP/1.1 200 OK\nContent-type: image/x-icon\n\n";
        // const static char PDF_HEADER[] = "HTTP/1.1 200 OK\nContent-type: application/pdf\n\n";
        // const static char EVENT_HEADER[] = "HTTP/1.1 200 OK\nContent-Type: text/event-stream\nCache-Control: no-cache\nretry: 3000\n\n";
        // Index HTML
        extern const unsigned char index_html_start[] asm( "_binary_index_html_start" );
        extern const unsigned char index_html_end[]   asm( "_binary_index_html_end" );
        const size_t index_html_len = index_html_end - index_html_start;

        // TODO: Add favicon.ico
        // extern const uint8_t favicon_ico_start[] asm( "_binary_favicon_ico_start" );
        // extern const uint8_t favicon_ico_end[] asm( "_binary_favicon_ico_end" );
        // const uint32_t favicon_ico_len = favicon_ico_end - favicon_ico_start;

        // Error HTML
        extern const uint8_t error_html_start[] asm( "_binary_error_html_start" );
        extern const uint8_t error_html_end[] asm( "_binary_error_html_end" );
        const uint32_t error_html_len = error_html_end - error_html_start;

        struct netbuf* input_buffer;
        char* buffer;
        uint16_t buffer_len;
        err_t err;

        // Allow a connection timeout of 1 second
        netconn_set_recvtimeout( connection, 1000 );
        err = netconn_recv( connection, &input_buffer);

        if( err == ERR_OK ) {
            netbuf_data( input_buffer, (void**)&buffer, &buffer_len );

            if( buffer ){
                // Index
                if( strstr( buffer, "GET / " )){
                    ESP_LOGI( TAG,"GET /" );
                    netconn_write( connection, HTML_HEADER, sizeof(HTML_HEADER) - 1, NETCONN_NOCOPY );
                    netconn_write( connection, index_html_start, index_html_len, NETCONN_NOCOPY );
                    netconn_close( connection );
                    netconn_delete( connection );
                    netbuf_delete( input_buffer );
                }

                // Upgrade to WebSockets
                else if(
                  strstr( buffer, "GET /ws " ) &&
                  strstr( buffer, "Upgrade: websocket" )
                ){
                  ESP_LOGI( TAG, "GET /ws Upgrade: websocket");
                  ws_server_add_client( connection, buffer, buffer_len, "/ws", websocket_callback );
                  netbuf_delete( input_buffer );
                }

                // TODO: favicon.ico
                // else if( strstr( buffer, "GET /favicon.ico " )){
                //     ESP_LOGI( TAG, "HTTP /favicon.ico" );
                //     netconn_write( connection, ICO_HEADER,sizeof(ICO_HEADER) - 1, NETCONN_NOCOPY );
                //     netconn_write( connection, favicon_ico_start, favicon_ico_len, NETCONN_NOCOPY );
                //     netconn_close( connection );
                //     netconn_delete( connection );
                //     netbuf_delete( input_buffer );
                // }

                else if( strstr( buffer, "GET /" )){
                    buffer[buffer_len - 1] = '\0'; // trailing zero for ESP_LOGI
                    ESP_LOGI( TAG, "Unknown Request\n %s", buffer );
                    netconn_write( connection, ERROR_HEADER, sizeof(ERROR_HEADER) - 1, NETCONN_NOCOPY );
                    netconn_write( connection, error_html_start, error_html_len, NETCONN_NOCOPY );
                    netconn_close( connection );
                    netconn_delete( connection );
                    netbuf_delete( input_buffer );
                }

                else {
                    ESP_LOGI( TAG, "Unknown request");
                    netconn_close( connection );
                    netconn_delete( connection );
                    netbuf_delete( input_buffer );
                }
            } else {
                ESP_LOGI( TAG, "Unknown request (empty?...)" );
                netconn_close( connection );
                netconn_delete( connection );
                netbuf_delete( input_buffer );
            }
        } else { // err != ERR_OK
            ESP_LOGI( TAG, "error on read, closing connection");
            netconn_close( connection );
            netconn_delete( connection );
            netbuf_delete( input_buffer );
        }
    }

    static void websocket_callback( uint8_t num ,WEBSOCKET_TYPE_t type, char* msg, uint64_t len ){
        const static char* TAG = "WEBS";
        int value;

        switch( type ){
            case WEBSOCKET_CONNECT:
                ESP_LOGI( TAG, "client %i connected!", num );
                websocket_send_status( num );
                break;
            case WEBSOCKET_DISCONNECT_EXTERNAL:
                ESP_LOGI( TAG, "client %i sent a disconnect message", num );
                break;
            case WEBSOCKET_DISCONNECT_INTERNAL:
                ESP_LOGI( TAG, "client %i was disconnected", num );
                break;
            case WEBSOCKET_DISCONNECT_ERROR:
                ESP_LOGI( TAG, "client %i was disconnected due to an error", num );
                break;
            case WEBSOCKET_TEXT:
                if( len ) {
                    // switch(msg[0]) {
                    //     case 'L':
                    //         if(sscanf(msg,"L%i",&value)) {
                    //             ESP_LOGI(TAG,"LED value: %i",value);
                    //             led_duty(value);
                    //             ws_server_send_text_all_from_callback(msg,len); // broadcast it!
                    //         }
                    //         break;
                    // }
                }
                break;
            case WEBSOCKET_BIN:
                ESP_LOGI(TAG,"client %i sent binary message of size %i:\n%s",num,(uint32_t)len,msg);
                break;
            case WEBSOCKET_PING:
                ESP_LOGI(TAG,"client %i pinged us with message of size %i:\n%s",num,(uint32_t)len,msg);
                break;
            case WEBSOCKET_PONG:
                ESP_LOGI(TAG,"client %i responded to the ping",num);
                break;
        }
    }

    static void websocket_send_status( uint8_t num ){
        const static char* TAG = "WEBS";
        esp_err_t err;

        // Root JSON node
        cJSON *root, *tmp, *tmp2, *tmp3;
        char *device_name = NULL, *hostname = NULL, *json;

        if(!( root = cJSON_CreateObject() )) goto json_build_error;

        // WiFi Mode
        if(!( tmp = cJSON_CreateNumber( wifi_mode ) )) goto json_build_error;
        cJSON_AddItemToObject( root, "w", tmp );

        // WiFi SSID
        if( wifi_mode == WiFiMode::Station ){
            if(!( tmp = cJSON_CreateString( (const char*)wifi_config.sta.ssid ) ))
                goto json_build_error;
        } else {
            if(!( tmp = cJSON_CreateString( (const char*)wifi_config.ap.ssid ) ))
                goto json_build_error;
        }
        cJSON_AddItemToObject( root, "s", tmp );

        // Device hostname
        err = nvs_get_str2( system_nvs, "hostname", hostname );
        ESP_ERROR_CHECK( err );
        if(!( tmp = cJSON_CreateString( hostname ) )) goto json_build_error;
        cJSON_AddItemToObject( root, "h", tmp );

        // Device name
        err = nvs_get_str2( system_nvs, "device_name", device_name );
        ESP_ERROR_CHECK( err );
        if(!( tmp = cJSON_CreateString( device_name ) )) goto json_build_error;
        cJSON_AddItemToObject( root, "n", tmp );

        // Color palette
        if(!( tmp = cJSON_CreateArray() )) goto json_build_error;
        cJSON_AddItemToObject( root, "c", tmp );
        {
            size_t rgb_size = 3;
            char key[2] = "0";
            uint8_t rgb[3];
            for( uint8_t i = 0; i < 8; ++i ){
                key[0] = (char)(i + 48);
                err = nvs_get_blob( LED::led_nvs, key, &rgb, &rgb_size );
                ESP_ERROR_CHECK( err );
                if(!( tmp2 = cJSON_CreateObject() )) goto json_build_error;
                cJSON_AddItemToArray( tmp, tmp2 );
                if(!( tmp3 = cJSON_CreateNumber( i ) )) goto json_build_error;
                cJSON_AddItemToObject( tmp2, "i", tmp3 );
                char hex[7];
                snprintf( hex, 7, "%02X%02X%02X", rgb[0], rgb[1], rgb[2] );
                if(!( tmp3 = cJSON_CreateString( hex ) )) goto json_build_error;
                cJSON_AddItemToObject( tmp2, "c", tmp3 );
            }
        }

        // Active color index
        if(!( tmp = cJSON_CreateNumber( wifi_mode ) )) goto json_build_error;
        cJSON_AddItemToObject( root, "a", tmp );

        // List of available modes
        {
            if(!( tmp = cJSON_CreateArray() )) goto json_build_error;
            cJSON_AddItemToObject( root, "m", tmp );
            char* modes[] = {
                "Off",
                "Static Color",
                "HSV Fade",
                "Random Fade"
            };
            for( int i = 0; i < sizeof(modes) / sizeof(char*); ++i ){
                if(!( tmp2 = cJSON_CreateObject() )) goto json_build_error;
                cJSON_AddItemToArray( tmp, tmp2 );
                if(!( tmp3 = cJSON_CreateNumber( i ) )) goto json_build_error;
                cJSON_AddItemToObject( tmp2, "i", tmp3 );
                if(!( tmp3 = cJSON_CreateString( modes[i] ) )) goto json_build_error;
                cJSON_AddItemToObject( tmp2, "n", tmp3 );
            }
        }

        // Active mode
        if(!( tmp = cJSON_CreateNumber( LED::active_mode ) )) goto json_build_error;
        cJSON_AddItemToObject( root, "q", tmp );

        json = cJSON_Print( root );
        ws_server_send_text_client_from_callback( num, json, strlen(json) );

        return;
json_build_error:
        ESP_LOGE( TAG, "JSON generation error." );
    }
};

QueueHandle_t HTTPServerPrivate::client_queue = QueueHandle_t();
HTTPServerPrivate* HTTPServer::d = new HTTPServerPrivate;

void HTTPServer::start(){
    ws_server_start();
    xTaskCreate( &d->http_listen_task, "http_listen_task", 3000, NULL, 9, NULL );
    xTaskCreate( &d->http_connection_task, "http_connection_task", 4000, NULL, 6, NULL );
}

void HTTPServer::stop(){
    ws_server_stop();
}
