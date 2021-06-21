/* WebSocket Echo Server Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "keypad.h"
#include "panel.h"
#include "keypad_display.h"
#include "keypad_storage.h"
#include "keypad_log.h"

#include <esp_https_server.h>

#define COMMAND_DISPLAY 'D'
#define COMMAND_LED 'L'
#define COMMAND_TONE 'T'
#define COMMAND_BACKLIGHT 'B'
#define COMMAND_AUDIO 'A'
#define COMMAND_AUTHENTICATION_RESULT 'F'
#define COMMAND_LOG 'G'

#define INVALID_SOCKET -1
/* A simple example that demonstrates using websocket echo server
 */
static const char *TAG = "virtual_rkp";
static httpd_handle_t server;
static int authenticated_sockets[4] = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };
static int authenticated_sockets_length = sizeof(authenticated_sockets)/sizeof(int);
static const size_t max_clients = 4;
// static int REQUIRES_AUTHENTICATION = 1;
static int NOT_AUTHENTICATED = 0;
static int AUTHENTICATED = 1;

static char* credentials = NULL;
static size_t credentials_len = 0;

#define WS_COMMAND_KEYPRESS 'K'
#define WS_COMMAND_AUTHENTICATE 'A'
#define WS_COMMAND_PANEL_MESSAGE 'P'
#define WS_COMMAND_SET_CREDENTIALS 'S'
#define WS_COMMAND_SET_ALARM_TOKEN 'T'
#define WS_COMMAND_SET_SSL_CERT 'C'
#define WS_COMMAND_SET_SSL_PRIVATE_KEY 'R'
#define WS_COMMAND_SET_SERIAL_NUMBER 'N'
#define WS_COMMAND_RESET 'E'

typedef struct session_context {
    int authenticated;
    int socket;
} session_context_t;

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    char *text;
    int text_length;
};

static inline void deep_copy_async_resp_arg(struct async_resp_arg *from, struct async_resp_arg *to) {
    to->fd = from->fd;
    to->hd = from->hd;
    to->text_length = from->text_length;
    to->text = malloc(from->text_length);
    memcpy(to->text, from->text, from->text_length);
}

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)resp_arg->text;
    ws_pkt.len = resp_arg->text_length;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // info("Here it goes to socket: %d\n", fd);
    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg->text);
    free(resp_arg);
}

static int authenticate(char* test_credentials, uint16_t length)
{
    if (credentials_len == 0 && length == 0) {
        // No credentials set and none provided, consider this a success
        return 0;
    } else {
        if ((length != credentials_len) || strncmp(credentials, test_credentials, length)) {
            ESP_LOGE(TAG, "Authentication failed");
            return 1;
        }
    }
    return 0;
}

static int respond(int socket, char *text);

static int add_socket(int socket) {
    info("Add socket: %d\n", socket);
    for (int i = 0; i < authenticated_sockets_length; i++) {
        if (authenticated_sockets[i] == INVALID_SOCKET) {
            authenticated_sockets[i] = socket;
            return i;
        }
    }
    // TODO would be better to pick oldest socket
    httpd_sess_trigger_close(server, authenticated_sockets[0]);
    authenticated_sockets[0] = socket;
    return 0;
}

static int remove_socket(int socket) {
    info("Remove socket: %d\n", socket);
    for (int i = 0; i < authenticated_sockets_length; i++) {
        if (authenticated_sockets[i] == socket) {
            authenticated_sockets[i] = INVALID_SOCKET;
            return i;
        }
    }
    return -1;
}

static int send_one(int socket, struct async_resp_arg *resp_arg) {
    if (socket != INVALID_SOCKET) {
        // info("Sending to socket: %d\n", socket);
        resp_arg->fd = socket;
        ESP_ERROR_CHECK(httpd_queue_work(server, ws_async_send, resp_arg));
    } else {
        // Invalid socket, free resp_arg (which is normally freed after response
        // is sent).
        free(resp_arg->text);
        free(resp_arg);
    }
    return 0;
}

