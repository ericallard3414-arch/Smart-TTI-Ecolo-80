# Installation

## 1. Copy the external component

Copy:

```text
esphome/my_components/ecolo80/
```

to:

```text
/config/esphome/my_components/ecolo80/
```

The final path should contain:

```text
/config/esphome/my_components/ecolo80/__init__.py
/config/esphome/my_components/ecolo80/ecolo80.h
/config/esphome/my_components/ecolo80/ecolo80.cpp
```

## 2. Copy the example configuration

Copy `esphome/ecolo-80.yaml` into your ESPHome directory.

Review:

- Device name
- Friendly name
- Wi-Fi secrets
- API encryption key
- OTA configuration
- GPIO4 receive pin
- GPIO18 transmit pin

## 3. Validate

In ESPHome, select **Validate**.

## 4. Compile and install

Compile and install by USB the first time. OTA updates can be used afterward.

## 5. Add to Home Assistant

Home Assistant should discover the ESPHome device automatically. Add it through:

```text
Settings -> Devices & services -> ESPHome
```

## 6. Verify

Confirm that:

- Water temperature updates
- Target temperature updates
- Power switch follows the physical keypad
- Temperature buttons work
- Heating turns ON while the heat pump is heating
- Heating turns OFF while idle
