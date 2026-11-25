# eCOOK 3.0 Firmware

Firmware for the ATEC eCOOK 3.0 smart electric cooking stove. It runs on the Holtek HT32F54253 (ARM Cortex-M0+) and covers stove UI, cooking-session tracking, cellular MQTT reporting, and over-the-air updates.

Repository: [atecglobal/eCOOK3_firmware_OTA](https://github.com/atecglobal/eCOOK3_firmware_OTA)

## Overview

The firmware is a dual-image design:

- **IAP bootloader** (12 KB) — Intel HEX programming, OTA flash write, jump to application
- **AP application** — FSM, touch UI, I2C power-board control, GSM/MQTT, cooking-session storage

The application talks to AWS IoT Core over MQTT using a SIMCom A7682E Cat-1 LTE module. It supports single-plate and dual-plate stoves, capacitive touch keys, a 7-segment LED display, persistent cooking records in internal flash (emulated EEPROM), and remote management through an OTA command framework.

## Key features

- Capacitive touch UI, 7-segment display, red/RGB LEDs, and buzzer feedback
- Independent left/right cooking sessions with persistent flash storage
- MQTT payloads: meta, cooking (full or essential), errors, connection retry
- OTA commands: `fw`, `fota`, `pu`, `lb`, `so`, `mr`, `rd`, `rp`, `pg`
- Factory CLI and GSM bridge mode for provisioning
- SIM APN selection (Onomondo / Aeris) from ICCID prefix

## Hardware

| Component | Part / spec |
| --- | --- |
| Main MCU | Holtek HT32F54253, ARM Cortex-M0+, 128 KB flash, 16 KB SRAM |
| GSM module | SIMCom A7682E Cat-1 LTE |
| User input | 24-key capacitive touch panel (Holtek Touch Key Library) |
| Display | LEDC 4-digit 7-segment, red indicators, 2x RGB LEDs |
| Power boards | I2C slaves (left I2C0 / right I2C1, address 0x50) |
| Battery | LiFePO4, ADC on PA7 |
| RTC | Internal LSE 32.768 kHz |

## Repository layout

```
eCOOK3_firmware_OTA/
├── project/AP/          Application firmware (FSM, payload, OTA, UI)
├── project/IAP/         In-application programming bootloader
├── library/             Holtek CMSIS, drivers, Touch Key, USB
├── utilities/           Board support and middleware
├── MDK_ARMv5/           Keil µVision 5 project (HT32F54253)
├── RELEASE/             Shipped .hex images (latest + Archives/)
└── docs/                Firmware specification, changelog, diagrams
```

## Documentation

| Document | Description |
| --- | --- |
| [docs/eCOOK3_Firmware_Specification.md](docs/eCOOK3_Firmware_Specification.md) | Firmware specification (architecture, FSM, OTA, payloads) |
| [docs/CHANGELOG.md](docs/CHANGELOG.md) | Release overview, hex files, fixes, and GitHub Release notes |
| [RELEASE/](RELEASE/) | Combined (IAP+AP) and AP-only hex images |

## Development environment

- **IDE:** Keil µVision 5
- **Compiler:** ARM MDK Compiler Version 5 [V 5.06 update 7 (build 960)]
- **Target MCU:** HT32F54253
- **Factory / recovery flash:** e-Link32 Pro ICP Tool with the combined hex in `RELEASE/`

## Building and flashing

1. Open the Keil workspace under `MDK_ARMv5/` and build the IAP + AP images.
2. For first-time flash, factory programming, or recovery, program the combined hex with **e-Link32 Pro ICP Tool**. Current release: [RELEASE/eC3_113_11082026.hex](RELEASE/eC3_113_11082026.hex).
3. For OTA/FOTA on devices that already have IAP, use the AP-only image: [RELEASE/eC3_113_AP_11082026.hex](RELEASE/eC3_113_AP_11082026.hex).

Release notes, known issues, and test links are in [docs/CHANGELOG.md](docs/CHANGELOG.md).
