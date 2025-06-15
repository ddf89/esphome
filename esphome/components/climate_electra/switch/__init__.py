from esphome.components import switch
import esphome.codegen as cg

import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_FAN

from ..climate import ElectraClimate, electra_ns

CODEOWNERS = ["@ddf89"]
CONF_ELECTRA_ID = "electra_id"
CONF_IFEEL = "ifeel"

IFeelSwitch = electra_ns.class_("IFeelSwitch", switch.Switch)

CONFIG_SCHEMA = cv.Schema(
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
    parent = await cg.get_variable(config[CONF_ELECTRA_ID])

    if conf := config.get(CONF_IFEEL):
        sw_var = await switch.new_switch(conf)
        await cg.register_parented(sw_var, parent)
        cg.add(getattr(parent, f"set_{CONF_IFEEL}_switch")(sw_var))

    await switch.new_switch(config)
