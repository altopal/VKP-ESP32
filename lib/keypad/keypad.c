#include "queue.h"
#include "keypad.h"
#include "keypad_display.h"
#include "keypad_log.h"

#define KEY_QUEUE_SIZE 20

static uint8_t queue_buf[KEY_QUEUE_SIZE];
static queue_t key_queue = {0, 0, KEY_QUEUE_SIZE, queue_buf};

static uint8_t BROADCAST_FRAME_TOKEN[] = {0xFF, 0x00, 0x00, 0x00};
//static uint8_t KEYPAD_FRAME_TOKEN[] = {0x00, 0x07, 0x1E, 0xFB};
static uint8_t KEYPAD_FRAME_TOKEN[] = {0x00, 0x01, 0x23, 0x45};
static const int KEYPAD_FRAME_TOKEN_LENGTH = sizeof(KEYPAD_FRAME_TOKEN) / sizeof(int8_t);
static const int FRAME_LENGTH_PADDING = 9;

static uint8_t response_buffer[256];
static uint16_t response_buffer_length = sizeof(response_buffer) / sizeof(uint8_t);
static uint8_t processed_42 = 0;

static keypad_command_t keypad_commands[] = {
  {.key = KEYPRESS_40, .name = "Keypress 40", .process_command = &process_poll_for_key_press_40},
  {.key = DISPLAY_TEXT_50, .name = "Display Text 50", .process_command = &process_display_50},
  {.key = UNKNOWN_01, .name = "StartScan 01?", .process_command = &process_01},
  {.key = SCAN_02, .name = "Scan 02", .process_command = &process_keypad_scan_02},
  {.key = KEYPAD_ID_03, .name = "KeypadId", .process_command = &process_keypad_id_03},
  {.key = UNKNOWN_22, .name = "Unknown22", .process_command = &process_22},
  {.key = UPDATE_BACKLIGHT_23, .name = "Update Backlight 23", .process_command = &process_update_backlight_23}, // Backlight on: 23 00 01 08 / off: 23 00 01 10 
  {.key = QUERY_FIRMWARE_VERSION_29, .name = "Query Firmware 29", .process_command = &process_query_firmware_version_29},
  {.key = QUERY_AUDIO_VERSION_41, .name = "Query Audio 41", .process_command = &process_query_audio_version_41},
  {.key = UNKNOWN_42, .name = "Unknown42", .process_command = &process_42},
  {.key = UPDATE_LEDS_45, .name = "Update LEDs 45", .process_command = &process_update_leds_45}, // LED: 45 00 03 FF FF FF. FF = solid, 55 = flashing, one byte for each LED
  {.key = PLAY_AUDIO_60, .name = "Play Audio 60", .process_command = &process_play_audio_60}, // Play audio
  {.key = PLAY_TONE_61, .name = "Play Tone 61", .process_command = &process_play_tone_61},
  {.key = UNKNOWN, .name = "Unseen"},
};

const int KEYPAD_COMMANDS_LENGTH = sizeof(keypad_commands) / sizeof(keypad_command_t);
static const int FRAME_TOKEN_LENGTH = 6;
static uint8_t keypadId = 0;

int keypad_init(uint8_t *serial_number) {
  KEYPAD_FRAME_TOKEN[0] = serial_number[0];
  KEYPAD_FRAME_TOKEN[1] = serial_number[1];
  KEYPAD_FRAME_TOKEN[2] = serial_number[2];
  KEYPAD_FRAME_TOKEN[3] = serial_number[3];
  return 0;
}

static void print_buffer(uint8_t *buf, uint16_t length) {
  debug("%s", "P ");
  for (int i = 0; i < length; i++) {
    debug("%02X ", buf[i]);
  }
  for (int i = 0; i < 28-length; i++) {
    debug("%s", "-- ");
  }
  debug("%s", "--:");

  uint8_t c;
  for (int i = 0; i < length; i++) {
    c = buf[i] & 0x7F;
    if (c >= 0x20) {
      debug("%c", c);
    } else {
      debug("%s", ".");
    }
  }
  debug("%s", "\n");
}

static const int RESET_FRAME_RESULT = -10;

