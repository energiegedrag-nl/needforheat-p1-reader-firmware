# Twomes P1 Reader  <!-- omit in toc -->

This repository contains firmware and binary releases that can be installed on the [twomes-p1-gateway-hardware](https://github.com/energiegedrag-nl/twomes-p1-gateway-hardware). Connected to the [P1 port of smart meters adhering to the DSMR standards](https://github.com/energiegedrag-nl/dsmr-info), this device can read smart meter data. It can also register occupancy (i.e. the number of smartphones of participating subjects) via [Bluetooth name requests](https://github.com/energiegedrag-nl/needforheat-generic-firmware/tree/main/src/presence_detection). All data that is registered will be sent regularly to Twomes server via the [Twomes v2 API](https://github.com/energiegedrag-nl/needforheat-server-api).

> **Own deployment.** This repository is maintained by [energiegedrag-nl](https://github.com/energiegedrag-nl) and deployed under `energiegedrag.nl`. To run your own instance, replace `energiegedrag-nl` by your GitHub organisation and `energiegedrag.nl` by your domain (hosts used: `api.<domain>`, `manuals.<domain>`), consistently in configuration and build settings.

## Table of contents  <!-- omit in toc -->
- [General info](#general-info)
- [Measurements](#measurements)
- [Deploying](#deploying)
	- [Deploying on twomes-p1-gateway-hardware](#deploying-on-twomes-p1-gateway-hardware)
	- [Deploying on an M5Stack CoreInk](#deploying-on-an-m5stack-coreink)
- [Developing](#developing)
- [Supported devices](#supported-devices)
- [Features](#features)
- [Status](#status)
- [License](#license)
- [Citation](#citation)
- [Credits](#credits)

## General info

This measurement device firmware is built on top of the [Generic Firmware for Twomes measurement devices](https://github.com/energiegedrag-nl/needforheat-generic-firmware) with presence detection enabled. It contains an improved and updated way to read data from a P1 port, compared to the earlier [twomes-p1-gateway-firmware](https://github.com/energiegedrag-nl/twomes-p1-gateway-firmware). 


## Measurements

The Twomes P1 Reader, in addition to [generic data sent by any Twomes measurement device](https://github.com/energiegedrag-nl/needforheat-generic-firmware#readme), sends data about the following properties via the [Twomes API](https://github.com/energiegedrag-nl/needforheat-server-api) to a Twomes server:

| Sensor | Property           | Unit | [Printf format](https://en.wikipedia.org/wiki/Printf_format_string) | Default measurement interval \[h:mm:ss\] | Description                            |
|--------|--------------------|------|--------|-------------------|----------------------------------------|
| P1 port   | `e_use_lo_cum__kWh`         | kWh   | %.3f   | 0:10:00           | electricity meter reading                        |
| P1 port   | `e_use_hi_cum__kWh`         | kWh   | %.3f   | 0:10:00           | electricity meter reading                        |
| P1 port   | `e_ret_lo_cum__kWh`         | kWh   | %.3f   | 0:10:00           | electricity meter reading                        |
| P1 port   | `e_ret_hi_cum__kWh`         | kWh   | %.3f   | 0:10:00           | electricity meter reading                        |
| P1 port   | `g_use_cum__m3`             |  m<sup>3</sup> | %.3f  | 0:10:00   | gas meter reading                                |
| P1 port   | `dsmr_version__0`           | [-]     | %.1f   | 0:10:00           | DSMR version                                     |
| P1 port   | `meter_code__str`           | [-]     | %s     | 0:10:00           | [smart meter code](https://github.com/energiegedrag-nl/dsmr-info/blob/main/dsmr-e-meters.csv) (the type, not the unique identifier)      |
| Bluetooth  | `occupancy__p`         | [-]   | %u   | 0:10:00           | number of smartphones responding to Bluetooth name request (only if enabled compile time)                       |

## Deploying 

### Deploying on [twomes-p1-gateway-hardware](https://github.com/energiegedrag-nl/twomes-p1-gateway-hardware) 

This section provides instructions on deploying firmware releases on the [twomes-p1-gateway-hardware](https://github.com/energiegedrag-nl/twomes-p1-gateway-hardware). This deployment method allows you to update the firmware without modifying the source code, without requiring a development environment, and without the need to compile the source code.

#### Prerequisites  <!-- omit in toc -->

To deploy the firmware, in addition to the [generic prerequisites for deploying Twomes firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware#prerequisites), you need:
* a 3.3V TTL-USB Serial Port Adapter (e.g. [FT232RL](https://www.tinytronics.nl/shop/en/communication-and-signals/usb/ft232rl-3.3v-5v-ttl-usb-serial-port-adapter), CP210x, etc..), including the cable to connect ths adapter to a free USB port on your computer (a USB to miniUSB cable in the case of a [FT232RL](https://www.tinytronics.nl/shop/en/communication-and-signals/usb/ft232rl-3.3v-5v-ttl-usb-serial-port-adapter));
* (optional: more stable) Supply 5V DC power to the device via the micro-USB jack of the device.
* Find a row of 6 holes holes (next to the ESP32 on the PCB of the  P1 Gateway), find the `GND` pin (see  bottom of the PCB), align the 6 pins of the serial port adapter such that `GND` and other pins match; then connect the serial port adapter to your computer and connect the 6 pins of the serial port adapter to the 6 holes on the PCB.

#### Device preparation step 1: Uploading firmware  <!-- omit in toc -->

* Download the [binary release for your device](https://github.com/energiegedrag-nl/twomes-p1-gateway-firmware/releases) and extract it to a directory of your choice.
* If you used the device before, you shoud first [erase all persistenly stored data](https://github.com/energiegedrag-nl/needforheat-generic-firmware#erasing-all-persistenly-stored-data).
* Follow the [generic Twomes firmware upload instructions ](https://github.com/energiegedrag-nl/needforheat-generic-firmware#device-preparation-step-1a-uploading-firmware-to-esp32), with the exceptions mentioned below:
	* When you see the beginning of the sequence `Connecting ......_____......`, press and hold the button labeled `GPIO0 (SW2)` on the PCB, then briefly press the button labeled `RESET`, then release the button labeled `GPIO0 (SW2) `;
	* You should see an indication that the firmware is being written to the device.
	* When the upload is finished, view the serial output with a serial monitor tool like [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/) or the utility of your IDE (115200 baud). Press `RESET (SW1)` shortly to  make sure the firmware boots. 

#### Device Preparation step 2 and further   <!-- omit in toc -->

Please follow the [generic firmware instructions for these steps](https://github.com/energiegedrag-nl/needforheat-generic-firmware#device-preparation-step-2-establishing-a-device-name-and-device-activation_token). 

### Deploying on an [M5Stack CoreInk](https://github.com/m5stack/M5-CoreInk)

To deploy this software on [M5Stack CoreInk](https://github.com/m5stack/M5-CoreInk), see the [deploying section in the twomes-generic-esp-firmware library documentation](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/docs/deploying/prerequisites.md). The firmware needed can be found as a [release from this repository](https://github.com/energiegedrag-nl/needforheat-p1-reader-firmware). 

## Developing

To develop software, or based on this software, see the [developing section in the twomes-generic-esp-firmware library documentation](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/docs/starting/prerequisites.md). Remember to press buttons to upload the firmware on the [twomes-p1-gateway](https://github.com/energiegedrag-nl/twomes-p1-gateway-hardware): 
* When you see the beginning of the sequence `Connecting ....___....`, press and hold the button labeled `GPIO0 (SW2)` on the PCB, then briefly press the button labeled `RESET (SW1)`, then release the button labeled `GPIO0 (SW2)`;
* You should see an indication that the firmware is being written to the device.


## Supported devices

This example was tested on:
- [twomes-p1-gateway-hardware](https://github.com/energiegedrag-nl/twomes-p1-gateway-hardware)

Prepared, but not yet tested on hardware:
- [M5Stack CoreInk](https://docs.m5stack.com/en/core/coreink) with the [NeedForHeat P1-BASE v3](https://github.com/energiegedrag-nl/needforheat-p1-base-hardware): PlatformIO environment `M5STACK_COREINK_P1BASE_V3`. P1 data on G13 (not inverted), data request on G26 (active high), P1 charging of the CoreInk battery on G25/G14, the meter's 5 V on G36 and the existing reader's request on G34; see `src/p1base_v3.c` and the [hardware design](https://github.com/energiegedrag-nl/needforheat-p1-base-hardware/blob/main/docs/design-v3.md#5-firmware-interface). Details below.

### P1-BASE v3

* **Charging.** Only on DSMR 5 meters: 60 mA first, 120 mA when the meter's 5 V stays at or above 4.75 V, one step back below 4.6 V, off below 4.5 V. Off above 4.1 V cell voltage (with the CoreInk drawing from the cell the TP4056 may never terminate) and on again below 4.0 V. At most 60 mA when a BatteryBASE is detected (ADS1115 at I²C 0x48), so that no more than one BASE charges at full current.
* **Charging only happens while the CoreInk is on.** Between measurements the CoreInk switches itself off and the RTC wakes it; the ESP32 is then unpowered and the pull-down on the BASE switches G25 off (`gpio_hold` cannot help here). So after a read on DSMR 5, while the cell is below 4.0 V, the P1 task keeps the CoreInk on for a *charge window* until 5 s before the next read, the cell reaches 4.1 V, or the meter supply sags. The CoreInk itself draws far less than the charge current while it waits, so the cell gains charge.
* **Boot.** G14 and G25 are set low first thing in `app_main`; G14 has an internal pull-up at reset (harmless while G25 is low).
* **LED dimmer.** The PCA9633 (U5, I²C 0x62) is set to about 12 % brightness (MODE1 0x00, MODE2 0x14, PWM0 0xFF, GRPPWM 32, LEDOUT 0x03) after boot and after every read, because it restarts at full brightness when it loses its supply. A BASE without U5 does not answer at 0x62 and keeps its LEDs at full brightness. The firmware never writes to 0x03 (software reset) or 0x70 (all-call).
* **Diagnosis** (logged after every read, and shown on the e-ink screen with a generic library that has the e-ink status screen, build flag `NFH_EINK_STATUS`):

  | Meter 5 V (G36) | Telegrams | Status text | Meaning |
  |---|---|---|---|
  | 4.6–5.5 V | yes | `ok` | DSMR 4 or 5 |
  | 4.6–5.5 V | no | `geen data` | the meter does not send: request not reaching it, cable in the wrong socket, or a meter that releases its data line |
  | < 0.5 V | yes | `DSMR 2/3: USB nodig` | DSMR 2/3 meter without P1 supply: power the CoreInk from USB |
  | < 0.5 V | no | `geen meter` | no meter, crossed cable, or the cable is in the wrong socket |
  | 0.5–4.5 V | – | `meter 5 V te laag` | bad contact or a very weak supply |
  | 4.5–4.6 V | – | `P1 overbelast` | P1 supply overloaded: charging stepped down |

  With no cable in the meter socket and the CoreInk on, the data line is pulled high on the BASE: its LED1 lights steadily and the UART sees a break. On a DSMR 2/3 meter with the CoreInk off, an existing reader's request reaches the meter only passively (about 3.5 V with a weak reader); the active request path needs the CoreInk on.
* **Screens.** Screen 8 (P1 overview): main value `stroom` in kW (delivered minus returned, OBIS 1-0:1.7.0 and 1-0:2.7.0), tiles `DSMR`, `lezer` (existing reader requesting) and `laden` (charge current). Screen 9 (P1 cable diagnosis): the status text as label, the meter's 5 V as main value, tiles `lezer` and `laden`. Supply indicator `P1` / `accu` on the right of the footer.
* **HAT conflict.** A HAT on the CoreInk's top header shares G25, G26 and G36 with the P1-BASE: do not use both.
* **Not detected yet:** USB power. With USB connected while P1 charges, the cell may get more than 0.5 C; the 4.1 V limit stops P1 charging near full. This belongs in the generic power manager, as does the rest of the charging logic.

## Features

List of features ready and TODOs for future development (other than the [features of the generic Twomes firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware#features)). 

Ready:

* Automatically identifies the appropriate serial port configurations aand OBIS codes for [all DSMR versions (2-5)](https://github.com/energiegedrag-nl/dsmr-info/blob/main/dsmr-p1-specs.csv).
* Retrieves information from all current smart meters from [all DSMR versions (2-5)](https://github.com/energiegedrag-nl/dsmr-info/blob/main/dsmr-p1-specs.csv).
* Utilizes timestamps provided by the P1 port whenever available for measurement timestamps, assumiong `Europe/Amsterdam` timezone.
  * For [DSMR3 and older](https://github.com/energiegedrag-nl/dsmr-info), `YYMMDDhhmmss` timestamps are processed properly, even during the transition from summertime to wintertime.
  *  For [DSMR4 and newer](https://github.com/energiegedrag-nl/dsmr-info), `YYMMDDhhmmssX` timestamps are processed properly, X=`S` means summertime. and X=`W` means wintertime.
* Discards inaccurate measurements (e.g., sporadic errors exhibited by [Sagemcom XS210 ESMR5 - E0047](https://github.com/energiegedrag-nl/dsmr-info))
* Support Wi-Fi provisioning reset by holding down the K button (labeled as GPIO12 (SW3) on the PCB) for more than 10 seconds.

To-do:

* Provide visual feedback on status and errors through LEDs.
* Test the P1-BASE v3 support on hardware (charge window, LED dimmer, diagnosis) and switch on the e-ink screens 8 and 9 once the generic library with the e-ink status screen is released.
* Move the P1-BASE charging logic to the generic power manager (with USB detection and the BatteryBASE).


## Status

Project is: _in progress_

## License

This software is available under the [Apache 2.0 license](./LICENSE), Copyright 2023 [Research group Energy Transition, Windesheim University of Applied Sciences](https://windesheim.nl/energietransitie) 

## Citation

If you use this repository in your research or work, please cite the following pre-print, which describes the overall NeedForHeat DataGear system of which this repository is a part:

> Ter Hofte, H., & van Ravenzwaaij, N. (2025). *NeedForHeat DataGear: An Open Monitoring System to Accelerate the Residential Heating Transition*. arXiv preprint arXiv:2509.06927. https://doi.org/10.48550/arXiv.2509.06927

**Note:** This is a pre-print submitted on 8 Sep 2025 and has not yet been peer-reviewed. For the full paper, see [https://arxiv.org/abs/2509.06927](https://arxiv.org/abs/2509.06927).

## Credits

Author:
* Henri ter Hofte · [@henriterhofte](https://github.com/henriterhofte)

This repository continues [energietransitie/needforheat-p1-reader-firmware](https://github.com/energietransitie/needforheat-p1-reader-firmware), made by the Research group Energy Transition at Windesheim University of Applied Sciences.
Thanks to the authors and contributors of the original:
* Joël van de Weg · [@JoelvdWeg](https://github.com/JoelvdWeg)
* Sjors Smit ·  [@Shorts1999](https://github.com/Shorts1999)
* Fredrik-Otto Lautenbag ·  [@Fredrik1997](https://github.com/Fredrik1997)
* Gerwin Buma ·  [@GerwinBuma](https://github.com/GerwinBuma)
* Werner Heetebrij ·  [@Werner-Heetebrij](https://github.com/Werner-Heetebrij)
* Nick van Ravenzwaaij · [@n-vr](https://github.com/n-vr)

We use and gratefully acknowlegde the efforts of the makers of the following source code and libraries:

* [ESP-IDF](https://github.com/espressif/esp-idf), by Espressif Systems, licensed under [Apache License 2.0](https://github.com/espressif/esp-idf/blob/9d34a1cd42f6f63b3c699c3fe8ec7216dd56f36a/LICENSE)
* [dsmr-info](https://github.com/energiegedrag-nl/dsmr-info), by [Research group Energy Transition, Windesheim University of Applied Sciences](https://windesheim.nl/energietransitie), licensed under [CC-BY-4.0 license](https://github.com/energiegedrag-nl/dsmr-info/blob/main/LICENSE.md)
* [twomes-generic-esp-firmware](https://github.com/energiegedrag-nl/needforheat-generic-firmware), by [Research group Energy Transition, Windesheim University of Applied Sciences](https://windesheim.nl/energietransitie), licensed under [Apache License 2.0](https://github.com/energiegedrag-nl/needforheat-generic-firmware/blob/main/LICENSE.md)

### AI assistance

Since November 2026, parts of the maintenance of this repository have been done with [Claude Code](https://claude.com/claude-code) (Anthropic), under the author's direction and review. Commits made this way carry a `Co-Authored-By: Claude …` trailer. The author remains responsible for all content.
