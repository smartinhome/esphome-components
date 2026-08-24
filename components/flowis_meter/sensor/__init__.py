import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_WATER,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_DECIBEL_MILLIWATT,
)

from .. import NUMERIC_FIELDS, FlowisMeter, flowis_meter_ns

CONF_PARENT_ID = "parent_id"
CONF_FIELD = "field"

UNIT_CUBIC_METER = "m³"

FlowisSensor = flowis_meter_ns.class_(
    "FlowisSensor", sensor.Sensor, cg.Component, cg.Parented.template(FlowisMeter)
)

# Sensible Home Assistant defaults per field, so a minimal YAML entry still
# produces a correctly classified entity.
FIELD_DEFAULTS = {
    "total_m3": dict(
        unit_of_measurement=UNIT_CUBIC_METER,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_WATER,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        icon="mdi:water",
    ),
    "archive_m3": dict(
        unit_of_measurement=UNIT_CUBIC_METER,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_WATER,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        icon="mdi:water-check",
    ),
    "rssi_dbm": dict(
        unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:signal",
    ),
    "alarm": dict(accuracy_decimals=0, icon="mdi:alert"),
    "backflow": dict(accuracy_decimals=0, icon="mdi:backup-restore"),
}

_PERIOD_DEFAULTS = dict(
    unit_of_measurement=UNIT_CUBIC_METER,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_WATER,
    state_class=STATE_CLASS_MEASUREMENT,
    icon="mdi:chart-bar",
)
for _slot in range(7):
    FIELD_DEFAULTS[f"period_{_slot}_m3"] = dict(_PERIOD_DEFAULTS)


def _apply_defaults(config):
    """Fill in per-field entity metadata before the schema runs.

    This has to happen *before* validation, not after: sensor_schema turns
    state_class, device_class and entity_category into EnumValue objects that
    codegen dereferences, so a raw string injected afterwards reaches the
    generated C++ as a string literal and fails to compile.
    """
    if isinstance(config, dict):
        field = config.get(CONF_FIELD)
        if isinstance(field, str):
            for key, value in FIELD_DEFAULTS.get(field.lower(), {}).items():
                config.setdefault(key, value)
    return config


CONFIG_SCHEMA = cv.All(
    _apply_defaults,
    sensor.sensor_schema(FlowisSensor)
    .extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(FlowisMeter),
            # Kept as a string through validation so that _apply_defaults can
            # key off it; the C++ enum is looked up in to_code().
            cv.Required(CONF_FIELD): cv.one_of(*NUMERIC_FIELDS, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_PARENT_ID])
    cg.add(var.set_field(NUMERIC_FIELDS[config[CONF_FIELD]]))
