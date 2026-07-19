<div align="center">

# Smart-TTI-Ecolo-80

### ESPHome and Home Assistant integration for TTI ECOLO pool heat pumps

![Version](https://img.shields.io/badge/version-v2.0.0-success)
![Status](https://img.shields.io/badge/status-production%20release-brightgreen)
![ESPHome](https://img.shields.io/badge/ESPHome-external%20component-000000)
![Home Assistant](https://img.shields.io/badge/Home%20Assistant-compatible-41BDF5)
![ESP32](https://img.shields.io/badge/ESP32-supported-E7352C)
![License](https://img.shields.io/badge/license-MIT-green)

Reliable two-way communication with the original TTI ECOLO controller bus while keeping the factory keypad operational.

**Reverse engineered and developed by Eric Allard**

</div>

---

![Smart TTI ECOLO Home Assistant dashboard](docs/images/home-assistant-dashboard.png)

## Compatibility

> **This project is compatible only with TTI ECOLO heat pumps equipped with the following main controller board:**
>
> **Manufacturer:** GUANGDONG CHICO  
> **Main board model:** CC207S-V2.1
>
> Other controller revisions may use a different communication protocol and are not supported by this release.

## Overview

Smart-TTI-Ecolo-80 is a custom ESPHome external component that connects an ESP32 directly to the proprietary controller bus used by compatible TTI ECOLO pool heat pumps.

Unlike a relay-only modification, it reads the heater's own status frames and transmits valid keypad-style commands. Home Assistant receives native entities for power, temperatures, heating state, model selection, current consumption, power consumption, and estimated energy use.

## Features

- Native power ON/OFF switch
- Current water temperature
- Target temperature
- Temperature Up and Down controls
- Heating ON/OFF binary sensor
- ECOLO model dropdown in Home Assistant
- Estimated current consumption
- Estimated running power
- Daily and cumulative estimated energy
- Immediate updates when heating or model changes
- Automatic consumption refresh every five seconds
- Checksum validation and transmit-echo verification
- Heater acknowledgement for control commands
- Factory keypad remains functional
- Model selection saved in ESP32 flash
- OTA firmware updates
- No cloud dependency

## Supported models

Choose the installed unit from the **Ecolo Model** dropdown:

| Model | Rated operating current | Estimated running power at 240 V |
|---|---:|---:|
| ECOLO 50 | 10 A | 2.40 kW |
| ECOLO 65 | 12 A | 2.88 kW |
| ECOLO 80 | 18 A | 4.32 kW |
| ECOLO 100 | 21 A | 5.04 kW |
| ECOLO 120 | 24 A | 5.76 kW |

Estimated power is calculated as:

```text
240 V × rated operating current
```

Actual consumption may vary with line voltage, startup, defrost, ambient conditions, and equipment condition.

## Home Assistant entities

| Entity | Purpose |
|---|---|
| Ecolo Power | Heater ON/OFF |
| Ecolo Temperature Up | Raises target temperature |
| Ecolo Temperature Down | Lowers target temperature |
| Ecolo Target Temperature | Current target |
| Ecolo Water Temperature | Current water temperature |
| Ecolo Heating | ON while heating, OFF while idle |
| Ecolo Model | Selects ECOLO 50/65/80/100/120 |
| Ecolo Current Consumption | Estimated running current |
| Ecolo Power Consumption | Estimated running watts |
| Ecolo Energy Today | Estimated kWh since midnight |
| Ecolo Energy Total | Restored cumulative estimated kWh |

Current and power update immediately when the heating state changes, immediately when the selected model changes, and every five seconds while the ESP32 is online.

## Current capabilities

| Function | Status |
|---|:---:|
| Read water temperature | ✅ |
| Read target temperature | ✅ |
| Read heater power state | ✅ |
| Read heating state | ✅ |
| Turn heater ON | ✅ |
| Turn heater OFF | ✅ |
| Increase temperature | ✅ |
| Decrease temperature | ✅ |
| Model selection | ✅ |
| Estimated current consumption | ✅ |
| Estimated power consumption | ✅ |
| Estimated daily energy | ✅ |
| Estimated total energy | ✅ |
| Home Assistant integration | ✅ |
| ESPHome external component | ✅ |

## Hardware

- ESP32 DevKitC or compatible ESP32 board
- 2N3904 NPN transistor
- DROK adjustable buck converter
- 4.7 kΩ resistor
- 10 kΩ resistor
- 22 kΩ resistor
- 100 kΩ resistor
- Inline fuse
- Suitable wiring and insulation

## Wiring

![ECOLO ESP32 wiring diagram](docs/images/wiring-diagram-16x9.png)

### Power

```text
Heater Brown (+16–17 V DC) -> Fuse -> DROK IN+
Heater Yellow (GND) --------------> DROK IN-

DROK OUT+ adjusted to 5.0 V ------> ESP32 5V
DROK OUT- ------------------------> ESP32 GND
```

### Receive

```text
Heater Blue ---- 10 kΩ ----+---- ESP32 GPIO4
                           |
                          22 kΩ
                           |
                          GND
```

### Transmit

```text
ESP32 GPIO18 ---- 4.7 kΩ ----+---- 2N3904 Base
                              |
                            100 kΩ
                              |
                             GND

2N3904 Emitter -------------------- GND
2N3904 Collector ------------------ Heater Blue
```

Verify the pinout of the exact 2N3904 package being used.

## Installation

1. Copy `esphome/my_components/ecolo80/` to:

```text
/config/esphome/my_components/ecolo80/
```

2. Copy `esphome/ecolo-80.yaml` to the ESPHome configuration directory.
3. Review Wi-Fi, API, OTA, device naming, GPIO4 RX, and GPIO18 TX.
4. Validate, compile, and install.
5. Add the discovered ESPHome device to Home Assistant.
6. Select the correct heater using **Ecolo Model**.

The model selection is stored in ESP32 flash and restored after reboot.

## Protocol highlights

Observed frames contain 14 bytes.

- Packet A byte 2: encoded target temperature
- Packet A byte 7:
  - `0x7C`: OFF
  - `0x7E`: ON
- Packet B byte 10, bit `0x08`: heating
  - `0x10`: idle
  - `0x18`: heating startup
  - `0x58`: heating/running
- Final byte: checksum

See [docs/protocol.md](docs/protocol.md).

## Safety

This project connects to a proprietary heater control bus.

- Disconnect power before changing wiring.
- Fuse the supply feeding the buck converter.
- Verify 5.0 V output before connecting the ESP32.
- Never power the ESP32 directly from the heater supply.
- Verify transistor pinout and all common-ground connections.
- High-voltage heater wiring should be handled by a qualified person.
- Use this project at your own risk.

## Project status

**Smart-TTI-Ecolo-80 v2.0.0 is the first stable production release.**

It provides the complete supported feature set for compatible TTI ECOLO heat pumps using the **GUANGDONG CHICO CC207S-V2.1** main board.

## Author

**Eric Allard**

- Protocol reverse engineering
- Hardware interface development
- ESPHome component
- Home Assistant integration
- ECOLO-80 testing

## Acknowledgements

Special thanks to the ESPHome and Home Assistant communities for providing the open-source ecosystem that made this project possible.

## License

MIT License. See [LICENSE](LICENSE).

If this project helps you, consider starring the repository.
