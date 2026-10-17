import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from . import CONF_PHILS_LORA_ID, PhilsLora, phils_lora_ns
from .sensor import validate_field_key

PhilsLoraSwitch = phils_lora_ns.class_(
    "PhilsLoraSwitch", switch.Switch, cg.Component, cg.Parented.template(PhilsLora)
)

CONF_COMMAND_FIELD = "command_field"
CONF_STATE_FIELD = "state_field"
CONF_REPEAT = "repeat"
CONF_REPEAT_INTERVAL = "repeat_interval"
CONF_CONFIRM_DELAY = "confirm_delay"

CONFIG_SCHEMA = (
    switch.switch_schema(
        PhilsLoraSwitch,
        block_inverted=True,
        # Jamais de commande envoyée au démarrage de la passerelle.
        default_restore_mode="DISABLED",
    )
    .extend(
        {
            cv.GenerateID(CONF_PHILS_LORA_ID): cv.use_id(PhilsLora),
            cv.Optional(CONF_COMMAND_FIELD, default="S"): validate_field_key,
            cv.Optional(CONF_STATE_FIELD, default="L"): validate_field_key,
            cv.Optional(CONF_REPEAT, default=3): cv.int_range(min=1, max=10),
            cv.Optional(
                CONF_REPEAT_INTERVAL, default="600ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_CONFIRM_DELAY, default="2s"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_PHILS_LORA_ID])
    cg.add(var.set_command_field(config[CONF_COMMAND_FIELD]))
    cg.add(var.set_state_field(config[CONF_STATE_FIELD]))
    cg.add(var.set_repeat(config[CONF_REPEAT]))
    cg.add(var.set_repeat_interval(config[CONF_REPEAT_INTERVAL]))
    cg.add(var.set_confirm_delay(config[CONF_CONFIRM_DELAY]))
