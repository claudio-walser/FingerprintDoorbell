import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor, binary_sensor
from esphome.const import (
    CONF_ID,
    UNIT_PERCENT,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]
CODEOWNERS = ["@yourusername"]

# Define the namespace
fingerprint_sensor_ns = cg.esphome_ns.namespace("fingerprint_sensor")
FingerprintSensor = fingerprint_sensor_ns.class_(
    "FingerprintSensor", cg.Component, uart.UARTDevice
)

# Configuration keys
CONF_MATCH_ID = "match_id"
CONF_MATCH_NAME = "match_name"
CONF_CONFIDENCE = "confidence"
CONF_ENROLLED_COUNT = "enrolled_count"
CONF_STATUS = "status"
CONF_RING = "ring"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FingerprintSensor),
        cv.Optional(CONF_MATCH_ID): sensor.sensor_schema(
            icon="mdi:fingerprint",
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_MATCH_NAME): text_sensor.text_sensor_schema(
            icon="mdi:account",
        ),
        cv.Optional(CONF_CONFIDENCE): sensor.sensor_schema(
            icon="mdi:percent",
            accuracy_decimals=0,
            unit_of_measurement=UNIT_PERCENT,
        ),
        cv.Optional(CONF_ENROLLED_COUNT): sensor.sensor_schema(
            icon="mdi:counter",
            accuracy_decimals=0,
        ),
        cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:information",
        ),
        cv.Optional(CONF_RING): binary_sensor.binary_sensor_schema(
            icon="mdi:doorbell",
        ),
    }
).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Register sensors
    if CONF_MATCH_ID in config:
        sens = await sensor.new_sensor(config[CONF_MATCH_ID])
        cg.add(var.set_match_id_sensor(sens))

    if CONF_MATCH_NAME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_MATCH_NAME])
        cg.add(var.set_match_name_sensor(sens))

    if CONF_CONFIDENCE in config:
        sens = await sensor.new_sensor(config[CONF_CONFIDENCE])
        cg.add(var.set_confidence_sensor(sens))

    if CONF_ENROLLED_COUNT in config:
        sens = await sensor.new_sensor(config[CONF_ENROLLED_COUNT])
        cg.add(var.set_enrolled_count_sensor(sens))

    if CONF_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STATUS])
        cg.add(var.set_status_sensor(sens))

    if CONF_RING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_RING])
        cg.add(var.set_ring_sensor(sens))

    # Library is added in YAML, not here
