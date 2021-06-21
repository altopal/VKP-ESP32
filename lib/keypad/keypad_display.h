#pragma once

int keypad_show_text(char *buf, int length, uint32_t flashing_characters);
int keypad_set_leds(short power, short warn, short alarm);
int keypad_set_backlight(short state);
int keypad_set_tone(short state);
int keypad_set_audio(uint8_t *words, uint8_t length);
int keypad_log(char *buf, int length);
