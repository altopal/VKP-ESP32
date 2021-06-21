#pragma once

#include <stdlib.h>
#include <stdint.h>

#define NO_CHAR 0xFF

typedef struct {
    size_t head;
    size_t tail;
    size_t size;
    uint8_t *data;
} queue_t;

uint8_t queue_read(queue_t *queue);

int queue_write(queue_t *queue, uint8_t data);