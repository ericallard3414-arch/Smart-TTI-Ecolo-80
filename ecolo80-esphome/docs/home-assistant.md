# Home Assistant

## Entities

The production build exposes:

- Power switch
- Temperature Up button
- Temperature Down button
- Target temperature
- Water temperature
- Heating binary sensor

## Example automation: notify when heating stops

```yaml
alias: ECOLO heating finished
trigger:
  - platform: state
    entity_id: binary_sensor.ecolo_heating
    from: "on"
    to: "off"
action:
  - service: notify.mobile_app_your_phone
    data:
      message: "The ECOLO heat pump stopped heating."
```

## Example automation: turn off overnight

```yaml
alias: ECOLO off overnight
trigger:
  - platform: time
    at: "23:00:00"
action:
  - service: switch.turn_off
    target:
      entity_id: switch.ecolo_power
```

## Dashboard example

```yaml
type: entities
title: ECOLO-80
entities:
  - switch.ecolo_power
  - sensor.ecolo_water_temperature
  - sensor.ecolo_target_temperature
  - binary_sensor.ecolo_heating
  - button.ecolo_temperature_up
  - button.ecolo_temperature_down
```
