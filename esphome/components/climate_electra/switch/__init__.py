from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_FAN

from ..climate import CONFIG_SCHEMA, electra_ns

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
    await switch.new_switch(IFeelSwitch)