int process_frame(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length) {
  print_buffer(frame->buf, frame->index+1);
  // Not for us, just log it and return with no response.
  if (frame->target == FRAME_TARGET_OTHER_KEYPAD) {
    (*response_length) = 0;
    return 0;
  }
  uint8_t checksum = frame->buf[frame->index];
  debug("Checking checksum... %02X %02X\n", checksum, frame->checksum);
  if (checksum != frame->checksum && !(checksum == 0xFF && frame->checksum == 0x0)) {
    // Checksum doesn't match, reject
    warn("Checksum mismatch, rejecting. actual: %02X calculated: %02X\n", checksum, frame->checksum);
    return RESET_FRAME_RESULT;
  }
  debug("Checking length... %d %d\n", frame->length + FRAME_LENGTH_PADDING, frame->index+1);
  if (frame->length + FRAME_LENGTH_PADDING != frame->index+1) {
    return 2;
  }
  debug("%s", "Creating response...\n");
  uint8_t response_checksum = 0x0;
  // All start by copying back frame token/counter/command
  *response_length = FRAME_TOKEN_LENGTH;
  for (int i = 0; i < 6; i++) {
    response[i] = frame->buf[i];
    response_checksum += response[i];
  }
  if (frame->command->key != UNKNOWN) {
    debug("Processing command %s...\n", frame->command->name);
    frame->command->process_command(frame, response, response_length, &response_checksum);
  } else {
    debug("%s", "Processing unknown command %s...\n");
    process_unknown(frame, response, response_length, &response_checksum);
  }
  debug("%s", "Adding checksum...\n");
  // Add checksum to the end.
  if ((*response_length) > 0) {
    response[(*response_length)++] = response_checksum;
  }

  return 0;
}

int process_unknown(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  warn("Unknown command: %X\n", frame->buf[5]);
  (*response_length) = 0;
  return 0;
}

static char display_characters[32];
static uint32_t flashing_characters = 0;

int process_display_50(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  const int START_INDEX = 5 + FRAME_TOKEN_LENGTH;
  int length = 0;
  uint8_t *buf = frame->buf;
  flashing_characters = 0;
  for (int i = START_INDEX; i < frame->index && length < sizeof(display_characters); i++) {
    display_characters[i-START_INDEX] = buf[i] & 0x7F;
    flashing_characters |= (buf[i] & 0x80) > 0 ? (1 << (i - START_INDEX)) : 0;
    length++;
  }
  info("%.*s %X\n", length, display_characters, flashing_characters);
  keypad_show_text(display_characters, length, flashing_characters);
  if (frame->target == FRAME_TARGET_THIS_KEYPAD) {
    response[(*response_length)++] = 0x0;
    response[(*response_length)++] = 0x0;
  } else {
    (*response_length) = 0;
  }
  return 0;
}

// For easier testing and separate of responsibility, only pass buffer and buffer length instead
// of passing entire frame struct
int process_poll_for_key_press_40(keypad_frame_t *frame,  uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  // For regular poll for keys, we get this:
  //            00 01 08
  // When we are in engineering menu we get this (bit 0x02 also set on last byte)
  //            00 01 0A
  // Don't have a good reason to set them differently, maybe it indicates a specific LED should be lighting?
  // We could just ignore this validation step until we understand what these bytes mean.
  uint8_t *buf = frame->buf + FRAME_TOKEN_LENGTH;
  if (buf[0] == 0x0 && buf[1] == 0x01 && ((buf[2] & 0x08) == 0x08)) {
    // Valid poll for key press, respond with next key.
    uint8_t key = queue_read(&key_queue);
    uint8_t val = 0x0;
    if (key == NO_CHAR) {
      (*response_checksum) += val = 0x0; response[(*response_length)++] = val;
      // This is the length of the number of bytes in the data segment of the frame
      (*response_checksum) += val = 0x4; response[(*response_length)++] = val;
      (*response_checksum) += val = 0x4; response[(*response_length)++] = val;
      (*response_checksum) += val = 0x0; response[(*response_length)++] = val;
      // Don't know if this is actually necessary, can probably remove and use 0xD always
      if (processed_42) {
        (*response_checksum) += val = 0xD; response[(*response_length)++] = val;
      } else {
        (*response_checksum) += val = 0xC; response[(*response_length)++] = val;
      }
      (*response_checksum) += val = 0x0; response[(*response_length)++] = val;
    } else {
      (*response_checksum) += val = 0x0; response[(*response_length)++] = val;
      // This is the length of the number of bytes in the data segment of the frame
      (*response_checksum) += val = 0x6; response[(*response_length)++] = val;
      // At least part of this relates to tamper, the right 4 bits:
      // 0x4 = no tamper
      // 0x6 = keypad opened
      // 0xC = keypad closed? (send just before returing to 0x4)
      // Not sure what the left 4 bits are for
      (*response_checksum) += val = 0x24; response[(*response_length)++] = val;
      (*response_checksum) += val = 0x0; response[(*response_length)++] = val;
      (*response_checksum) += val = 0x0D; response[(*response_length)++] = val;
      (*response_checksum) += val = 0x01; response[(*response_length)++] = val;
      (*response_checksum) += val = key; response[(*response_length)++] = val;
      (*response_checksum) += val = 0x01; response[(*response_length)++] = val;
    }
  }
  return 0;
}

