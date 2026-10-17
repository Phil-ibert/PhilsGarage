import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TIMEOUT,
    CONF_TYPE,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import CONF_PHILS_LORA_ID, PhilsLora, phils_lora_ns
from .sensor import CONF_FIELD, validate_field_key

PhilsLoraLinkSensor = phils_lora_ns.class_(
    "PhilsLoraLinkSensor", binary_sensor.BinarySensor
)

TYPE_FIELD = "field"
TYPE_LINK = "link"

_PARENT = {cv.GenerateID(CONF_PHILS_LORA_ID): cv.use_id(PhilsLora)}

CONFIG_SCHEMA = cv.typed_schema(
    {
        # Champ booléen du message, ex. field: L -> "L:1" = ON
        TYPE_FIELD: binary_sensor.binary_sensor_schema().extend(
            {**_PARENT, cv.Required(CONF_FIELD): validate_field_key}
        ),
        # ON tant qu'un paquet valide a été reçu depuis moins de `timeout`
        TYPE_LINK: binary_sensor.binary_sensor_schema(
            PhilsLoraLinkSensor,
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(
            {
                **_PARENT,
                cv.Optional(
                    CONF_TIMEOUT, default="30min"
                ): cv.positive_time_period_milliseconds,
            }
        ),
    },
    key=CONF_TYPE,
    default_type=TYPE_FIELD,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PHILS_LORA_ID])
    var = await binary_sensor.new_binary_sensor(config)
    if config[CONF_TYPE] == TYPE_LINK:
        cg.add(var.set_timeout(config[CONF_TIMEOUT]))
        cg.add(parent.add_link_sensor(var))
    else:
        cg.add(parent.add_field_binary_sensor(config[CONF_FIELD], var))
