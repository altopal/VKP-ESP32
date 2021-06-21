# Protocol

## Communication
There are four terminals on the panel:
* +: 12V
* -: Ground
* A: 0 to 3.3V
* B: Inverse of A

It appears to use UART, 0V to 3.3V, but with RX and TX on one wire. Pin A is standard UART, 3.3V is logical 0, and 0V is logical 1. Pin B is the inverse of Pin A, so it looks like it is using balanced-differential transmission. I've had some success communicating with the panel using only Pin B, but in the end I've used both A and B, it will hopefully be more reliable.

## UART
It is using 8N1 UART at 19200 baud.

## Messages
The panel can send messages to individual keypads, which must respond to the message. The panel can also send broadcast messages which do not require any response.

A message from the panel looks like this:

    00 01 23 45 51 40 00 01 08 03

* Byte 0: 00 for messages to single keypad, FF for broadcast to all
* Bytes 1-3: The serial number of the keypad, or `00 00 00` for a broadcast to all
* Byte 4: First two bits, not sure. Last six bits are a counter.
* Byte 5: The command, see next section
* Byte 6: Always `0x00`
* Byte 7: The number of bytes in the body of this message (can be zero)
* Byte x-y: The body of the message, different for each command
* Last Byte: Checksum

A response from a keypad to the panel looks like this (actual response to the example message from panel above):

    00 01 23 45 51 40 00 04 04 00 0D 00 0F

It follows the same structure as a message from the panel. Bytes 0-6 are identical to the message from the panel. Byte 7 is again the number of bytes in the body, and the last byte is a checksum.

## Commands
  
### `0x01` I think indicates new scan starting
    FF 00 00 00 36 01 00 01 00 37

Broadcast message, so no response required
### `0x02` Scan for new keypad
    FF 00 00 00 68 02 00 08 00 00 00 00 00 FF FF FF __ 

* Body is always 8 bytes: `00 00 00 00 00 FF FF FF`

Response from keypad (if it has not responded since last 0x01 command)

    00 01 23 45 68 02 00 0A 00 01 23 45 01 61 FF FF FF FA __

Body is 10 bytes:
* Bytes 0-3: `00` followed by the serial number of our keypad
* Bytes 4-9: Always `01 61 FF FF FF FA` (not sure what they mean)

### `0x03` Assign/unassign keypad id
    00 01 23 45 41 03 00 01 01 AF

* Body `01`, keypad is being allocated an id on panel
* Body `00`, keypad deleted from the panel

Response from keypad to panel has empty body like this:

    00 01 23 45 41 03 00 00 AD

### `0x22` Don't know what this is

    00 01 23 45 4B 22 00 09 7F CD 02 02 01 01 05 05 01 3C

Response from keypad to panel has empty body:

    00 01 23 45 48 22 00 00 D3

### `0x23` Set backlight status

Body is 0x08 to turn backlight on, `0x10` to turn it off
Backlight on:

    00 01 23 45 4A 23 00 01 08 DF

Backlight off:
    00 01 23 45 4A 23 00 01 10 __

Response from keypad has empty body.

### `0x29` Query Keypad Firmware Version

Empty body in message from panel:

    00 01 23 45 49 29 00 00 DB

Response from keypad to panel looks like this:

    00 01 23 45 49 29 00 05 00 2B 04 00 01 10

* Body bytes 0-1: Always `00 2B`. Not sure what they mean
* Body bytes 2-4: The firmware version, e.g. `04 00 01` for version 4.0.1

### `0x40` Check for keypress

    00 01 23 45 51 40 00 01 08 03

Body is always 1 byte: `0x08`

Keypad response if there are no keypresses:

    00 01 23 45 51 40 00 04 04 00 0D 00 0F

Body is always these 4 bytes: `04 00 0D 00`

Keypad response if there is a keypress, as above, but with 6 bytes:

    __ __ __ __ __ __ __ __ 24 00 0D 01 01 01 __

* Byte 0 (0x24): Not sure about left four bits. The right four bits relate to tamper state, 0x4 is no-tamper, 0x6 tamper open?, 0xC tamper closed?
* Bytes 1-3: Always `00 0D 01`, not sure what it means
* Byte 4: The key that was pressed (see [Keycodes](./Keycodes.md))
* Byte 5: Always `01` (not sure what it means)

### `0x41` Query Keypad Audio Firmware Version

    00 01 23 45 5B 41 00 02 01 00 08

Response from keypad:

    00 01 23 45 5B 41 00 0A 01 00 00 02 01 39 B2 BB 00 00 B9

* Bytes 0-2: `01 00 00`. Not sure what they mean.
* Bytes 3-4: The audio firmware version, e.g. `02 01` for version 2.1
* Bytes 5-9: `39 B2 BB 00 00`. Not sure what they mean.

### `0x42` Don't know
From panel:

    00 01 23 45 61 42 00 01 00 0D

Keypad response:

    00 01 23 45 61 42 00 01 00 0D

### `0x45` Set LED status

    00 01 23 45 42 45 00 03 FF 55 55 9C

* Byte 0: Power LED
* Byte 1: Warning LED
* Byte 2: Alarm LED

* 0xFF: Solid
* 0x55: Flashing
* 0x00: Off

Response from keypad:

    00 01 23 45 43 45 00 02 00 00 __

### `0x50` Display Text

    00 01 23 45 44 50 00 13 10 20 10 4D 6F 6E 20 20 32 20 41 75 67 20 32 31 BA 31 37 CE

Above message is to display the text `Mon  2 Aug 21:17`, with flashing colon for the time.

* Bytes 0-2: `10 20 10`. Don't know what they mean
* Bytes 3-18: The text to display. For each byte:
    * First bit is 0 for regular text, 1 for flashing
    * Next 7 bits are ASCII codes

Response from keypad (only if this was a targetted message for this keypad):

    00 01 23 45 43 60 00 02 00 00 __

No response sent for a broadcast message.

### `0x60` Play Audio Messages

    00 01 23 45 62 60 00 0E 00 02 00 00 00 0A 00 00 02 0A 00 00 00 02 __

Above message plays audio: `System Unset`

Message body length can vary. I'm not sure the exact format of this message, needs more work.

See [Audio Messages](./AudioMessages.md)

Response from keypad:

    00 01 23 45 43 60 00 02 00 00 __

### `0x61` Play Tone

Long tone (e.g. when Arming):

    00 01 23 45 78 61 00 11 01 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 03 1F

Short beep:

    00 01 23 45 79 61 00 11 01 03 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 5B

Turn off tone:

    00 01 23 45 43 61 00 11 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 1E

Response from keypad:

    00 01 23 45 43 61 00 02 00 00 __