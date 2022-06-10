#include "nvs_flash.h"
#include "nvs.h"
#include "keypad_storage.h"
#include "keypad_log.h"

static nvs_handle_t my_handle;

static void open() {
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
      error("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
  }
  ESP_ERROR_CHECK( err );
}

void storage_init() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK( err );
  open();
}

size_t storage_get_value_length_for_key(char* key) {
  size_t length = 0;
  esp_err_t err = nvs_get_blob(my_handle, key, 0, &length);
  if (err == ESP_OK) {
    // It exists, return the length of the value
    return length;
  } else {
    if (err == ESP_ERR_NVS_NOT_FOUND) {
      // Storage is working OK, but key doesn't exist.
      return 0;
    } else {
      // Treat everything else as an error
      ESP_ERROR_CHECK(err);
      return -1;
    }
  }
}

esp_err_t storage_read(char* key, void* value, size_t* length) {
  ESP_ERROR_CHECK(nvs_get_blob(my_handle, key, value, length));
  debug("%s = %.*s\n", key, *length, (char*)value);
  return ESP_OK;
}

esp_err_t storage_write(char* key, void *value, size_t len) {
  return nvs_set_blob(my_handle, key, value, len);
}

esp_err_t storage_reset() {
  ESP_ERROR_CHECK(nvs_flash_erase());
  // esp_err_t err = nvs_erase_key(my_handle, ALARM_TOKEN_KEY);
  // if (err != ESP_ERR_NVS_NOT_FOUND && err != ESP_OK) { return err; }
  // err = nvs_erase_key(my_handle, CREDENTIALS_KEY);
  // if (err != ESP_ERR_NVS_NOT_FOUND && err != ESP_OK) { return err; }
  // err = nvs_erase_key(my_handle, SSL_CERT_KEY);
  // if (err != ESP_ERR_NVS_NOT_FOUND && err != ESP_OK) { return err; }
  // err = nvs_erase_key(my_handle, SSL_PRIVATE_KEY_KEY);
  // if (err != ESP_ERR_NVS_NOT_FOUND && err != ESP_OK) { return err; }
  return ESP_OK;
}