#include <stdint.h>

//typedef int (*panel_command_t)(uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);

typedef int (*send_panel_message_t)(uint8_t *response, uint16_t response_length);

int queue_messages(unsigned char *buf, uint16_t length);
int next_frame(send_panel_message_t send_panel_message);