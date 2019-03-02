#include "functions.hpp"

#include <cstring>
#include <assert.h>

esp_err_t nvs_get_str2( nvs_handle handle, const char *key, char *&value )
{
    size_t length;
    esp_err_t err;
    err = nvs_get_str( handle, key, NULL, &length );
    if( err != ESP_OK ) return err;
    value = (char*)malloc( length );
    err = nvs_get_str( handle, key, value, &length );
    return err;
}

esp_err_t nvs_get_set_default_str( nvs_handle handle, const char *key, char *&value, const char *default_value )
{
    size_t length;
    esp_err_t err;
    err = nvs_get_str( handle, key, NULL, &length );
    if( err == ESP_ERR_NVS_NOT_FOUND ){
        // value = (char*)malloc( strlen(default_value) + 1 );
        // strcpy( value, default_value );
        value = (char*)default_value;
        err = nvs_set_str( handle, key, value );
    } else {
        if( err != ESP_OK ) return err;
        value = (char*)malloc( length );
        err = nvs_get_str( handle, key, value, &length );
    }
    return err;
}

esp_err_t nvs_get_set_default_u8( nvs_handle handle, const char *key, uint8_t *value, uint8_t default_value  )
{
    esp_err_t err;
    err = nvs_get_u8( handle, key, value );
    if( err == ESP_ERR_NVS_NOT_FOUND ){
        *value = default_value;
        err = nvs_set_u8( handle, key, *value );
    }
    return err;
}

uint32_t hex2bin( char *s )
{
  uint8_t len = strlen( s );
  assert( len <= 8 ); // Meant for conversion of colors and capped at uint32_t
  uint32_t ret = 0;
  for( int i = 0; i < len; ++i ){
    char c = *s++;
    uint32_t n = 0;
    if( '0' <= c && c <= '9' )
      n = c - '0';
    else if( 'a' <= c && c <= 'f' )
      n = 10 + c - 'a';
    else if( 'A' <= c && c<= 'F' )
      n = 10 + c - 'A';
    ret <<= 4;
    ret += n;
  }
  return ret;
}
