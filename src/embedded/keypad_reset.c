/*
 Listen on pin 0 for reset. Three presses to reset NVS storage.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "keypad_storage.h"
#include "keypad_log.h"

#define RESET_INPUT_PIN 0
#define ESP_INTR_FLAG_DEFAULT 0
#define RESET_STATE_HIGH 0
#define RESET_STATE_LOW 1
#define RESET_STATE_COUNT 3

static xQueueHandle gpio_evt_queue = NULL;
static int press_count = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void process_reset(void* arg)
{
    uint32_t io_num;
    int level;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            level = gpio_get_level(io_num);
            info("GPIO[%d] intr, val: %d\n", io_num, level);
            if (level == RESET_STATE_HIGH) {
                press_count++;
            }
            if (press_count >= 3) {
                info("%s", "Resetting storage...\n");
                ESP_ERROR_CHECK(storage_reset());
                info("%s", "Restarting...\n");
                esp_restart();
            }
        }
    }
}

void monitor_reset()
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // Trigger interrupt on rising and falling edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;

    // bit mask of the pins to set
    io_conf.pin_bit_mask = 1ULL << RESET_INPUT_PIN;
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // enable pull-up mode
    io_conf.pull_up_en = 1;
    // configure GPIO with the given settings
    gpio_config(&io_conf);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    //start gpio task
    xTaskCreate(process_reset, "process_reset", 2048, NULL, 10, NULL);

    //install gpio isr service
    //gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(RESET_INPUT_PIN, gpio_isr_handler, (void*) RESET_INPUT_PIN);
}