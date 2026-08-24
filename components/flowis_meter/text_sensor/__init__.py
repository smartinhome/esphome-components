import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor

from .. import TEXT_FIELDS, FlowisMeter, flowis_meter_ns

CONF_PARENT_ID = "parent_id"
CONF_FIELD = "field"

FlowisTextSensor = flowis_meter_ns.class_(
    "FlowisTextSensor",
    text_sensor.TextSensor,
    cg.Component,
    cg.Parented.template(FlowisMeter),
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(FlowisTextSensor)
    .extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(FlowisMeter),
            cv.Required(CONF_FIELD): cv.one_of(*TEXT_FIELDS, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_PARENT_ID])
    cg.add(var.set_field(TEXT_FIELDS[config[CONF_FIELD]]))
