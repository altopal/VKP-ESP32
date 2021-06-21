# Virtual Remote Keypad

## Overview
This project allows an ESP32 board to be used as a Remote Keypad for the HKC SecureWave intruder alarm.

It was inspired by [OzmoOzmo](../../OzmoOzmo)'s projects for the HKC SecureWatch and Aritech alarms.

Full Disclosure: I intended to just make use of OzmoOzmo's CastleHKCArduinoRKP project for the HKC SecureWatch alarm, but I bought the HKC SecureWave panel by mistake, which uses an entirely different protocol. This project is the result, where I reverse engineered the protocol used by the HKC SecureWave panel.

## Features
* ESP32 can join your local WiFi network to allow remote control of your alarm panel
* Add as only Keypad for panel
* Add as secondary Keypad for panel
* Websocket to allow remote access
* Simple HTML/Javascript implementation of Keypad UI
  * This can be deployed independently, and connect to a remote ESP32, this will reduce the memory used a bit.
  * Display text from panel, including flashing characters like real keypad
  * Red/Amber/Green LEDs, with solid / flashing / off states
* Separate project implements Android Keypad UI with push notifications
* If wired up, listen for events on programmable outputs (e.g. pre-alarm or alarm), and send push notification (requires suitable server API).
