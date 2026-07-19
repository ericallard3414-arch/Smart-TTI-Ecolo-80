# Installation

## Compatibility

This release is compatible only with TTI ECOLO heat pumps using the **GUANGDONG CHICO CC207S-V2.1** main board.


## Copy files

Copy:

```text
esphome/my_components/ecolo80/
```

to:

```text
/config/esphome/my_components/ecolo80/
```

Copy `esphome/ecolo-80.yaml` into the ESPHome directory.

## Configure and install

Review Wi-Fi, API, OTA, device name, GPIO4 RX, and GPIO18 TX. Validate, compile, and install.

## Select the heater model

The model is selected in Home Assistant using **Ecolo Model**:

- ECOLO 50
- ECOLO 65
- ECOLO 80
- ECOLO 100
- ECOLO 120

The choice is saved in ESP32 flash.

## Verify

Confirm that power, temperatures, heating, current, power, and energy entities update correctly. Current and power refresh every five seconds and immediately on heating/model changes.
