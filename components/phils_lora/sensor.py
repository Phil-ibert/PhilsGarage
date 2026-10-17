import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_DECIBEL,
    UNIT_DECIBEL_MILLIWATT,
)

from . import CONF_PHILS_LORA_ID, PhilsLora

CONF_FIELD = "field"

TYPE_FIELD = "field"
TYPE_RSSI = "rssi"
TYPE_SNR = "snr"
TYPE_VALID_PACKETS = "valid_packets"
TYPE_REJECTED_PACKETS = "rejected_packets"


def validate_field_key(value):
    value = cv.string_strict(value)
    if not value or any(c in value for c in ":;#$~"):
        raise cv.Invalid("Le nom de champ ne doit pas être vide ni contenir : ; # $ ~")
    return value


_PARENT = {cv.GenerateID(CONF_PHILS_LORA_ID): cv.use_id(PhilsLora)}

CONFIG_SCHEMA = cv.typed_schema(
    {
        # Valeur numérique d'un champ du message, ex. field: T -> "T:21.50"
        TYPE_FIELD: sensor.sensor_schema().extend(
            {**_PARENT, cv.Required(CONF_FIELD): validate_field_key}
        ),
        TYPE_RSSI: sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(_PARENT),
        TYPE_SNR: sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL,
            accuracy_decimals=1,
            icon="mdi:signal-variant",
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(_PARENT),
        TYPE_VALID_PACKETS: sensor.sensor_schema(
            accuracy_decimals=0,
            icon="mdi:check-network-outline",
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(_PARENT),
        TYPE_REJECTED_PACKETS: sensor.sensor_schema(
            accuracy_decimals=0,
            icon="mdi:close-network-outline",
            state_class=STATE_CLASS_TOTAL_INCREASING,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend(_PARENT),
    },
    key=CONF_TYPE,
    default_type=TYPE_FIELD,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PHILS_LORA_ID])
    var = await sensor.new_sensor(config)
    kind = config[CONF_TYPE]
    if kind == TYPE_FIELD:
        cg.add(parent.add_field_sensor(config[CONF_FIELD], var))
    elif kind == TYPE_RSSI:
        cg.add(parent.set_rssi_sensor(var))
    elif kind == TYPE_SNR:
        cg.add(parent.set_snr_sensor(var))
    elif kind == TYPE_VALID_PACKETS:
        cg.add(parent.set_valid_packets_sensor(var))
    elif kind == TYPE_REJECTED_PACKETS:
        cg.add(parent.set_rejected_packets_sensor(var))
