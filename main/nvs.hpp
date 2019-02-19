#include <cstring>
#include <nvs_flash.h>

esp_err_t nvs_get_set_str_default( nvs_handle handle, const char *key, char *&value, const char *default_value )
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

esp_err_t nvs_get_set_u8_default( nvs_handle handle, const char *key, uint8_t *value, uint8_t default_value  )
{
    esp_err_t err;
    err = nvs_get_u8( handle, key, value );
    if( err == ESP_ERR_NVS_NOT_FOUND ){
        *value = default_value;
        err = nvs_set_u8( handle, key, *value );
    }

    return err;
}