static int send_all(struct async_resp_arg *resp_arg) {
    // info("%s", "Sending to all sockets...\n");
    int valid_sockets[authenticated_sockets_length];

    int j = 0;
    for (int i = 0; i < authenticated_sockets_length; i++) {
        int socket = authenticated_sockets[i];
        if (socket != INVALID_SOCKET) {
            valid_sockets[j++] = socket;
        }
    }
    if (j == 0) {
        // No sockets, must dispose of resp_arg
        free(resp_arg->text);
        free(resp_arg);
        return 0;
    } else {
        // If there is more than one valid socket, we must copy resp_arg
        // before sending.
        for (int i = 1; i < j; i++) {
            // Copy and send
            struct async_resp_arg *resp_arg_copy = malloc(sizeof(struct async_resp_arg));
            deep_copy_async_resp_arg(resp_arg, resp_arg_copy);
            send_one(valid_sockets[i], resp_arg_copy);
        }
        // And the last one can use the original.
        send_one(valid_sockets[0], resp_arg);
    }

    return 0;
}

static int authentication_response(int socket, char authentication_result) {
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = server;
    resp_arg->text = malloc(2);
    resp_arg->text_length = 2;
    resp_arg->text[0] = COMMAND_AUTHENTICATION_RESULT;
    resp_arg->text[1] = authentication_result;
    resp_arg->fd = socket;
    ESP_ERROR_CHECK(httpd_queue_work(server, ws_async_send, resp_arg));
    return 0;
}

static void free_session_ctx(void* ctx) {
    if (ctx != NULL) {
        session_context_t *session_ctx = (session_context_t *) ctx;
        remove_socket(session_ctx->socket);
        free(ctx);
    }
}

/*
 * Main handler for keypad, receives WebSocket messages and
 * processes them.
 */
