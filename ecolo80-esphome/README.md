# ECOLO-80 ESPHome Integration

Reverse-engineered ESPHome external component for selected **TTI ECOLO pool heat pumps**, developed and tested on an ECOLO-80 controller.

> **Project status:** Working production build  
> **Release:** v1.0.0  
> **ESP32 RX pin:** GPIO4  
> **ESP32 TX pin:** GPIO18

## Features

- Native Home Assistant power switch
- Water temperature sensor
- Target temperature sensor
- Temperature Up and Temperature Down controls
- Heating ON/OFF binary sensor
- Checksum validation
- Verified transmit echo
- Command acknowledgement through the heater's own status packets
- Rapid temperature-click grouping and direct final-target transmission
- Factory keypad remains usable

## Important safety notice

This project connects to a proprietary heater control bus. Incorrect wiring can damage the heater controller, ESP32, power supply, or connected equipment.

- Disconnect power before changing wiring.
- Confirm transistor pinout for your exact 2N3904 package.
- Confirm the buck converter output before connecting the ESP32.
- Use a fuse on the heater supply input.
- Do not connect the ESP32 directly to the heater's higher-voltage supply.
- Use this project at your own risk.

## Hardware

- ESP32 DevKitC or compatible ESP32 board
- 2N3904 NPN transistor
- 4.7 kΩ resistor
- 10 kΩ resistor
- 22 kΩ resistor
- 100 kΩ resistor
- Adjustable DROK buck converter
- Inline fuse
- Wire, heat shrink, and suitable connectors

## Wiring overview

### Power

```text
Heater Brown (+16–17 V DC)
        |
       Fuse
        |
   DROK IN+

Heater Yellow (GND) -------- DROK IN-

DROK OUT+ (5.0 V) ---------- ESP32 5V
DROK OUT- ------------------ ESP32 GND
Heater Yellow -------------- Common GND
```

### Receive path

```text
Heater Blue ---- 10 kΩ ----+---- ESP32 GPIO4
                           |
                          22 kΩ
                           |
                          GND
```

### Transmit path

```text
ESP32 GPIO18 ---- 4.7 kΩ ----+---- 2N3904 Base
                              |
                            100 kΩ
                              |
                             GND

2N3904 Emitter -------------------- GND
2N3904 Collector ------------------ Heater Blue
```

See [docs/wiring.md](docs/wiring.md) before building.

## Installation

1. Copy the component folder into ESPHome:

```text
/config/esphome/my_components/ecolo80/
```

2. Copy:

```text
esphome/ecolo-80.yaml
```

to your ESPHome configuration directory.

3. Adjust Wi-Fi secrets and device naming as needed.

4. Validate, compile, and install through ESPHome.

5. Add the device to Home Assistant through the ESPHome integration.

## Home Assistant entities

The production configuration exposes:

- `switch.ecolo_power`
- `button.ecolo_temperature_up`
- `button.ecolo_temperature_down`
- `sensor.ecolo_target_temperature`
- `sensor.ecolo_water_temperature`
- `binary_sensor.ecolo_heating`

Entity IDs may differ depending on ESPHome naming.

## Protocol summary

The controller uses alternating 14-byte Packet A and Packet B frames plus 14-byte keypad frames.

Known fields:

- Packet A byte 2: encoded target temperature
- Packet A byte 7: power/status flags
  - `0x7C`: power OFF
  - `0x7E`: power ON
- Packet B byte 10, bit `0x08`: heating state
  - `0x10`: idle
  - `0x18`: heating startup
  - `0x58`: heating/running
- Final byte: checksum

More details are in [docs/protocol.md](docs/protocol.md).

## Current limitations

- Tested on one ECOLO-80 installation.
- Other TTI models or controller revisions may use different fields or timing.
- Defrost, fan, compressor, and fault-code states are not yet decoded.
- Fahrenheit is used internally by the current implementation.

## Repository layout

```text
ecolo80-esphome/
├── README.md
├── LICENSE
├── CHANGELOG.md
├── .gitignore
├── esphome/
│   ├── ecolo-80.yaml
│   └── my_components/
│       └── ecolo80/
│           ├── __init__.py
│           ├── ecolo80.h
│           └── ecolo80.cpp
├── docs/
│   ├── installation.md
│   ├── wiring.md
│   ├── protocol.md
│   └── home-assistant.md
├── hardware/
│   └── README.md
└── captures/
    └── README.md
```

## Contributing

Protocol captures, testing on other ECOLO models, documentation corrections, and decoded status fields are welcome.

Please include:

- Exact model
- Controller revision if visible
- Operating condition
- Packet A and Packet B samples
- Whether the physical keypad was used
- Whether the unit was powered, idle, heating, or in an error state

## License

MIT. See [LICENSE](LICENSE).
