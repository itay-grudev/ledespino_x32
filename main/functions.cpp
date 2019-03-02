#include "functions.hpp"

#include <cstring>

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
