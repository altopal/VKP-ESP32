#pragma once

#include <stdio.h>
#include <stdint.h>

typedef enum keypad_frame_state {
  FRAME_IDLE,
  FRAME_IN_TOKEN,
  FRAME_IN_COUNTER,
  FRAME_IN_COMMAND,
  FRAME_IN_COMMAND_LENGTH_SPACER,
  FRAME_IN_LENGTH,
  FRAME_IN_BODY,
} keypad_frame_state_t;

typedef enum keypad_frame_action {
  FRAME_IGNORE,
  FRAME_PROCESS,
} keypad_frame_action_t;

typedef enum keypad_frame_target {
  FRAME_TARGET_BROADCAST,
  FRAME_TARGET_THIS_KEYPAD,
  FRAME_TARGET_OTHER_KEYPAD,
} keypad_frame_target_t;

typedef enum keypad_command_key {
  UNKNOWN_01 = 0x01,
  SCAN_02 = 0x02,
  KEYPAD_ID_03 = 0x03,
  UNKNOWN_22 = 0x22,
  UPDATE_BACKLIGHT_23 = 0x23,
  QUERY_FIRMWARE_VERSION_29 = 0x29, // query firmware version
  KEYPRESS_40 = 0x40,
  QUERY_AUDIO_VERSION_41 = 0x41, // query audio firmware (and other info?)
  UNKNOWN_42 = 0x42,
  UPDATE_LEDS_45 = 0x45,
  DISPLAY_TEXT_50 = 0x50,
  PLAY_AUDIO_60 = 0x60,
  PLAY_TONE_61 = 0x61,
  UNKNOWN = 0xFF,
} keypad_command_key_t;

typedef struct keypad_frame keypad_frame_t;
typedef struct keypad_command keypad_command_t;

struct keypad_frame {
  uint8_t *buf;
  uint16_t index;
  uint8_t counter;
  keypad_command_t *command;
  uint8_t length;
  uint8_t checksum;
  keypad_frame_state_t state;
  keypad_frame_target_t target;
  keypad_frame_action_t action;
};

struct keypad_command {
  keypad_command_key_t key;
  char name[20];
  int (*process_command)(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
};

typedef int (*process_frame_response_t)(uint8_t *response, uint16_t response_length);

typedef enum keys {
  KEY_1 = 0x01,
  KEY_2 = 0x02,
  KEY_3 = 0x04,
  KEY_4 = 0x11,
  KEY_5 = 0x12,
  KEY_6 = 0x14,
  KEY_7 = 0x21,
  KEY_8 = 0x22,
  KEY_9 = 0x24,
  KEY_ASTRISK = 0x31,
  KEY_0 = 0x32,
  KEY_HASH = 0x34,
  KEY_PLAY_QUIT = 0x18,
  KEY_REC_YES = 0x28,
  KEY_LIGHT_NO = 0x38,
  KEY_UNKNOWN = 0xFF
} keypad_key_t;

int keypad_init(uint8_t *serial_number);

int press_key(keypad_key_t key);
int press_char(uint8_t c);
int clear_keys();
int read_key();
int process_byte(keypad_frame_t *frame, uint8_t value, process_frame_response_t process_frame_response);
int process_frame(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length);

int process_unknown(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_01(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_keypad_scan_02(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_keypad_id_03(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_22(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_update_backlight_23(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_query_firmware_version_29(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_poll_for_key_press_40(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_query_audio_version_41(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_42(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_update_leds_45(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_display_50(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_play_audio_60(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);
int process_play_tone_61(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum);

extern int response_delay_ms;