#pragma once

#define ALARM_TOKEN_KEY "alarmToken"
#define SSL_CERT_KEY "sslCert"
#define SSL_PRIVATE_KEY_KEY "sslPrivateKey"
#define CREDENTIALS_KEY "credentials"
#define KEYPAD_SERIAL_NUMBER_KEY "serialNumber"

void storage_init();
size_t storage_get_value_length_for_key(char* key);
esp_err_t storage_read(char* key, void* value, size_t* length);
esp_err_t storage_write(char* key, void* value, size_t len);
esp_err_t storage_reset();