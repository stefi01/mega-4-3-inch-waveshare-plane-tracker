# Mega 4.3

Firmware for the Mega 4.3 tracker. Version 1.0.

The screen is a **Freenove FNK0115B**, 4.3 inch, 480 by 272, with an XPT2046 touch. This sketch is for that board. It is not the Waveshare 7 inch Mega, and it is not Home Field Scope.

Planes, ships, the map, and the weather come in over 2.4 GHz Wi-Fi. It is not FAA equipment and not for navigation.

## If you bought one

The tracker must already be on Wi-Fi.

1. On the screen: **SETUP → ABOUT**
2. Tap **CHECK**
3. If a newer version is listed, tap **INSTALL**
4. Leave the USB power connected until it restarts

The screen reads these two files from this repository:

- https://raw.githubusercontent.com/stefi01/mega-4-3-inch-waveshare-plane-tracker/main/ota/version_43.txt
- https://raw.githubusercontent.com/stefi01/mega-4-3-inch-waveshare-plane-tracker/main/ota/mega43.bin

This repository stays public so CHECK and INSTALL can reach those files.

The buyer manual is `Mega 4.3 User Manual.pdf`.

## Screen

Freenove FNK0115B.

- 4.3 inch, 480×272, rotation 2
- Touch XPT2046. SCK 12, MISO 13, MOSI 11, CS 38, IRQ 18
- Backlight is GPIO 2. LOW, MED, and HIGH change the brightness
- Cable in the Type-C port that shows up as CH340. This board has no UART switch

Do not load the 7 inch file `ws7.bin` onto this screen.

## Arduino Tools menu

Board package: esp32 by Espressif Systems (this build used 3.3.12). Arduino IDE 1.8.18.

Board: **ESP32S3 Dev Module**

| Tools item | Set it to |
| --- | --- |
| Board | ESP32S3 Dev Module |
| PSRAM | OPI PSRAM |
| Flash Size | 16MB (128Mb) |
| USB CDC On Boot | Enabled |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| Upload Speed | 921600 |
| USB Mode | Hardware CDC and JTAG |
| USB Firmware MSC On Boot | Disabled |
| USB DFU On Boot | Disabled |
| Upload Mode | UART0 / Hardware CDC |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO 80MHz |
| Core Debug Level | None |
| Erase All Flash Before Sketch Upload | Disabled |
| JTAG Adapter | Disabled |
| Arduino Runs On | Core 1 |
| Events Run On | Core 1 |
| Zigbee Mode | Disabled |

USB CDC On Boot stays Enabled. The Waveshare 7 inch uses Disabled. Do not swap those.

Port is the CH340 COM port. Do not hold BOOT. Click Upload.

## Bin uploader

The shop tool is **Flash Download Tool 3.9.3** (`flash_download_tool_3.9.3.exe`). These boxes are not the Arduino Tools menu.

First window: WorkMode **Develop**, LoadMode **UART**, ChipType **ESP32-S3**.

SPIDownload window:

| Box | Set it to |
| --- | --- |
| SPI SPEED | 80MHz |
| SPI MODE | DIO |
| DoNotChgBin | ticked |
| LockSettings | unticked |
| BAUD | 115200 |
| File row | one row, address `0x10000` |

Choose `sketch/mega_43/mega_43.ino.esp32s3.bin`. That file is the same bytes as `ota/mega43.bin`. Do not choose a bootloader, a partitions file, or a merged bin. Do not press ERASE.

Cable in the Type-C port. Pick the CH340 COM port. Click START. Wait until the box says FINISH.

## What is in this repo

| Path | What it is |
| --- | --- |
| `sketch/mega_43/mega_43.ino` | The sketch to compile |
| `sketch/mega_43/iss_world.h` | ISS earth picture, same folder as the sketch |
| `sketch/mega_43/mega_43.ino.esp32s3.bin` | The 1.0 app binary from Arduino |
| `ota/version_43.txt` | One line, `1.0`, the version CHECK compares |
| `ota/mega43.bin` | The same 1.0 binary, the file INSTALL writes |
| `Mega 4.3 User Manual.pdf` | Buyer manual |
| `LIBRARIES.txt` | Library Manager names |
| `EXAMPLES.txt` | Board and library demos |
| `library/XPT2046_Touchscreen/` | Spare copy of the touch library. Do not move it into the sketch folder |
| `settings to upload.txt` | Arduino Tools and the flash download tool, as plain text |
| `CATCH-UP.txt` | Where version 1.0 left off |

## Libraries

Install the four in `LIBRARIES.txt`: GFX Library for Arduino (Moon), ArduinoJson (Benoit Blanchon), TJpg_Decoder (Bodmer), and XPT2046_Touchscreen (Paul Stoffregen). Wi-Fi, HTTP, SPI, SD, and Preferences come with the esp32 package.

## Publish a later version

1. Change `HFS_VER` in the sketch and put that same text in `ota/version_43.txt`.
2. Arduino: Sketch → Export compiled Binary.
3. Replace `ota/mega43.bin` and `sketch/mega_43/mega_43.ino.esp32s3.bin` with that binary.
4. Push `main`.
