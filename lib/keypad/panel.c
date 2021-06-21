#include "panel.h"
#include "stdio.h"

uint8_t counter = 0x00;

#define PANEL_QUEUE_LENGTH 40
#define PANEL_MAX_MESSAGE_LENGTH 40

static uint8_t queue_buf[PANEL_QUEUE_LENGTH][PANEL_MAX_MESSAGE_LENGTH];
static uint8_t queue_next = 0;
static uint8_t queue_current = 0;

// static void print_buffer(uint8_t *buf, uint16_t length) {
//   printf("Q ");
//   for (int i = 0; i < length; i++) {
//     printf("%02X ", buf[i]);
//   }
//   for (int i = 0; i < 28-length; i++) {
//     printf("-- ");
//   }
//   printf("--:");

//   uint8_t c;
//   for (int i = 0; i < length; i++) {
//     c = buf[i] & 0x7F;
//     if (c >= 0x20) {
//       printf("%c", c);
//     } else {
//       printf(".");
//     }
//   }
//   printf("\n");
// }

int queue_messages(unsigned char *buf, uint16_t length) {
  unsigned char *c;
  uint8_t message_index = 0;
  //printf("%.*s\n", length, buf);
  c = buf;
  // 0 
  for (int i = 0; i < length; i++) {
    if (buf[i] == ' ' || buf[i] == '\n' || i == length - 1) {
      // Assume we have 2 chars, with c pointing to the first
      uint8_t v = 0;
      while (c < buf + i) {
        v = v << 4;
        if (*c >= '0' && *c <= '9') v |= *c - '0';
        else if (*c >= 'A' && *c <= 'F') v |= *c - 'A' + 10;
        c++;
      }
      queue_buf[queue_next][message_index++] = v;
      c = buf + i;
      if (buf[i] == '\n' || i == length - 1) {
        //print_buffer(queue_buf[queue_next], message_index);
        queue_next = ((queue_next + 1) % PANEL_QUEUE_LENGTH);
        message_index = 0;
      }
    }
  }
  return 0;
}

// int panel_frame(uint8_t *response, uint16_t *response_length, panel_command_t command) {
//   uint8_t val;
//   uint8_t response_checksum = 0;

//   counter++;
//   *response_length = 0;

//   response_checksum += val = 0x00; response[(*response_length)++] = val; 
//   response_checksum += val = 0x07; response[(*response_length)++] = val; 
//   response_checksum += val = 0x1E; response[(*response_length)++] = val; 
//   response_checksum += val = 0xFB; response[(*response_length)++] = val; 

//   command(response, response_length, &response_checksum);
//   response[(*response_length)++] = response_checksum; 
// }

// int panel_03(uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
//   uint8_t val;
//   (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
//   (*response_checksum) += val = 0x01; response[(*response_length)++] = val; 
//   (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
//   return 0;
// }

int next_frame(send_panel_message_t send_panel_message) {
  static uint8_t response[256];
  static uint16_t response_length;
  uint8_t response_checksum = 0;
  if (queue_current < queue_next) {
    uint8_t *buf = queue_buf[queue_current];
    uint8_t length = buf[7];
    uint8_t val;

    counter++;
    response_length = 0;

    for (int i = 0; i < 4; i++) {
      response_checksum += val = buf[i]; response[response_length++] = val; 
    }
    // Add counter byte, for broadcast messages bit 0x40 is also set in this byte.
    if (buf[0] == 0x00) {
      response_checksum += val = counter | 0x40; response[response_length++] = val; 
    } else {
      response_checksum += val = counter; response[response_length++] = val; 
    }

    for (int i = 5; i <= length + 7; i++) {
      response_checksum += val = buf[i]; response[response_length++] = val; 
    }
    response[response_length++] = response_checksum; 
    send_panel_message(response, response_length);
    queue_current++;
    return 1;
  } else {
    uint8_t val;
    counter++;
    response_length = 0;
    // 00 07 1E FB 54 40 00 01 08 BD 
    response_checksum += val = 0x0; response[response_length++] = val; 
    response_checksum += val = 0x07; response[response_length++] = val; 
    response_checksum += val = 0x1E; response[response_length++] = val; 
    response_checksum += val = 0xFB; response[response_length++] = val; 
    response_checksum += val = counter; response[response_length++] = val; 
    response_checksum += val = 0x40; response[response_length++] = val; 
    response_checksum += val = 0x00; response[response_length++] = val; 
    response_checksum += val = 0x01; response[response_length++] = val; 
    response_checksum += val = 0x08; response[response_length++] = val; 
    response[response_length++] = response_checksum; 
    send_panel_message(response, response_length);
    return 0;
  }
}

// int send_lots(panel_command_t command, process_panel_response_t panel_response) {
//   static uint8_t response[256];
//   static uint16_t response_length;
//   response_length = 0;
//   panel_frame(response, &response_length, command);
//   if (response_length > 0) {
//     panel_response(response, response_length);
//   }
//   return 0;
// }
