import esphome.codegen as cg
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_FAN

from ..climate import CONF_ELECTRA_ID, CONF_IFEEL, CONFIG_SCHEMA, electra_ns

CODEOWNERS = ["@ddf89"]

IFeelSwitch = electra_ns.class_("IFeelSwitch", switch.Switch)

CONFIG_SCHEMA.add_extra(
    switch.switch_schema(
        IFeelSwitch,
        icon=ICON_FAN,
        entity_category=ENTITY_CATEGORY_CONFIG,
        default_restore_mode="DISABLED",
    ),
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ELECTRA_ID])

    if conf := config.get(CONF_IFEEL):
        sw_var = await switch.new_switch(conf)
        await cg.register_parented(sw_var, parent)
        cg.add(getattr(parent, f"set_{CONF_IFEEL}_switch")(sw_var))
