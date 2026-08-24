import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

from ..flowis_radio import FlowisRadio, flowis_radio_ns  # noqa: F401

CODEOWNERS = ["@smartinhome"]
DEPENDENCIES = ["flowis_radio"]
MULTI_CONF = True

CONF_RADIO_ID = "radio_id"
CONF_METER_ID = "meter_id"

flowis_meter_ns = cg.esphome_ns.namespace("flowis_meter")
FlowisMeter = flowis_meter_ns.class_("FlowisMeter", cg.Component)

NumericField = flowis_meter_ns.enum("NumericField", is_class=True)
TextField = flowis_meter_ns.enum("TextField", is_class=True)

NUMERIC_FIELDS = {
    "total_m3": NumericField.TOTAL_M3,
    "archive_m3": NumericField.ARCHIVE_M3,
    "alarm": NumericField.ALARM,
    "backflow": NumericField.BACKFLOW,
    "rssi_dbm": NumericField.RSSI_DBM,
    # Consumption history.  Slot 0 is the volume since the archive snapshot,
    # slots 1..6 are the six preceding periods, newest first.
    "period_0_m3": NumericField.PERIOD_0_M3,
    "period_1_m3": NumericField.PERIOD_1_M3,
    "period_2_m3": NumericField.PERIOD_2_M3,
    "period_3_m3": NumericField.PERIOD_3_M3,
    "period_4_m3": NumericField.PERIOD_4_M3,
    "period_5_m3": NumericField.PERIOD_5_M3,
    "period_6_m3": NumericField.PERIOD_6_M3,
}

TEXT_FIELDS = {
    "device_time": TextField.DEVICE_TIME,
    "telegram_type": TextField.TELEGRAM_TYPE,
    "telegram_hex": TextField.TELEGRAM_HEX,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FlowisMeter),
        cv.GenerateID(CONF_RADIO_ID): cv.use_id(FlowisRadio),
        cv.Required(CONF_METER_ID): cv.hex_uint32_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    radio = await cg.get_variable(config[CONF_RADIO_ID])
    cg.add(var.set_radio(radio))
    cg.add(var.set_meter_id(config[CONF_METER_ID]))
