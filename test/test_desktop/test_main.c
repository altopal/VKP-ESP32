#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"
#include "keypad.h"

uint8_t FRAME_TOKEN[] = {0x00, 0x07, 0x1E, 0xFB};
const int FRAME_TOKEN_LENGTH = sizeof(FRAME_TOKEN) / sizeof(int8_t);
uint8_t BROADCAST_FRAME_TOKEN[] = {0xFF, 0x00, 0x00, 0x00};
const int BROADCAST_FRAME_TOKEN_LENGTH = sizeof(BROADCAST_FRAME_TOKEN) / sizeof(int8_t);

void print_frame(uint8_t *buffer, int length)
{
  for (int i = 0; i < length; i++) {
    printf("%02X ", buffer[i]);
  }
  printf("%*s", 150 - (length*3), "");
  uint8_t c;
  for (int i = 0; i < length; i++) {
    c = buffer[i] & 0x7F;
    if (c >= 0x20 && c < 0x7F) {
      printf("%c", c);
    } else {
      printf(".");
    }
  }
  printf("\n");
}

void logger_process_byte(uint8_t *buf
                , int *index
                , int *token_index
                , int *broadcast_token_index
                , uint8_t value
                , int *line_number)
{

  // 0 = not in token
  // 1 = in regular token
  // 2 = in control token
  // 3 = in frame
  static state = 0;
  //printf("%d %02X %d %02X\n", source, value, *token_index, token[*token_index]);
  buf[(*index)++] = value;
  if (FRAME_TOKEN[*token_index] == value) {
    //printf(" Found token\n");
    // Have we found complete frame token?
    if (++(*token_index) == FRAME_TOKEN_LENGTH && (*index) > FRAME_TOKEN_LENGTH) {
      // Found token
      print_frame(buf, (*index) - FRAME_TOKEN_LENGTH);
      // Set up next frame with frame starting marker
      for (int i = 0; i < FRAME_TOKEN_LENGTH; i++) {
        buf[i] = FRAME_TOKEN[i];
      }
      (*index) = FRAME_TOKEN_LENGTH;
      (*token_index) = 0;
      (*broadcast_token_index) = 0;
    }
  } else {
    (*token_index) = 0;
  }
  if (BROADCAST_FRAME_TOKEN[*broadcast_token_index] == value) {
       // Have we found complete frame token?
    if (++(*broadcast_token_index) == BROADCAST_FRAME_TOKEN_LENGTH && (*index) > BROADCAST_FRAME_TOKEN_LENGTH) {
      // Found token
      print_frame(buf, (*index) - BROADCAST_FRAME_TOKEN_LENGTH);
      // Set up next frame with frame starting marker
      for (int i = 0; i < BROADCAST_FRAME_TOKEN_LENGTH; i++) {
        buf[i] = BROADCAST_FRAME_TOKEN[i];
      }
      (*index) = BROADCAST_FRAME_TOKEN_LENGTH;
      (*broadcast_token_index) = 0;
      (*token_index) = 0;
    }
  } else {
    (*broadcast_token_index) = 0;
  }
}

void test_open_file() {
  FILE *fptr;
  fptr = fopen("/Users/ahamilt/Documents/PlatformIO/Projects/SerialLoggerESP/test/test_desktop/startup.log", "r");
  TEST_ASSERT_NOT_EQUAL(NULL, fptr);
  int32_t value;
  uint8_t fbuf[200];
  int index = 0;
  char input_line_buf[10];

  int frame_token_index = 0;
  int broadcast_frame_token_index = 0;
  int line_number = 0;

  while (fgets(input_line_buf, sizeof(input_line_buf), fptr) != NULL) {
    int matches = sscanf(input_line_buf, "%2X*", &value);
    if (matches == 1) {
      logger_process_byte(
                 fbuf
               , &index
               , &frame_token_index
               , &broadcast_frame_token_index
               , value
               , &line_number);
    }
  }
  fclose(fptr);
}

void test_foo() {
  TEST_ASSERT_EQUAL(1, 1);
}

void test_poll_for_key_press_no_key() {
  uint8_t buf[] = {0x00,0x01,0x08};
  uint16_t buf_length = sizeof(buf);
  uint8_t tx[300];
  uint16_t tx_len;
  uint8_t tx_checksum = 0x0;
  int result = process_poll_for_key_press(buf, buf_length, tx, &tx_len, &tx_checksum);
  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(6, tx_len);
  uint8_t expected_response[] = {0x00,0x04, 0x04, 0x00, 0x0D, 0x00};
  TEST_ASSERT_EQUAL_INT8_ARRAY(expected_response, tx, tx_len);
}

