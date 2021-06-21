// A simple fifo queue (or ring buffer) in c.
// This implementation \should be\ "thread safe" for single producer/consumer with atomic writes of size_t.
// This is because the head and tail "pointers" are only written by the producer and consumer respectively.
// Demonstrated with void pointers and no memory management.
// Note that empty is head==tail, thus only QUEUE_SIZE-1 entries may be used.

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

uint8_t queue_read(queue_t *queue) {
    if (queue->tail == queue->head) {
        return NO_CHAR;
    }
    uint8_t val = queue->data[queue->tail];
    queue->data[queue->tail] = 0;
    queue->tail = (queue->tail + 1) % queue->size;
    return val;
}

int queue_write(queue_t *queue, uint8_t data) {
    if (data == NO_CHAR) {
        // We are only using this queue to store limited set of keys,
        // 0xFF is not one of those. Don't allow it to be added to the
        // queue.
        return -2;
    }
    if (((queue->head + 1) % queue->size) == queue->tail) {
        return -1;
    }
    queue->data[queue->head] = data;
    queue->head = (queue->head + 1) % queue->size;
    return 0;
}