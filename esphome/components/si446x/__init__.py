from esphome import pins
import esphome.codegen as cg
from esphome.components import spi
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@davidf89"]

si446x_ns = cg.esphome_ns.namespace("si446x")
SI446xComponent = si446x_ns.class_("SI446xComponent", cg.Component, spi.SPIDevice)

CONF_SDN_PIN = "sdn"
CONF_NIRQ_PIN = "nirq"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SI446xComponent),
            cv.Required(CONF_SDN_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_NIRQ_PIN): pins.gpio_input_pin_schema,
        }
    )
    .extend(
        spi.spi_device_schema(
            cs_pin_required=True,
            default_data_rate=1000000,
            default_mode=spi.SPI_MODE_OPTIONS[3],
            mode=spi.TYPE_SINGLE,
        )
    )
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = spi.final_validate_device_schema(
    "si446x",
    require_miso=True,
    require_mosi=True,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if sdn_pin := config.get(CONF_SDN_PIN):
        cg.add(var.set_sdn_pin(await cg.gpio_pin_expression(sdn_pin)))  # type: ignore
    if nirq_pin := config.get(CONF_NIRQ_PIN):
        cg.add(var.set_nirq_pin(await cg.gpio_pin_expression(nirq_pin)))  # type: ignore
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)


# SI446x_ID_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(SI446xComponent)})
