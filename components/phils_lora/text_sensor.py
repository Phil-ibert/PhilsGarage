import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_PHILS_LORA_ID, PhilsLora

TYPE_LAST_MESSAGE = "last_message"
TYPE_LAST_ERROR = "last_error"

_PARENT = {cv.GenerateID(CONF_PHILS_LORA_ID): cv.use_id(PhilsLora)}

CONFIG_SCHEMA = cv.typed_schema(
    {
        # Dernier message déchiffré et validé (ex. "#T:21.5;H:55.2;L:1;E:12.3;P:345$")
        TYPE_LAST_MESSAGE: text_sensor.text_sensor_schema(
            icon="mdi:message-text-outline",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(_PARENT),
        # Raison du dernier rejet (CRC incorrect, longueur invalide...)
        TYPE_LAST_ERROR: text_sensor.text_sensor_schema(
            icon="mdi:message-alert-outline",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(_PARENT),
    },
    key=CONF_TYPE,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PHILS_LORA_ID])
    var = await text_sensor.new_text_sensor(config)
    if config[CONF_TYPE] == TYPE_LAST_MESSAGE:
        cg.add(parent.set_last_message_sensor(var))
    else:
        cg.add(parent.set_last_error_sensor(var))
