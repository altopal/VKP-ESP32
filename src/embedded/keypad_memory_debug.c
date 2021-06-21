/*
 Monitor memory used.
*/
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keypad_display.h"
#include "keypad_log.h"

#define RESET_INPUT_PIN 0
#define ESP_INTR_FLAG_DEFAULT 0

static void process_memory(void* arg)
{
    for(;;) {
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      uint32_t free_heap = xPortGetFreeHeapSize();
      int64_t uptimeMillis = esp_timer_get_time() / 1000;
      char memory_message[100];
      int memory_message_length = sprintf(memory_message, "{\"uptime\": %lld, \"freeHeap\": %d }", uptimeMillis, free_heap);
      info("Free Heap: %d\n", free_heap);
      keypad_log(memory_message, memory_message_length);
    }
}

void monitor_memory()
{
    // start memory monitor task
    xTaskCreate(process_memory, "process_memory", 2048, NULL, 10, NULL);
}