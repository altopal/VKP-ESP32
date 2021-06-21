#include <esp_log.h>
#include "esp_http_client.h"
#include "keypad_storage.h"
#include "keypad_send_alarm.h"
#include "keypad_log.h"

static const char *TAG = "send_alarm";
static esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            info("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                info("%.*s", evt->data_len, (char*)evt->data);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

void send_alarm(char* title, char* message)
{
  size_t alarmTokenLength = storage_get_value_length_for_key(ALARM_TOKEN_KEY);
  if (alarmTokenLength > 0) {
    char* alarmToken = malloc(alarmTokenLength);
    storage_read(ALARM_TOKEN_KEY, alarmToken, &alarmTokenLength);

    esp_http_client_config_t config = {
        .url = "https://europe-west1-virtual-rkp.cloudfunctions.net/messages",
        .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    char* buf = malloc(2000);
    size_t len = sprintf(buf, "{\"alarmToken\": \"%.*s\", \"title\": \"%s\", \"body\": \"%s\"}"
                         , alarmTokenLength, alarmToken
                         , title, message);
    esp_http_client_set_post_field(client, buf, len);
    esp_err_t err = esp_http_client_perform(client);

    free(buf);
    free(alarmToken);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    }
    esp_http_client_cleanup(client);
  }
}