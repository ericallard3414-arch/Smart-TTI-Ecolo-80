<div align="center">

# Smart-TTI-Ecolo-80



### Reverse-engineered ESPHome Integration for TTI ECOLO Pool Heat Pumps

![ESPHome](https://img.shields.io/badge/ESPHome-Compatible-blue)
![Home Assistant](https://img.shields.io/badge/Home%20Assistant-Compatible-41BDF5)
![ESP32](https://img.shields.io/badge/ESP32-Supported-red)
![License](https://img.shields.io/badge/License-MIT-green)
![Version](https://img.shields.io/badge/Version-v1.0.0-success)

Reliable two-way communication between **TTI ECOLO** pool heat pumps and **Home Assistant** using an **ESP32** running **ESPHome**.

Developed and reverse engineered by **Eric Allard**

</div>

---

# Overview

This project provides a complete ESPHome external component that communicates directly with the proprietary serial bus used by the **TTI ECOLO** series pool heat pumps.

Unlike relay-based solutions, this integration communicates with the original controller exactly like the factory keypad, allowing Home Assistant to control the heater while keeping the factory control panel fully functional.

The protocol was completely reverse engineered from scratch using a logic analyzer and custom ESPHome decoding software.

---
## ⚠️ Compatibility

This project is currently compatible **ONLY** with TTI ECOLO heat pumps equipped with the following controller board:

**Manufacturer**

GUANGDONG CHICO

**Main Board**

CC207S-V2.1

<img width="847" height="652" alt="image" src="https://github.com/user-attachments/assets/1c89b98d-0086-4ebb-b3b3-2b20d83cb3ea" />


Other controller revisions may use a different communication protocol and are **not supported** by this release.

If you own another controller revision and would like to help add support, protocol captures are welcome.

## Features

- ✅ Native ON/OFF power switch
- ✅ Current water temperature
- ✅ Target water temperature
- ✅ Heating Status binary sensor
- ✅ Temperature control (70°F–99°F)
- ✅ ECOLO Model selector
- ✅ Estimated current consumption (A)
- ✅ Estimated power consumption (W)
- ✅ Daily energy consumption (kWh)
- ✅ Total energy consumption (kWh)
- ✅ Persistent settings stored in ESP32 flash
- ✅ Automatic checksum verification
- ✅ Automatic command acknowledgement
- ✅ Immediate Home Assistant updates
- ✅ Automatic 5-second telemetry refresh
- ✅ Full compatibility with the factory keypad
- ✅ OTA firmware updates
- ✅ No cloud required

---

# Home Assistant

<img width="1443" height="837" alt="image" src="https://github.com/user-attachments/assets/17a68365-fa28-4505-b33b-f47ac8d7a59a" />

The integration creates the following entities:

| Entity | Description |
|----------|------------|
| Power | Turns the heater ON / OFF |
| Water Temperature | Current pool water temperature |
| Target Temperature | Desired water temperature |
| Heating | Indicates when the heater is actively heating |
| Temperature Up | Raises target temperature |
| Temperature Down | Lowers target temperature |

---

# Hardware

Required components

- ESP32 DevKitC
- 2N3904 transistor
- DROK Buck Converter (5V)
- 4.7k resistor
- 10k resistor
- 22k resistor
- 100k resistor
- Fuse
- Wiring

---

# Wiring
<img width="1536" height="1024" alt="ecolo-80-wiring" src="https://github.com/user-attachments/assets/4bab961d-844f-40cc-94db-a444fdcc5be4" />

## Power

```
Heater Brown (+16V)
        │
       Fuse
        │
    DROK IN+

Heater Yellow
        │
    DROK IN-

DROK OUT+
        │
 ESP32 5V

DROK OUT-
        │
 ESP32 GND
```

---

## Receive

```
Heater Blue
      │
     10k
      │──────────── GPIO4
      │
     22k
      │
     GND
```

---

## Transmit

```
GPIO18
   │
 4.7k
   │
 Base
   │
100k
   │
 GND

Emitter → GND

Collector → Heater Blue
```

The transistor acts as an **open-collector driver**, matching the behavior of the original keypad.

---

# Installation

Copy the component into:

```
/config/esphome/my_components/ecolo80/
```

Copy the supplied YAML configuration into your ESPHome folder.

Compile and upload to the ESP32.

Home Assistant will automatically discover the device.

---

# Protocol

The heater communicates using three different frame types.

## Packet A

Status information

- Power state
- Target temperature
- Status flags

## Packet B

Operating information

- Water temperature
- Heating state
- Runtime flags

## Keypad Frame

Transmitted by the ESP32 to emulate the factory keypad.

Every transmitted packet is verified before being acknowledged.

---

# Project Status

Current release

**Version 1.0.0**

## Current Capabilities

| Function | Status |
|----------|:------:|
| Read Water Temperature | ✅ |
| Read Target Temperature | ✅ |
| Read Heater Power | ✅ |
| Read Heating Status | ✅ |
| Heater ON/OFF | ✅ |
| Increase Temperature | ✅ |
| Decrease Temperature | ✅ |
| Model Selection | ✅ |
| Current Consumption | ✅ |
| Power Consumption | ✅ |
| Daily Energy | ✅ |
| Total Energy | ✅ |
| Home Assistant Integration | ✅ |
| ESPHome External Component | ✅ |

---

# Tested Hardware

Successfully tested on

- TTI ECOLO-80

Additional testing on ECOLO-50 and ECOLO-100 is welcome.

---

# Contributing

Contributions are welcome.

Useful contributions include:

- Testing on additional ECOLO models
- Error code captures
- Defrost captures
- Documentation improvements
- Hardware improvements

---

# License

MIT License

---

# Acknowledgements

Special thanks to the Home Assistant and ESPHome communities for providing the excellent open-source ecosystem that made this project possible.

---

# Author

## Eric Allard

Reverse engineering

Protocol decoding

ESPHome component development

Hardware interface design

Home Assistant integration

---

If this project helps you, please consider giving it a ⭐ on GitHub.