static esp_err_t receive_message(httpd_req_t *req)
{
    ESP_LOGI(TAG, "receive_message");
    int socket = httpd_req_to_sockfd(req);
    // Initialize or get session context.
    session_context_t *session;
    if (req->sess_ctx == NULL) {
        session = malloc(sizeof(session_context_t));
        session->authenticated = NOT_AUTHENTICATED;
        session->socket = socket;
        req->sess_ctx = session;
        req->free_ctx = free_session_ctx;
    } else {
        session = (session_context_t *) req->sess_ctx;
    }

    uint8_t buf[7000] = { 0 };
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }
    //ESP_LOGD(TAG, "Got packet with message: %s", ws_pkt.payload);
    //ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
    // If multiple chars are sent, assume it is a panel message, for
    // testing a real keypad (where ESP32 is emulating real panel)
    uint8_t command = ws_pkt.payload[0];
    if (command == WS_COMMAND_AUTHENTICATE) {
        if (authenticate((char *)(ws_pkt.payload + 1), ws_pkt.len - 1) == 1) {
            authentication_response(socket, 'F');
            session->authenticated = NOT_AUTHENTICATED;
            remove_socket(socket);
        } else {
            authentication_response(socket, 'T');
            session->authenticated = AUTHENTICATED;
            add_socket(socket);
        }
    } else {
        if (session->authenticated == AUTHENTICATED) {
            ESP_LOGI(TAG, "Authenticated.");
            switch (command) {
                case WS_COMMAND_KEYPRESS:
                    ESP_LOGI(TAG, "Keypress.");
                    press_char(ws_pkt.payload[1]);
                break;
                case WS_COMMAND_PANEL_MESSAGE:
                    queue_messages(ws_pkt.payload, ws_pkt.len);
                break;
                case WS_COMMAND_SET_CREDENTIALS: {
                    if (credentials != NULL) {
                        free(credentials);
                        credentials = NULL;
                        credentials_len = 0;
                    }
                    credentials_len = ws_pkt.len - 1;
                    credentials = malloc(credentials_len);
                    memcpy(credentials, ws_pkt.payload + 1, credentials_len);
                    esp_err_t res = storage_write(CREDENTIALS_KEY, credentials, credentials_len);
                    if (res == ESP_OK) {
                        respond(socket, "VKP Token saved successfully");
                    } else {
                        ESP_LOGE(TAG, "Credentials save failed %d", res);
                        respond(socket, "VKP Token save failed");
                    }
                }
                break;
                case WS_COMMAND_SET_ALARM_TOKEN: {
                    esp_err_t res = storage_write(ALARM_TOKEN_KEY, ws_pkt.payload + 1, ws_pkt.len - 1);
                    if (res == ESP_OK) {
                        respond(socket, "Alarm token saved successfully");
                    } else {
                        ESP_LOGE(TAG, "Alarm token save failed %d", res);
                        respond(socket, "Alarm token save failed");
                    }
                }
                break;
                case WS_COMMAND_SET_SERIAL_NUMBER: {
                    int length = ws_pkt.len - 1;
                    if (length == 3) {
                      esp_err_t res = storage_write(KEYPAD_SERIAL_NUMBER_KEY, ws_pkt.payload + 1, length);
                      if (res == ESP_OK) {
                          respond(socket, "Keypad Serial Number saved.");
                      } else {
                          ESP_LOGE(TAG, "Keypad Serial Number save failed. %d", res);
                          respond(socket, "Keypad Serial Number save failed.");
                      }
                    } else {
                      respond(socket, "Keypad Serial Number wrong length, should be 3.");
                    }
                }
                break;
                case WS_COMMAND_SET_SSL_CERT: {
                    //ESP_LOGI(TAG, "Received certificate length: %d", ws_pkt.len - 1);
                    esp_err_t res = storage_write(SSL_CERT_KEY, ws_pkt.payload + 1, ws_pkt.len - 1);
                    if (res == ESP_OK) {
                        respond(socket, "Cert saved successfully");
                    } else {
                        ESP_LOGE(TAG, "Cert save failed %d", res);
                        respond(socket, "Cert save failed");
                    }
                }
                break;
                case WS_COMMAND_SET_SSL_PRIVATE_KEY: {
                    esp_err_t res = storage_write(SSL_PRIVATE_KEY_KEY, ws_pkt.payload + 1, ws_pkt.len - 1);
                    if (res == ESP_OK) {
                        respond(socket, "Key saved successfully");
                    } else {
                        ESP_LOGE(TAG, "Key save failed %d", res);
                        respond(socket, "Key save failed");
                    }
                }
                break;
                case WS_COMMAND_RESET: {
                    esp_restart();
                }
                break;
            }
        } else {
            ESP_LOGI(TAG, "Not Authenticated.");
            authentication_response(socket, 'F');
        }
    }
    return ret;
}

static int keypad_send(int socket, char command, char *text, int length, uint32_t flashing_characters) {
    if (server == NULL) {
        warn("%s\n", "Server NULL!");
        return 0;
    }
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    int flashing_length = 0;
    char flashing[9];
    if (flashing_characters > 0) {
        flashing_length = sprintf(flashing, "%X", flashing_characters);
    }
    resp_arg->hd = server;
    resp_arg->text_length = length + flashing_length + (flashing_length > 0 ? 2 : 1);
    resp_arg->text = malloc(resp_arg->text_length);
    resp_arg->text[0] = command;
    memcpy(resp_arg->text + 1, text, length);
    if (flashing_length > 0) {
        *(resp_arg->text + length + 1) = '|';
        memcpy(resp_arg->text + length + 2, flashing, flashing_length);
    }
    // info("%d %.*s\n", resp_arg->text_length, resp_arg->text_length, resp_arg->text);
    int result;
    if (socket == INVALID_SOCKET) {
        result = send_all(resp_arg);
    } else {
        result = send_one(socket, resp_arg);
    }
    return result;
}
static int keypad_send_all(char command, char *text, int length, uint32_t flashing_characters) {
    return keypad_send(INVALID_SOCKET, command, text, length, flashing_characters);
}

