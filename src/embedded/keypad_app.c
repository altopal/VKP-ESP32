/* UART Events Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "keypad.h"
#include "panel.h"
#include "keypad.h"
#include "keypad_wifi.h"
#include "keypad_display.h"
#include "keypad_web_socket.h"
#include "keypad_storage.h"
#include "keypad_alarm.h"
#include "keypad_reset.h"
#include "keypad_memory_debug.h"


static const char *TAG = "keypad_app";

/**
 * This example shows how to use the UART driver to handle special UART events.
 *
 * It also reads data from UART0 directly, and echoes it to console.
 *
 * - Port: UART0
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: on
 * - Pin assignment: TxD (default), RxD (default)
 */

#define EX_UART_NUM UART_NUM_0
#define KEYPAD_UART_NUM UART_NUM_1
#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define BUF_SIZE (UART_FIFO_LEN)
#define RD_BUF_SIZE (UART_FIFO_LEN)
#define QUEUE_BUF_SIZE (RD_BUF_SIZE+2)
static QueueHandle_t uart_keypad_queue;

int write_to_panel(uint8_t *buf, uint16_t length) {
  // printf("K ");
  // for (int i = 0; i < length; i++) {
    // printf("%02X ", buf[i]);
  // }
  // printf(" (%dms) \n", response_delay_ms);

  vTaskDelay(response_delay_ms / portTICK_PERIOD_MS);
  uart_write_bytes(KEYPAD_UART_NUM, (const char *)buf, length);
  return 0;
}

int write_to_keypad(uint8_t *buf, uint16_t length) {
  // printf("p ");
  // for (int i = 0; i < length; i++) {
  //   printf("%02X ", buf[i]);
  // }
  // printf("\n");

  uart_write_bytes(KEYPAD_UART_NUM, (const char *)buf, length);
  return 0;
}

static void uart_keypad_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(QUEUE_BUF_SIZE);
    uint8_t buf[256];
    keypad_frame_t frame = {
      .buf = buf,
      .index = 0,
      .checksum = 0,
      .counter = 0x3F,
      .state = FRAME_IDLE,
      .command = NULL
    };
    for(;;) {
        //Waiting for UART event.
        if(xQueueReceive(uart_keypad_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            bzero(dtmp, QUEUE_BUF_SIZE);
            //ESP_LOGI(TAG, "uart[%d] event:", KP_UART_NUM);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    //ESP_LOGI(TAG, "[UART DATA]: %d", event.size);
                    uart_read_bytes(KEYPAD_UART_NUM, dtmp, event.size, portMAX_DELAY);
                    for (int i = 0; i < event.size; i++) {
                        //msg[0] = hex[dtmp[i]>>4&0xF];
                        //msg[1] = hex[dtmp[i]&0xF];
                        //msg[2] = '\n';
                        //uart_write_bytes(EX_UART_NUM, (const char*) msg, 3);
                        process_byte(&frame, dtmp[i], &write_to_panel);
                    }
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    //ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(KEYPAD_UART_NUM);
                    xQueueReset(uart_keypad_queue);
                    uart_write_bytes(EX_UART_NUM, "2--OV\n", 6);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(KEYPAD_UART_NUM);
                    xQueueReset(uart_keypad_queue);
                    uart_write_bytes(EX_UART_NUM, "2--RBF\n", 7);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    //ESP_LOGI(TAG, "uart rx break");
                    uart_write_bytes(EX_UART_NUM, "2--BK\n", 6);
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "uart frame error");
                    break;
                //Others
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}
#define UART_INTR_RXFIFO_FULL       (0x1<<0)
#define UART_INTR_RXFIFO_OVF        (0x1<<4)
#define UART_INTR_RXFIFO_TOUT       (0x1<<8)
#define UART_INTR_BRK_DET           (0x1<<7)
#define UART_INTR_PARITY_ERR        (0x1<<2)

static void uart_zero() {
    uart_driver_install(EX_UART_NUM, BUF_SIZE, BUF_SIZE, 20, NULL, 0);
    uart_set_baudrate(EX_UART_NUM, 921600);
}

/**
 * Emulate panel to drive real keypad, to help analyse the protocol.
 */
void send_panel_messages(void *pvParameter)
{
  while (1)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    next_frame(&write_to_keypad);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //esp_log_level_set("httpd_ws", ESP_LOG_DEBUG);
    //esp_log_level_set("httpd_parse", ESP_LOG_DEBUG);

    uart_zero();

    storage_init();
    int serial_number_length = storage_get_value_length_for_key(KEYPAD_SERIAL_NUMBER_KEY);

    if (serial_number_length == 3) {
        uint8_t *values = malloc(serial_number_length + 1);
        values[0] = 0;
        size_t length = 3;
        storage_read(KEYPAD_SERIAL_NUMBER_KEY, values + 1, &length);
        keypad_init(values);
        free(values);
    }
    monitor_reset();
    // This will block until wifi is connected
    wifi_connect();

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 19200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    uart_intr_config_t uart_intr = {
        .intr_enable_mask = UART_INTR_RXFIFO_FULL | UART_INTR_RXFIFO_TOUT | UART_INTR_RXFIFO_OVF | UART_INTR_BRK_DET | UART_INTR_PARITY_ERR,
        .rxfifo_full_thresh = 10,
        .rx_timeout_thresh = 1,
    };

    //Install UART driver, and get the queue.
    uart_driver_install(KEYPAD_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_keypad_queue, 0);
    uart_param_config(KEYPAD_UART_NUM, &uart_config);
    uart_intr_config(KEYPAD_UART_NUM, &uart_intr);
    //uart_set_line_inverse(KEYPAD_UART_NUM, UART_SIGNAL_RXD_INV | UART_SIGNAL_TXD_INV);
    uart_set_pin(KEYPAD_UART_NUM, GPIO_NUM_26, GPIO_NUM_27, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);

    start_server();
    //Create a task to handler UART event from ISR
    xTaskCreatePinnedToCore(uart_keypad_event_task, "uart_keypad_event_task", 4096, NULL, 12, NULL, 0);
    monitor_alarm();
    monitor_memory();

    // For protocol analysis, emulate panel to drive real keypad, see how it
    // reacts. To avoid comms error, periodically sends poll for keypress. 
    // New commands can be sent via web socket, see panel.html
    //xTaskCreate(&send_panel_messages, "send_panel_messages", 8096, NULL, 5, NULL);
}