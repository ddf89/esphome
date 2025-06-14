from esphome.components import switch

from ..climate import electra_ns

CODEOWNERS = ["@ddf89"]

IFeelSwitch = electra_ns.class_("IFeelSwitch", switch.Switch)


async def to_code(config):
    await switch.new_switch(config)