int keypad_log(char *text, int length) {
    return keypad_send_all(COMMAND_LOG, text, length, 0);
}

int keypad_show_text(char *text, int length, uint32_t flashing_characters) {
    return keypad_send_all(COMMAND_DISPLAY, text, length, flashing_characters);
}

static int respond(int socket, char *text) {
    return keypad_send(socket, COMMAND_DISPLAY, text, strlen(text), 0);
}

int keypad_set_leds(short power, short warn, short alarm) {
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = server;
    resp_arg->text = malloc(4);
    resp_arg->text_length = 4;
    resp_arg->text[0] = COMMAND_LED;
    resp_arg->text[1] = power + '0';
    resp_arg->text[2] = warn + '0';
    resp_arg->text[3] = alarm + '0';
    info("%d %.*s\n", resp_arg->text_length, resp_arg->text_length, resp_arg->text);
    int result = send_all(resp_arg);
    return result;
}

int keypad_set_backlight(short state) {
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = server;
    resp_arg->text_length = 2;
    resp_arg->text = malloc(resp_arg->text_length);
    resp_arg->text[0] = COMMAND_BACKLIGHT;
    resp_arg->text[1] = state + '0';
    info("%d %.*s\n", resp_arg->text_length, resp_arg->text_length, resp_arg->text);
    int result = send_all(resp_arg);
    return result;
}

int keypad_set_tone(short state) {
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = server;
    resp_arg->text_length = 2;
    resp_arg->text = malloc(resp_arg->text_length);
    resp_arg->text[0] = COMMAND_TONE;
    resp_arg->text[1] = state + '0';
    info("%d %.*s\n", resp_arg->text_length, resp_arg->text_length, resp_arg->text);
    int result = send_all(resp_arg);
    return result;
}

int keypad_set_audio(uint8_t *words, uint8_t length) {
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    resp_arg->hd = server;
    resp_arg->text_length = length * 3;
    resp_arg->text = malloc(resp_arg->text_length);
    resp_arg->text[0] = COMMAND_AUDIO;
    sprintf(resp_arg->text + 1, "%02X", words[0]);
    for (int i = 1; i < length; i++) {
        sprintf(resp_arg->text + i + 1, " %02X", words[i]);
    }
    info("%d %.*s\n", resp_arg->text_length, resp_arg->text_length, resp_arg->text);
    int result = send_all(resp_arg);
    return result;
}

int ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) {
        return 0;
    }
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr) {
        return 0;
    }
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

esp_err_t get_handler(httpd_req_t *req)
{
    info("%s\n", "Get... %d");
    /* Send a simple response */
    if (ends_with(req->uri, "/keypad.js") || ends_with(req->uri, "/")) {
        httpd_resp_set_type(req, "application/javascript");
        extern const unsigned char keypad_js_start[] asm("_binary_keypad_js_start");
        extern const unsigned char keypad_js_end[]   asm("_binary_keypad_js_end");
        ssize_t size = (keypad_js_end - 1) - keypad_js_start;
        info("Sending: %d\n", size);
        httpd_resp_send(req, (const char *) keypad_js_start, size);
    } else if (ends_with(req->uri, "/keypad.html")) {
        httpd_resp_set_type(req, "text/html");
        extern const unsigned char keypad_html_start[] asm("_binary_keypad_html_start");
        extern const unsigned char keypad_html_end[]   asm("_binary_keypad_html_end");
        ssize_t size = (keypad_html_end - 1) - keypad_html_start;
        info("Sending: %d\n", size);
        httpd_resp_send(req, (const char *) keypad_html_start, (keypad_html_end - 1) - keypad_html_start);
    } else {
        httpd_resp_send_404(req);
    }
    return ESP_OK;
}

static httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = receive_message,
        .user_ctx   = NULL,
        .handle_ws_control_frames = true,
        .is_websocket = true
};