int press_key(keypad_key_t key) {
  return queue_write(&key_queue, key);
}

int press_char(uint8_t c) {
  keypad_key_t key = KEY_UNKNOWN;
  switch (c)
  {
  case '0': key = KEY_0; break;
  case '1': key = KEY_1; break;
  case '2': key = KEY_2; break;
  case '3': key = KEY_3; break;
  case '4': key = KEY_4; break;
  case '5': key = KEY_5; break;
  case '6': key = KEY_6; break;
  case '7': key = KEY_7; break;
  case '8': key = KEY_8; break;
  case '9': key = KEY_9; break;
  case '*': key = KEY_ASTRISK; break;
  case '#': key = KEY_HASH; break;
  case 'Y': key = KEY_REC_YES; break;
  case 'N': key = KEY_LIGHT_NO; break;
  case 'Q': key = KEY_PLAY_QUIT; break;
  default:
    warn("press_char unknown key: %c\n", c);
    break;
  }
  if (key != KEY_UNKNOWN) {
    return press_key(key);
  }
  
  return 1;
}

int clear_keys() {
  while (queue_read(&key_queue) != NO_CHAR);
  return 0;
}

int read_key() {
  return queue_read(&key_queue);
}


static inline int process_idle_byte(keypad_frame_t *frame, uint8_t value) {
  if (KEYPAD_FRAME_TOKEN[0] == value) {
    // Start of frame addressed to specific keypad, assume initially
    // it is this keypad until we determine otherwise
    frame->target = FRAME_TARGET_THIS_KEYPAD;
    frame->state = FRAME_IN_TOKEN;
    frame->buf[(frame->index)++] = value; 
    frame->checksum += value;
  } else if (BROADCAST_FRAME_TOKEN[0] == value) {
    // Start of broadcast frame
    frame->target = FRAME_TARGET_BROADCAST;
    frame->state = FRAME_IN_TOKEN;
    frame->buf[(frame->index)++] = value; 
    frame->checksum += value;
  }
  return 0;
}

static inline int process_in_token_byte(keypad_frame_t *frame, uint8_t value) {
  // So far it looks like a keypad frame for this keypad, check if next value
  // also matches our ID. If not, change target to OTHER_KEYPAD
  if (frame->target == FRAME_TARGET_THIS_KEYPAD && KEYPAD_FRAME_TOKEN[frame->index] != value) {
    frame->target = FRAME_TARGET_OTHER_KEYPAD;
  }
  // If it is a keypad targetted frame, accept any value (so we can log
  // other keypad frames for debugging), otherwise check if it is valid
  // broadcast token value.
  if (frame->target != FRAME_TARGET_BROADCAST || BROADCAST_FRAME_TOKEN[frame->index] == value) {
    debug(" Found token char %X\n", value);
    frame->buf[(frame->index)++] = value; 
    frame->checksum += value;
    if (frame->index == KEYPAD_FRAME_TOKEN_LENGTH) {
      debug("%s", "Last token char, changing state\n");
      // End of token, next will be Counter byte
      frame->state = FRAME_IN_COUNTER;
    }
  } else {
    // error, drop frame and restart
    frame->buf[(frame->index)++] = value; 
    warn("Invalid byte in frame token, rejecting %02X\n", value);
    return RESET_FRAME_RESULT;
  }
  return 0;
}

