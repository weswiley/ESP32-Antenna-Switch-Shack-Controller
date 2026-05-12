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
