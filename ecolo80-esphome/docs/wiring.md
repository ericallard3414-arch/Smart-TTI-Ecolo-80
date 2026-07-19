# Wiring

## Confirm the transistor pinout first

The pin order of a 2N3904 depends on the package and manufacturer. Do not assume left/base/right orientation from a generic drawing. Check the datasheet for the exact part you have.

## Shared ground

The following points must share the same electrical ground:

- Heater yellow wire
- DROK OUT-
- ESP32 GND
- 2N3904 emitter
- Bottom of the 22 kΩ receive resistor
- Bottom of the 100 kΩ base pull-down resistor

## Power supply

```text
Heater Brown -> Fuse -> DROK IN+
Heater Yellow --------> DROK IN-

DROK OUT+ set to 5.0 V -> ESP32 5V
DROK OUT- ------------> ESP32 GND
```

Measure the DROK output before connecting the ESP32.

## Receive circuit

```text
Heater Blue ---- 10 kΩ ----+---- GPIO4
                           |
                          22 kΩ
                           |
                          GND
```

The 10 kΩ and 22 kΩ resistors form the receive divider used by the working installation.

## Transmit circuit

```text
GPIO18 ---- 4.7 kΩ ----+---- Base
                        |
                      100 kΩ
                        |
                       GND

Emitter ------------------- GND
Collector ----------------- Heater Blue
```

The transistor behaves as an open-collector bus pull-down:

- GPIO18 LOW: transistor released
- GPIO18 HIGH: transistor pulls the blue bus LOW

## Heater wires used

| Heater wire | Function observed |
|---|---|
| Brown | Approximately 16–17 V DC supply |
| Yellow | Ground |
| Blue | Shared data bus |

## Final ESP32 pins

| Purpose | Pin |
|---|---|
| Receive | GPIO4 |
| Transmit | GPIO18 |
| Power | 5V |
| Ground | GND |