static int _process_byte(keypad_frame_t *frame,
                  uint8_t value,
                  process_frame_response_t process_frame_response) {
  debug("state %d index %d 0x%02X\n", frame->state, frame->index, value);
  uint8_t new_counter = value & 0x3F;
  switch (frame->state) {
    case FRAME_IDLE:
      return process_idle_byte(frame, value);
      break;
    case FRAME_IN_TOKEN:
      return process_in_token_byte(frame, value);
      break;
    case FRAME_IN_COUNTER:
      // Check if we already processed this frame from the panel. If we did,
      // this frame is a response from a keypad.
      if ((new_counter == frame->counter) || (((new_counter + 0x40 - frame->counter) % 0x40) >= 0x10)) {
        debug(" Frame counter already processed, ignoring 0x%02X 0x%02X 0x%02X.\n", frame->counter, new_counter, (new_counter + 0x40 - frame->counter));
        frame->action = FRAME_IGNORE;
      } else {
        frame->action = FRAME_PROCESS;
        // Check if we've missed a frame, it increments the counter, and
        // wraps from 0x7F back to 0x40
        if (frame->counter != (new_counter - 1) && (frame->counter != 0x3F && new_counter != 0x0)) {
          warn("  Unexpected counter, we might have missed something: 0x%02X -> 0x%02X\n", frame->counter, new_counter);
        } else {
          debug("%s\n", " Frame counter OK\n");
        }
        frame->counter = new_counter;
      }
      // New frame from panel
      frame->buf[(frame->index)++] = value; 
      frame->checksum += value;
      frame->state = FRAME_IN_COMMAND;
    break;
    case FRAME_IN_COMMAND:
      frame->buf[(frame->index)++] = value; 
      frame->checksum += value;
      frame->state = FRAME_IN_COMMAND_LENGTH_SPACER;
      frame->command = keypad_commands + KEYPAD_COMMANDS_LENGTH - 1;
      for (int i = 0; i < KEYPAD_COMMANDS_LENGTH; i++) {
        if (value == keypad_commands[i].key) {
          frame->command = keypad_commands + i;
          break;
        }
      }
      debug("Found command %s 0x%02X.\n", frame->command->name, value);
    break;
    case FRAME_IN_COMMAND_LENGTH_SPACER:
      if (value == 0x0) {
        frame->buf[(frame->index)++] = value; 
        frame->checksum += value;
        frame->state = FRAME_IN_LENGTH;
      } else {
        // We are not actually in a frame, check if we encountered another possible frame token
        // already and move on to that. Add current value at the end to ensure it is included
        // in this check.
        warn("Invalid spacer byte, rejecting %02X\n", value);
        frame->buf[(frame->index)++] = value; 
        return RESET_FRAME_RESULT;
      }
    break;
    case FRAME_IN_LENGTH:
      frame->buf[(frame->index)++] = value; 
      frame->checksum += value;
      frame->state = FRAME_IN_BODY;
      frame->length = value;
    break;
    case FRAME_IN_BODY:
      frame->buf[(frame->index)++] = value; 
      if (frame->index == frame->length + FRAME_LENGTH_PADDING) {
        debug("%s", "  Reached end of data segment for command.\n");
        frame->index = frame->index - 1;
        int result = 0;
        if (frame->action == FRAME_PROCESS) {
          // Reached end of command, ready to process
          result = process_frame(frame, response_buffer, &response_buffer_length);
          if (result == 0 && response_buffer_length > 0) {
            // Frame was process and response buffer filled, go ahead and process the response (i.e. send to panel)
            result = process_frame_response(response_buffer, response_buffer_length);
          } else if (result == RESET_FRAME_RESULT) {
            // Checksum didn't match, we'll scan over the bytes in our buffer to see if
            // a frame started in the middle somewhere.
            return result;
          }
        } else if (frame->action == FRAME_IGNORE) {
          print_buffer(frame->buf, frame->index+1);
        }
        // Clean reset after receiving valid frame
        frame->index = 0;
        frame->state = FRAME_IDLE;
        frame->checksum = 0x0;
        return result;
      } else {
        frame->checksum += value;
      }
    break;
    default:
    break;
  }
  return 0;
}

int process_byte(keypad_frame_t *frame,
                  uint8_t value,
                  process_frame_response_t process_frame_response) {
  int result = _process_byte(frame, value, process_frame_response);
  // Not proper structure for frame, retry starting with second byte
  // in buffer in case a frame started in the middle of our buffer.
  if (result == RESET_FRAME_RESULT) {
    uint16_t index = frame->index;
    warn("Reset frame %d: ", index);
    print_buffer(frame->buf, frame->index);
    frame->checksum = 0;
    frame->index = 0;
    frame->state = FRAME_IDLE;

    for (int i = 1; i < index; i++) {
      result = _process_byte(frame, frame->buf[i], process_frame_response);
      if (result == RESET_FRAME_RESULT) {
        frame->checksum = 0;
        frame->index = 0;
        frame->state = FRAME_IDLE;
      }
    }
    warn(" After reset %d: ", frame->index);
    if (frame->index > 0) {
      print_buffer(frame->buf, frame->index);
    } else {
      warn("%s", "\n");
    }
  }
  return result;
}

int process_01(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  // No response
  (*response_length) = 0;
  return 0;
}

