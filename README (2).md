# Home Assistant

## Entities

- Ecolo Power
- Ecolo Temperature Up
- Ecolo Temperature Down
- Ecolo Target Temperature
- Ecolo Water Temperature
- Ecolo Heating
- Ecolo Model
- Ecolo Current Consumption
- Ecolo Power Consumption
- Ecolo Energy Today
- Ecolo Energy Total

## Dashboard example

```yaml
type: entities
title: ECOLO
entities:
  - switch.ecolo_power
  - select.ecolo_model
  - sensor.ecolo_water_temperature
  - sensor.ecolo_target_temperature
  - binary_sensor.ecolo_heating
  - sensor.ecolo_current_consumption
  - sensor.ecolo_power_consumption
  - sensor.ecolo_energy_today
  - sensor.ecolo_energy_total
  - button.ecolo_temperature_up
  - button.ecolo_temperature_down
```

Energy values are estimates based on rated current rather than a calibrated meter.
