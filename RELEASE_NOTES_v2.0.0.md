# Smart-TTI-Ecolo-80 v2.0.0

## Production Release

Smart-TTI-Ecolo-80 v2.0.0 is the first stable public production release.

## Compatibility

Compatible only with TTI ECOLO heat pumps equipped with:

- **Manufacturer:** GUANGDONG CHICO
- **Main board model:** CC207S-V2.1

Other controller boards are not supported by this release.

## Included

- Native power ON/OFF
- Water temperature
- Target temperature
- Temperature Up/Down
- Heating ON/OFF binary sensor
- Home Assistant ECOLO model dropdown
- ECOLO 50 / 65 / 80 / 100 / 120 support
- Estimated current and power consumption
- Estimated daily and cumulative energy
- Five-second consumption refresh
- Persistent model selection
- Factory keypad compatibility
- Two-way protocol communication
- Checksum and acknowledgement validation

## Model table

| Model | Current | Estimated power |
|---|---:|---:|
| ECOLO 50 | 10 A | 2.40 kW |
| ECOLO 65 | 12 A | 2.88 kW |
| ECOLO 80 | 18 A | 4.32 kW |
| ECOLO 100 | 21 A | 5.04 kW |
| ECOLO 120 | 24 A | 5.76 kW |

Energy values are estimates based on rated operating current at 240 V.