int process_keypad_scan_02(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  // Send my id and other guff, complete frame:
  // 00 07 1E FB 42 02 00 0A 00 07 1E FB 01 61 FF FF FF FA E7
  if (!keypadId) {
    uint8_t val;
    (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
    (*response_checksum) += val = 0x0A; response[(*response_length)++] = val; 
    for (int i = 0; i < KEYPAD_FRAME_TOKEN_LENGTH; i++) {
      (*response_checksum) += val = KEYPAD_FRAME_TOKEN[i]; response[(*response_length)++] = val; 
    }
    (*response_checksum) += val = 0x01; response[(*response_length)++] = val; 
    (*response_checksum) += val = 0x61; response[(*response_length)++] = val; 
    (*response_checksum) += val = 0xFF; response[(*response_length)++] = val; 
    (*response_checksum) += val = 0xFF; response[(*response_length)++] = val; 
    (*response_checksum) += val = 0xFF; response[(*response_length)++] = val; 
    (*response_checksum) += val = 0xFA; response[(*response_length)++] = val; 
  } else {
    // Already registered, ignore
    (*response_length) = 0;
  }
  return 0;
}

int process_keypad_id_03(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  if (frame->buf[2 + FRAME_TOKEN_LENGTH] == 1) {
    keypadId = 1;
  } else {
    // Assume values other than 1 mean this keypad should be deleted.
    keypadId = 0;
  }
  response[(*response_length)++] = 0;
  response[(*response_length)++] = 0;
  return 0;
}

int process_22(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  response[(*response_length)++] = 0;
  response[(*response_length)++] = 0;
  return 0;
}

int process_update_backlight_23(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  uint8_t state = frame->buf[8];
  if (state == 0x08) {
    keypad_set_backlight(1);
  } else {
    keypad_set_backlight(0);
  }
  response[(*response_length)++] = 0;
  response[(*response_length)++] = 0;
  return 0;
}

int process_query_firmware_version_29(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  uint8_t val;
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val;
  (*response_checksum) += val = 0x05; response[(*response_length)++] = val;
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val;
  (*response_checksum) += val = 0x2B; response[(*response_length)++] = val;
  // Firmware Version: 4.0.1
  (*response_checksum) += val = 0x04; response[(*response_length)++] = val;
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val;
  (*response_checksum) += val = 0x01; response[(*response_length)++] = val;
  return 0;
}

int process_query_audio_version_41(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  uint8_t val;
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0x0A; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0x01; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
  // Audio firmware version: 2.1
  (*response_checksum) += val = 0x02; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0x01; response[(*response_length)++] = val; 

  (*response_checksum) += val = 0x39; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0xB2; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0xBB; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
  return 0;
}

int process_42(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  uint8_t val;
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0x01; response[(*response_length)++] = val; 
  (*response_checksum) += val = 0x00; response[(*response_length)++] = val; 
  processed_42 = 1;
  return 0;
}

int process_update_leds_45(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  uint8_t power = frame->buf[8];
  uint8_t warn = frame->buf[9];
  uint8_t alarm = frame->buf[10];

  keypad_set_leds(power == 0xFF ? 2 : (power == 0x55 ? 1 : 0),
                  warn  == 0xFF ? 2 : (warn  == 0x55 ? 1 : 0),
                  alarm == 0xFF ? 2 : (alarm == 0x55 ? 1 : 0));
  response[(*response_length)++] = 0;
  response[(*response_length)++] = 0;
  return 0;
}

/**
 * // System Arming (and plays tone)
 * 60 00 0E 00 02 00 00 00 0A 00 00 01 0A 00 00 00 01 33
 * // System Unset (tone stops)
 * 60 00 0E 00 02 00 00 00 0A 00 00 02 0A 00 00 00 02 20
 * // Panel Battery Fault, Call Engineer
 * 60 00 1A 00 04 00 00 57 0A 00 00 35 0A 00 00 41 1E 00 01 2F 0A 00 57 00 35 00 41 4E 20 65
 */
int process_play_audio_60(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  response[(*response_length)++] = 0;
  response[(*response_length)++] = 0;
  return 0;
}

/**
 * Play Tone:
 *   61 00 11 01 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 1F
 * Stop playing tone:
 *   61 00 11 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 06
 * 
 * Short beeps (on tamper for example):
 *   61 00 11 01 03 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 11
 */
int process_play_tone_61(keypad_frame_t *frame, uint8_t *response, uint16_t *response_length, uint8_t *response_checksum) {
  keypad_set_tone(frame->buf[8]);
  response[(*response_length)++] = 0;
  response[(*response_length)++] = 0;
  return 0;
}