void test_press_key() {
  TEST_ASSERT_EQUAL(NO_CHAR, read_key());
  TEST_ASSERT_EQUAL(-2, press_key(NO_CHAR));
  TEST_ASSERT_EQUAL(NO_CHAR, read_key());
  press_key(KEY_1);
  TEST_ASSERT_EQUAL(KEY_1, read_key());
  TEST_ASSERT_EQUAL(NO_CHAR, read_key());
  keypad_key_t keys[] = {KEY_6, KEY_2, KEY_ASTRISK, KEY_HASH, KEY_LIGHT_NO, KEY_PLAY_QUIT, KEY_REC_YES, KEY_7};
  for (int i = 0; i < sizeof(keys) / sizeof(keypad_key_t); i++) {
    press_key(keys[i]);
  }
  for (int i = 0; i < sizeof(keys) / sizeof(keypad_key_t); i++) {
    TEST_ASSERT_EQUAL(keys[i], read_key());
  }
  TEST_ASSERT_EQUAL(NO_CHAR, read_key());
  TEST_ASSERT_EQUAL(NO_CHAR, read_key());
}

void test_poll_for_key_press_with_keys() {
  uint8_t rx[] = {0x00,0x01,0x08};
  uint16_t rx_len = 3;
  uint8_t tx[200];
  uint16_t tx_len = 0;
  // Test pressing key 1 returns key 1 in response
  press_key(KEY_1);
  uint8_t tx_checksum = 0x0;
  int result = process_poll_for_key_press(rx, rx_len, tx, &tx_len, &tx_checksum);
  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(8, tx_len);
  uint8_t expected_response[] = {0x00,0x06, 0x24, 0x00, 0x0D, 0x01, 0x01, 0x01};
  TEST_ASSERT_EQUAL_INT8_ARRAY(expected_response, tx, tx_len);

  // Test with two keys in buffer
  press_key(KEY_7);
  press_key(KEY_0);

  // KEY_7 expected in first response
  expected_response[6] = KEY_7;
  tx_checksum = 0x0;
  memset(tx, 0, tx_len);
  tx_len = 0;
  result = process_poll_for_key_press(rx, rx_len, tx, &tx_len, &tx_checksum);
  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(8, tx_len);
  TEST_ASSERT_EQUAL_INT8_ARRAY(expected_response, tx, tx_len);

  // KEY_0 expected in first response
  expected_response[6] = KEY_0;
  tx_checksum = 0x0;
  memset(tx, 0, tx_len);
  tx_len = 0;
  result = process_poll_for_key_press(rx, rx_len, tx, &tx_len, &tx_checksum);
  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(8, tx_len);
  TEST_ASSERT_EQUAL_INT8_ARRAY(expected_response, tx, tx_len);

  // No key in last response
  tx_checksum = 0x0;
  memset(tx, 0, tx_len);
  tx_len = 0;
  result = process_poll_for_key_press(rx, rx_len, tx, &tx_len, &tx_checksum);
  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(6, tx_len);
  uint8_t expected_response_no_key[] = {0x00,0x04, 0x04, 0x00, 0x0D, 0x00};
  TEST_ASSERT_EQUAL_INT8_ARRAY(expected_response_no_key, tx, tx_len);
}

void test_display() {
  uint8_t rx[] = {0x00,0x13,0x09,0x00,0x10,0x55,0x73,0x65,0x72,0x20,0x43,0x6F,0x64,0x65,0x20,0x2A,0x2A,0x2A,0xAD,0x2D,0x2D};
  uint16_t rx_len = 21;
  uint8_t tx[300];
  uint16_t tx_len;
  uint8_t tx_checksum = 0x0;
  int result = process_display(rx, rx_len, tx, &tx_len, &tx_checksum);
  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(2, tx_len);
  TEST_ASSERT_EQUAL(0x0, tx[0]);
  TEST_ASSERT_EQUAL(0x0, tx[1]);
}

static uint8_t tx_buf[256];
static uint16_t tx_buf_len = sizeof(tx_buf);
int copy_tx(uint8_t *tx, uint16_t length) {
  if (length <= tx_buf_len) {
    for (int i = 0; i < length; i++) {
      tx_buf[i] = tx[i];
    }
  }
  return 0;
}

void test_process_byte() {
  uint8_t rx_frame[] = {0x00,0x07,0x1E,0xFB,0x40, 0x03,0x00,0x01,0x01,0x65};
  uint8_t buf[256];
  keypad_frame_t frame = {
    .buf = buf,
    .index = 0,
    .checksum = 0,
    .state = FRAME_IDLE,
    .command = NULL,
    .counter = 0x7F,
  };
  for (int i = 0; i < sizeof(rx_frame); i++) {
    process_byte(&frame, rx_frame[i], &copy_tx);
  }
  TEST_ASSERT_EQUAL_INT8_ARRAY(rx_frame, tx_buf, 6);
}


int main() {
  UNITY_BEGIN();
  RUN_TEST(test_foo);
  RUN_TEST(test_open_file);
  RUN_TEST(test_press_key);
  RUN_TEST(test_display);
  RUN_TEST(test_poll_for_key_press_no_key);
  RUN_TEST(test_poll_for_key_press_with_keys);
  RUN_TEST(test_process_byte);
  UNITY_END();
  return 0;
}
