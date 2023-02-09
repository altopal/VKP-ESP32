/*
 Listen on pin 33 for alarm.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "keypad_alarm.h"
#include "keypad_send_alarm.h"
#include "keypad_log.h"

#define ALARM_INPUT_PIN_1 32
#define ALARM_INPUT_PIN_2 33
#define ESP_INTR_FLAG_DEFAULT 0
#define ALARM_STATE_IN_ALARM 1
#define ALARM_STATE_NO_ALARM 0
#define ALARM_MESSAGE_TITLE "House Alarm"
#define ALARM_MESSAGE_BODY "An alarm has triggered"
#define ALARM_DISARMED_MESSAGE_TITLE "House Alarm Disarmed"
#define ALARM_DISARMED_MESSAGE_BODY "An alarm disarmed"
#define PRE_ALARM_MESSAGE_TITLE "House Pre-Alarm"
#define PRE_ALARM_MESSAGE_BODY "A pre-alarm has triggered"
#define PRE_ALARM_DISARMED_MESSAGE_TITLE "House Pre-Alarm Disarmed"
#define PRE_ALARM_DISARMED_MESSAGE_BODY "A pre-alarm disarmed"

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void process_alarm(void* arg)
{
    uint32_t io_num;
    int level;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            level = gpio_get_level(io_num);
            info("GPIO[%d] intr, val: %d\n", io_num, level);
            if (io_num == ALARM_INPUT_PIN_1) {
                if (level == ALARM_STATE_IN_ALARM) {
                    send_alarm(ALARM_MESSAGE_TITLE, ALARM_MESSAGE_BODY);
                } else {
                    send_alarm(ALARM_DISARMED_MESSAGE_TITLE, ALARM_DISARMED_MESSAGE_BODY);
                }
            } else {
                if (level == ALARM_STATE_IN_ALARM) {
                    send_alarm(PRE_ALARM_MESSAGE_TITLE, PRE_ALARM_MESSAGE_BODY);
                } else {
                    send_alarm(PRE_ALARM_DISARMED_MESSAGE_TITLE, PRE_ALARM_DISARMED_MESSAGE_BODY);
                }
            }
        }
    }
}

void monitor_alarm()
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // Trigger interrupt on rising and falling edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;

    // bit mask of the pins to set
    io_conf.pin_bit_mask = (1ULL << ALARM_INPUT_PIN_1) | (1ULL << ALARM_INPUT_PIN_2);
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // enable pull-up mode
    io_conf.pull_up_en = 1;
    // configure GPIO with the given settings
    gpio_config(&io_conf);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    //start gpio task
    xTaskCreate(process_alarm, "process_alarm", 8192, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(ALARM_INPUT_PIN_1, gpio_isr_handler, (void*) ALARM_INPUT_PIN_1);
    gpio_isr_handler_add(ALARM_INPUT_PIN_2, gpio_isr_handler, (void*) ALARM_INPUT_PIN_2);
}