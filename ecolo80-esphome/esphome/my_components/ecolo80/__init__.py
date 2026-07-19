import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import binary_sensor, button, sensor, switch
from esphome.const import (
    CONF_ID,
    CONF_PIN,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["binary_sensor", "button", "sensor", "switch"]

ecolo80_ns = cg.esphome_ns.namespace("ecolo80")

Ecolo80Component = ecolo80_ns.class_(
    "Ecolo80Component",
    cg.Component,
)

Ecolo80PowerSwitch = ecolo80_ns.class_(
    "Ecolo80PowerSwitch",
    switch.Switch,
)





Ecolo80TemperatureUpButton = ecolo80_ns.class_(
    "Ecolo80TemperatureUpButton",
    button.Button,
)

Ecolo80TemperatureDownButton = ecolo80_ns.class_(
    "Ecolo80TemperatureDownButton",
    button.Button,
)







CONF_TRANSMIT_PIN = "transmit_pin"
CONF_POWER_SWITCH = "power_switch"
CONF_TEMPERATURE_UP = "temperature_up"
CONF_TEMPERATURE_DOWN = "temperature_down"
CONF_HEATING = "heating"
CONF_TARGET_TEMPERATURE = "target_temperature"
CONF_CURRENT_TEMPERATURE = "current_temperature"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Ecolo80Component),

            cv.Required(CONF_PIN):
                pins.internal_gpio_input_pin_schema,

            cv.Required(CONF_TRANSMIT_PIN):
                pins.internal_gpio_output_pin_schema,

            cv.Required(CONF_POWER_SWITCH):
                switch.switch_schema(Ecolo80PowerSwitch),

            cv.Required(CONF_TEMPERATURE_UP):
                button.button_schema(Ecolo80TemperatureUpButton),

            cv.Required(CONF_TEMPERATURE_DOWN):
                button.button_schema(Ecolo80TemperatureDownButton),

            cv.Required(CONF_TARGET_TEMPERATURE):
                sensor.sensor_schema(
                    unit_of_measurement="°F",
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_TEMPERATURE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),

            cv.Required(CONF_CURRENT_TEMPERATURE):
                sensor.sensor_schema(
                    unit_of_measurement="°F",
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_TEMPERATURE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),

            cv.Required(CONF_HEATING):
                binary_sensor.binary_sensor_schema(),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    component = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(component, config)

    receive_pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(component.set_pin(receive_pin))

    transmit_pin = await cg.gpio_pin_expression(config[CONF_TRANSMIT_PIN])
    cg.add(component.set_transmit_pin(transmit_pin))

    power_switch = await switch.new_switch(config[CONF_POWER_SWITCH])
    cg.add(power_switch.set_parent(component))
    cg.add(component.set_power_switch(power_switch))

    temperature_up = await button.new_button(
        config[CONF_TEMPERATURE_UP]
    )
    cg.add(temperature_up.set_parent(component))

    temperature_down = await button.new_button(
        config[CONF_TEMPERATURE_DOWN]
    )
    cg.add(temperature_down.set_parent(component))

    target_temperature = await sensor.new_sensor(
        config[CONF_TARGET_TEMPERATURE]
    )
    cg.add(component.set_target_temperature_sensor(target_temperature))

    current_temperature = await sensor.new_sensor(
        config[CONF_CURRENT_TEMPERATURE]
    )
    cg.add(component.set_current_temperature_sensor(current_temperature))

    heating = await binary_sensor.new_binary_sensor(config[CONF_HEATING])
    cg.add(component.set_heating_sensor(heating))
