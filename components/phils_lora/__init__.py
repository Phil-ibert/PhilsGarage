"""PhilsCryptoLoRa : passerelle ESPHome compatible avec l'émetteur Arduino du garage.

Le composant ne parle à aucune puce radio directement : le modem Semtech
(sx127x, sx126x...) est décrit dans le YAML, ce qui permet de changer de carte
sans toucher au code.
"""

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY

CODEOWNERS = ["@Phil-ibert"]
DEPENDENCIES = ["esp32"]
MULTI_CONF = True

CONF_PHILS_LORA_ID = "phils_lora_id"
CONF_ON_TRANSMIT = "on_transmit"
CONF_ON_MESSAGE = "on_message"
CONF_PAYLOAD = "payload"

phils_lora_ns = cg.esphome_ns.namespace("phils_lora")
PhilsLora = phils_lora_ns.class_("PhilsLora", cg.Component)
SendAction = phils_lora_ns.class_(
    "SendAction", automation.Action, cg.Parented.template(PhilsLora)
)


def validate_aes256_key(value):
    """Clé AES-256 : 64 caractères hexa (séparateurs ':' ' ' '-' tolérés) ou liste de 32 octets."""
    if isinstance(value, list):
        data = [cv.hex_uint8_t(v) for v in value]
    else:
        text = cv.string_strict(value)
        if "${" in text:
            raise cv.Invalid(
                f"{text} n'est pas défini : ajoute `lora_aes_key:` (valeur de la clé ou "
                "!secret phils_lora_aes_key) dans le bloc `substitutions:` du YAML du module"
            )
        for sep in (":", " ", "-", ",", "0x"):
            text = text.replace(sep, "")
        if len(text) != 64:
            raise cv.Invalid(
                f"La clé AES-256 doit faire 32 octets = 64 caractères hexa (reçu {len(text)})"
            )
        try:
            data = list(bytes.fromhex(text))
        except ValueError as err:
            raise cv.Invalid(f"Clé hexadécimale invalide : {err}") from err
    if len(data) != 32:
        raise cv.Invalid(f"La clé AES-256 doit faire 32 octets (reçu {len(data)})")
    return data


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PhilsLora),
        cv.Required(CONF_KEY): cv.sensitive(validate_aes256_key),
        cv.Optional(CONF_ON_TRANSMIT): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_MESSAGE): automation.validate_automation(single=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_key(config[CONF_KEY]))

    if CONF_ON_TRANSMIT in config:
        cg.add(var.set_has_transmit_automation(True))
        await automation.build_automation(
            var.get_transmit_trigger(),
            [(cg.std_vector.template(cg.uint8), "x")],
            config[CONF_ON_TRANSMIT],
        )
    if CONF_ON_MESSAGE in config:
        cg.add(var.set_has_message_automation(True))
        await automation.build_automation(
            var.get_message_trigger(),
            [(cg.std_string, "message"), (cg.float_, "rssi"), (cg.float_, "snr")],
            config[CONF_ON_MESSAGE],
        )


SEND_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(PhilsLora),
        cv.Required(CONF_PAYLOAD): cv.templatable(cv.string),
    },
    key=CONF_PAYLOAD,
)


@automation.register_action(
    "phils_lora.send", SendAction, SEND_ACTION_SCHEMA, synchronous=True
)
async def send_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    templ = await cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
    cg.add(var.set_payload(templ))
    return var
