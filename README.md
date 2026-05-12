# ESP32-Antenna-Switch-Shack-Controller
A control interface for https://github.com/TechMindsYT/ESP32-Antenna-Switch

A touchscreen remote control for the **TechMinds ESP32 Antenna Switch**, built for the **ESP32-2432S028 "Cheap Yellow Display" (CYD)**.

This project turns the CYD into a dedicated Wi-Fi touchscreen controller that mirrors the live state of a remote ESP32 antenna switch.

## Features

- 4-button touchscreen antenna switch control
- Live synchronization with the antenna switch via WebSocket
- Dynamic button labels pulled from the switch controller
- Real-time red/green status indication
- Automatic Wi-Fi reconnect
- Automatic WebSocket reconnect
- Visual touch feedback
- Native touchscreen UI (no browser required)

## Hardware

### Controller
- ESP32-2432S028 (Cheap Yellow Display / CYD)
- ST7789 TFT display (integrated)
- XPT2046 touch controller (integrated)

### Controlled Device
This project is designed to work with:

**TechMindsYT ESP32 Antenna Switch**
https://github.com/TechMindsYT/ESP32-Antenna-Switch

## How It Works

The CYD connects over Wi-Fi to the remote antenna switch ESP32.

Communication happens over WebSocket:

```text
ws://<antenna-switch-ip>/ws

The antenna switch sends JSON updates containing:

{
  "states": [true, false, false, false],
  "names": ["HF Vertical", "Dipole", "Satellite", "Dummy Load"]
}

Touching a button sends:

{
  "action": "toggle",
  "id": 2
}

This keeps the CYD perfectly synchronized with the web interface hosted on the antenna switch ESP.

Wiring / Pinout
Display (ST7789)
Signal	GPIO
MOSI	13
SCLK	14
CS	15
DC	2
BL	21
Touch (XPT2046)
Signal	GPIO
CS	33
IRQ	36
MOSI	32
MISO	39
CLK	25
Libraries Required

Install these from Arduino Library Manager:

TFT_eSPI
XPT2046_Touchscreen
ArduinoJson
ArduinoWebsockets
TFT_eSPI Configuration

This project requires a custom User_Setup.h for the CYD.

Use:

#define ST7789_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  -1

#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define SPI_FREQUENCY 20000000

And ensure:

User_Setup_Select.h

includes:

#include <User_Setup.h>

NOT:

#include <User_Setups/Setup49_ESP32_S3_ILI9341.h>
Touch Calibration

This project uses calibrated values for the tested CYD:

RAW_X_MIN 588
RAW_X_MAX 3512
RAW_Y_MIN 848
RAW_Y_MAX 3429

Your CYD may vary slightly.

Configuration

Edit these values in the sketch:

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SWITCH_HOST = "192.168.1.50";
Build Settings

Arduino IDE:

Board: ESP32 Dev Module
Upload Speed: 921600
CPU Frequency: 240MHz
Flash Frequency: 80MHz
Flash Mode: DIO
Flash Size: 4MB
Partition Scheme: Default 4MB with spiffs
PSRAM: Disabled
