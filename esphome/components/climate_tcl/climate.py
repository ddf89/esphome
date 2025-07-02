import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@ddf89"]

tcl_ns = cg.esphome_ns.namespace("tcl")
TclClimate = tcl_ns.class_("TclClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(TclClimate)


async def to_code(config):
    cg.add_library(name="crankyoldgit/IRremoteESP8266", version="")
    await climate_ir.new_climate_ir(config)
