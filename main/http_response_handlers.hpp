#include <esp_http_server.h>

extern wifi_config_t wifi_config;
extern nvs_handle led_nvs;
extern nvs_handle system_nvs;
extern esp_err_t err;
extern uint8_t mode;

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
const char json[] = "{\"a\":\"%d\",\"c\": [\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\",\"%02x%02x%02x\"],\"m\":\"%d\",\"n\":\"%s\",\"h\":\"%s\",\"s\":\"%s\",\"w\":\"%d\"}";
esp_err_t http_status_handler( httpd_req_t *req )
{
    ESP_LOGI( "HTTPD", "/status" );

    char temp[sizeof(json) + 8 * 6 + 129];

    // Currently active color (0-8) from presets
    uint8_t a;
    err = nvs_get_u8( led_nvs, "a", &a );
    ESP_ERROR_CHECK( err );

    // Stores the colours loaded from NVS
    uint8_t c[8][3];

    // Get colours from NVS
    size_t rgb_size = 3;
    char key[2] = "0";
    for( uint8_t i = 0; i < 8; ++i ){
        key[0] = (char)(i + 48);
        err = nvs_get_blob( led_nvs, key, &c[i], &rgb_size );
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
        mode,
        device_name,
        hostname,
        wifi_config.sta.ssid,
        0
    );

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

/**
 * POST /set
 * Set path response handler
 */
esp_err_t http_set_handler( httpd_req_t *req )
{
    ESP_LOGI( "HTTPD", "/set" );

    // char ssid[32];
    // char password[64];

//     String password, temp, error="";
//     char temp2[64];
//     bool storeConfig = false;
//     if( error.length() == 0 ) {
//         for(int i = 0; i < server.args(); ++i) {
//             if ( server.argName(i) == "a" ) { // Current color
//                 EEPROM.write( 25, server.arg(i).toInt() % 255 );
//                 char color[6];
//                 byte preset[3];
//                 EEPROM.get(1 + server.arg(i).toInt() % 8 * 3, preset);
// //                snprintf(color, 6, "%02x%02x%02x", preset[0], preset[1], preset[2]);
// //                Serial.print('b');
// //                Serial.println(color);
//                 Serial.print("c ");
//                 Serial.print(int(preset[0]));
//                 Serial.print(" ");
//                 Serial.print(int(preset[1]));
//                 Serial.print(" ");
//                 Serial.println(int(preset[2]));
//             } else if ( server.argName(i) == "m") { // Current mode
//                 int wmode = server.arg(i).toInt() % 255;
//                  // EEPROM.write( 0, wmode );
//                 mode = wmode;
//                 // Prepare the mode for initialization
//                 stage = -1;
//                 switch (mode) {
//                   case 0:
//                     Serial.println("effect stop");
//                     break;
//                   case 1:
//                     Serial.println("effect HSV fade");
//                     break;
//                   case 2:
//                     Serial.println("effect duration down");
//                     break;
//                   case 3:
//                     Serial.println("effect brightness up");
//                     break;
//                   case 4:
//                     Serial.println("shutdown");
//                     break;
//                   case 5:
//                     Serial.println("effect random fade");
//                     break;
//                   case 6:
//                     Serial.println("effect duration up");
//                     break;
//                   case 7:
//                     Serial.println("effect brightness down");
//                     break;
//                   default:
//                     Serial.println(mode);
//                     break;
//                 }
//             } else if ( server.argName(i) == "n") { // Device Human Readable name
//                 temp = server.arg(i);
//                 temp.remove(63);
//                 urldecode(temp2, temp.c_str());
//                 device.setName(temp2);
//                 storeConfig = true;
//             } else if ( server.argName(i) == "d") { // Domain name
//                 temp = server.arg(i);
//                 temp.remove(63);
//                 urldecode(temp2, temp.c_str());
//                 // Ignore
//             } else { // Color input
//                 for(int j = 0; j < 8; ++j) {
//                     if( server.argName(i) == (String("c") + j) ) {
//
//                       // Convert hex color to byte array
//                       temp = server.arg(i);
//                       temp.remove(6);
//                       Serial.print('b');
//                       Serial.println(temp);
//                       const char *src = temp.c_str();
//                       byte color[3];
//                       for(int k = 0; k < 3; ++k) {
//                           color[k] = hex2bin(src);
//                           src += 2;
//                       }
//
//                       EEPROM.put( 1 + j * 3, color );
//                     }
//                 }
//             }
//         }
//     }
//     if( error.length() > 0) {
//         server.sendHeader("Refresh", "5;url=/");
//         server.send(400, "text/plain", error);
//     } else {
//         if( storeConfig ) {
//             device.initWiFi();
//         }
//         EEPROM.commit();
//         server.sendHeader("Redirect", "/");
//         server.send(200);
//     }
    return ESP_OK;
}
httpd_uri http_set_uri = {
    "/set",
    HTTP_GET,
    http_set_handler,
    NULL
};
