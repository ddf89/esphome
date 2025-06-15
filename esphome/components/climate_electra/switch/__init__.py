from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_FAN

from ..climate import CONFIG_SCHEMA, ElectraClimate, electra_ns

CODEOWNERS = ["@ddf89"]
CONF_ELECTRA_ID = "electra_id"
CONF_IFEEL = "ifeel"

IFeelSwitch = electra_ns.class_("IFeelSwitch", switch.Switch)

CONFIG_SCHEMA.add_extra(
    {
        cv.GenerateID(CONF_ELECTRA_ID): cv.use_id(ElectraClimate),
        cv.Optional(CONF_IFEEL): switch.switch_schema(
            IFeelSwitch,
            icon=ICON_FAN,
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="DISABLED",
        ),
    }
)


async def to_code(config):
    await switch.new_switch(config)
