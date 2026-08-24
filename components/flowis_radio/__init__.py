import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import spi
from esphome.const import CONF_ID, CONF_FREQUENCY, CONF_TRIGGER_ID

CODEOWNERS = ["@smartinhome"]
DEPENDENCIES = ["esp32", "spi"]
MULTI_CONF = True

CONF_GDO0_PIN = "gdo0_pin"
CONF_GDO2_PIN = "gdo2_pin"
CONF_ON_TELEGRAM = "on_telegram"

flowis_radio_ns = cg.esphome_ns.namespace("flowis_radio")
FlowisRadio = flowis_radio_ns.class_("FlowisRadio", cg.Component, spi.SPIDevice)
FlowisTelegram = flowis_radio_ns.struct("FlowisTelegram")
TelegramTrigger = flowis_radio_ns.class_(
    "TelegramTrigger", automation.Trigger.template(FlowisTelegram)
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FlowisRadio),
            # GDO0 is configured as "RX FIFO at threshold" and the threshold is
            # a full FIFO, so the pin means "a complete packet is waiting".
            # Optional: without it the component polls RXBYTES over SPI.
            cv.Optional(CONF_GDO0_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_GDO2_PIN): pins.internal_gpio_input_pin_schema,
            # Flowis modules are not on the wM-Bus channel.  Confirm the
            # carrier for your installation before trusting the default.
            cv.Optional(CONF_FREQUENCY, default=868.000): cv.float_range(
                min=300.0, max=928.0
            ),
            cv.Optional(CONF_ON_TELEGRAM): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TelegramTrigger)}
            ),
        }
    )
    .extend(spi.spi_device_schema(cs_pin_required=True))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if CONF_GDO0_PIN in config:
        cg.add(var.set_gdo0_pin(await cg.gpio_pin_expression(config[CONF_GDO0_PIN])))
    if CONF_GDO2_PIN in config:
        cg.add(var.set_gdo2_pin(await cg.gpio_pin_expression(config[CONF_GDO2_PIN])))

    cg.add(var.set_frequency(config[CONF_FREQUENCY]))

    for conf in config.get(CONF_ON_TELEGRAM, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(FlowisTelegram, "telegram")], conf
        )
