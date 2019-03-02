#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <nvs_flash.h>

esp_err_t nvs_get_str2( nvs_handle handle, const char *key, char *&value );
esp_err_t nvs_get_set_default_str( nvs_handle handle, const char *key, char *&value, const char *default_value );
esp_err_t nvs_get_set_default_u8( nvs_handle handle, const char *key, uint8_t *value, uint8_t default_value  );
uint8_t hex2bin( const char *s );

#endif //FUNCTIONS_H
