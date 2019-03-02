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

#include <string.h>

#include <websocket_server.h>

#include "led.hpp"
#include "functions.hpp"

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
        const char json[] = "{\"a\":\"%d\",\"c\": [\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\"],\"m\":\"%d\",\"n\":\"%s\",\"h\":\"%s\",\"s\":\"%s\",\"w\":\"%d\"}";
        char temp[sizeof(json) + 160];

        // Currently active color (0-8) from presets
        uint8_t a;
        esp_err_t err;
        err = nvs_get_u8( LED::led_nvs, "a", &a );
        ESP_ERROR_CHECK( err );

        // Stores the colours loaded from NVS
        uint8_t c[8][3];

        // Get colours from NVS
        size_t rgb_size = 3;
        char key[2] = "0";
        for( uint8_t i = 0; i < 8; ++i ){
            key[0] = (char)(i + 48);
            err = nvs_get_blob( LED::led_nvs, key, &c[i], &rgb_size );
            ESP_ERROR_CHECK( err );
        }

        // Load the device name from NVS
        char *device_name = NULL, *hostname = NULL;
        err = nvs_get_str2( system_nvs, "device_name", device_name );
        ESP_ERROR_CHECK( err );
        err = nvs_get_str2( system_nvs, "hostname", hostname );
        ESP_ERROR_CHECK( err );

        snprintf(
            temp,
            sizeof(json) + 8 * 6 + 129,
            json,
            a,
            c[0][0], c[0][1], c[0][2],
            c[1][0], c[1][1], c[1][2],
            c[2][0], c[2][1], c[2][2],
            c[3][0], c[3][1], c[3][2],
            c[4][0], c[4][1], c[4][2],
            c[5][0], c[5][1], c[5][2],
            c[6][0], c[6][1], c[6][2],
            c[7][0], c[7][1], c[7][2],
            LED::mode,
            device_name,
            hostname,
            wifi_config.sta.ssid,
            0
        );

        if( ! ws_server_send_text_client_from_callback( num, temp, strlen( temp ) ) ){
            ESP_LOGE( TAG, "Unable to send message." );
        }
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
