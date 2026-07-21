# Wiring

## Shared ground

Heater yellow, DROK OUT-, ESP32 GND, 2N3904 emitter, 22 kΩ ground, and 100 kΩ ground must be common.

## Power

```text
Heater Brown -> Fuse -> DROK IN+
Heater Yellow --------> DROK IN-
DROK OUT+ at 5.0 V ---> ESP32 5V
DROK OUT- ------------> ESP32 GND
```

## Receive

```text
Heater Blue ---- 10 kΩ ----+---- GPIO4
                           |
                          22 kΩ
                           |
                          GND
```

## Transmit

```text
GPIO18 ---- 4.7 kΩ ----+---- 2N3904 Base
                        |
                      100 kΩ
                        |
                       GND

Emitter ---- GND
Collector -- Heater Blue
```
