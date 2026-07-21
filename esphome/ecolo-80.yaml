esphome:
  name: ecolo-80
  friendly_name: ECOLO-HEATER
  min_version: 2026.4.0
  name_add_mac_suffix: false

esp32:
  variant: esp32
  framework:
    type: esp-idf
    advanced:
      minimum_chip_revision: "3.1"
      sram1_as_iram: true

external_components:
  - source:
      type: local
      path: my_components
    components:
      - ecolo80

logger:
  level: DEBUG

api:

ota:
  - platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

button:
  - platform: restart
    name: "Ecolo Restart"


time:
  - platform: homeassistant
    id: homeassistant_time

sensor:
  - platform: total_daily_energy
    name: "Ecolo Energy Today"
    power_id: ecolo_power_consumption
    unit_of_measurement: "kWh"
    accuracy_decimals: 2
    device_class: energy
    state_class: total_increasing
    method: left
    restore: true
    filters:
      - multiply: 0.001

  - platform: integration
    name: "Ecolo Energy Total"
    sensor: ecolo_power_consumption
    time_unit: h
    integration_method: left
    restore: true
    unit_of_measurement: "kWh"
    accuracy_decimals: 2
    device_class: energy
    state_class: total_increasing
    filters:
      - multiply: 0.001


ecolo80:
  pin:
    number: GPIO4
    mode:
      input: true
      pullup: false
      pulldown: false

  transmit_pin:
    number: GPIO18
    mode:
      output: true

  model_select:
    name: "Ecolo Model"
    icon: "mdi:pool-thermometer"

  power_switch:
    name: "Ecolo Power"
    icon: "mdi:power"

  temperature_up:
    name: "Ecolo Temperature Up"
    icon: "mdi:chevron-up"

  temperature_down:
    name: "Ecolo Temperature Down"
    icon: "mdi:chevron-down"

  target_temperature:
    name: "Ecolo Target Temperature"

  current_temperature:
    name: "Ecolo Water Temperature"

  heating:
    name: "Ecolo Heating"
    icon: "mdi:fire"

  current_consumption:
    name: "Ecolo Current Consumption"
    icon: "mdi:current-ac"

  power_consumption:
    id: ecolo_power_consumption
    name: "Ecolo Power Consumption"
    icon: "mdi:flash"
