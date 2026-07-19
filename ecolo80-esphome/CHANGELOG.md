# Changelog

All notable changes to this project will be documented here.

## [1.0.0] - 2026-07-18

### Added

- ESPHome external component for ECOLO-80
- Passive packet receiver on GPIO4
- 2N3904 open-collector transmitter on GPIO18
- Power ON/OFF control with acknowledgement
- Temperature Up and Temperature Down controls
- Direct final-target temperature transmission
- Rapid click grouping
- Water temperature sensor
- Target temperature sensor
- Heating binary sensor
- Checksum validation
- Home Assistant entities
- Production cleanup with diagnostic entities removed

### Known limitations

- Tested on a single ECOLO-80 controller revision
- Fahrenheit only
- Defrost and error codes not decoded