static httpd_uri_t keypad_html = {
    .uri = "/keypad.html",
    .method = HTTP_GET,
    .handler = get_handler,
    .is_websocket = false,
};

static httpd_uri_t keypad_js = {
    .uri = "/keypad.js",
    .method = HTTP_GET,
    .handler = get_handler,
    .is_websocket = false,
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    ESP_LOGI(TAG, "Starting server");

    size_t nvsCertLength = storage_get_value_length_for_key(SSL_CERT_KEY);
    size_t nvsPrivateKeyLength = storage_get_value_length_for_key(SSL_PRIVATE_KEY_KEY);
    esp_err_t ret;
    //if (nvsCertLength > 0) {
        //unsigned char *cacert_pem = malloc(nvsCertLength);
        //ESP_ERROR_CHECK(storage_read(SSL_CERT_KEY, cacert_pem, &nvsCertLength));
        //free(cacert_pem);
    //}
    if (nvsCertLength > 0 && nvsPrivateKeyLength > 0) {
        httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
        config.httpd.max_open_sockets = max_clients;
        unsigned char *cacert_pem = malloc(nvsCertLength + 1);
        unsigned char *cacert_private_key = malloc(nvsPrivateKeyLength + 1);

        ESP_ERROR_CHECK(storage_read(SSL_CERT_KEY, cacert_pem, &nvsCertLength));
        // The cert must be NUL terminated, add null character to the end.
        if (cacert_pem[nvsCertLength - 1] != '\0') {
            cacert_pem[nvsCertLength] = '\0';
            nvsCertLength++;
        }
        ESP_ERROR_CHECK(storage_read(SSL_PRIVATE_KEY_KEY, cacert_private_key, &nvsPrivateKeyLength));
        // The cert must be NUL terminated, add null character to the end.
        if (cacert_private_key[nvsPrivateKeyLength - 1] != '\0') {
            cacert_private_key[nvsPrivateKeyLength] = '\0';
            nvsPrivateKeyLength++;
        }
        config.cacert_pem = cacert_pem;
        config.cacert_len = nvsCertLength;
        config.prvtkey_pem = cacert_private_key;
        config.prvtkey_len = nvsPrivateKeyLength;
        ret = httpd_ssl_start(&server, &config);

        if (config.cacert_len > 0) {
            free((void*) config.cacert_pem);
            config.cacert_pem = NULL;
        }
        if (config.prvtkey_len > 0) {
            free((void*) config.prvtkey_pem);
            config.prvtkey_pem = NULL;
        }
        ESP_LOGI(TAG, "https default stack size: %d\n", config.httpd.stack_size);
        //config.httpd.stack_size = 16384;
        config.httpd.stack_size = 24576;
        //config.httpd.stack_size = 32768;
        ESP_LOGI(TAG, "http new stack size: %d\n", config.httpd.stack_size);
    } else {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.max_open_sockets = max_clients;
        ESP_LOGI(TAG, "http default stack size: %d\n", config.stack_size);
        config.stack_size = 16384;
        //config.stack_size = 32768;
        ESP_LOGI(TAG, "http new stack size: %d\n", config.stack_size);
        ret = httpd_start(&server, &config);
    }

    if (ESP_OK != ret) {
        ESP_LOGI(TAG, "Error starting server!");
        return NULL;
    }
    if (credentials != NULL) {
        free(credentials);
        credentials = NULL;
    }
    credentials_len = storage_get_value_length_for_key(CREDENTIALS_KEY);
    if (credentials_len > 0) {
        credentials = malloc(credentials_len);
        storage_read(CREDENTIALS_KEY, credentials, &credentials_len);
    }

    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &ws);
    httpd_register_uri_handler(server, &keypad_html);
    httpd_register_uri_handler(server, &keypad_js);
    return server;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

int start_server()
{
    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    /* Start the server for the first time */
    server = start_webserver();
    return 0;
}