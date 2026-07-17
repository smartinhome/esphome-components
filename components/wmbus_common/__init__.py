import os
from pathlib import Path
import logging
import re

import esphome.config_validation as cv
from esphome import codegen as cg
from esphome.const import CONF_ID, SOURCE_FILE_EXTENSIONS

LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@SzczepanLeon", "@kubasaw"]
CONF_DRIVERS = "drivers"

wmbus_common_ns = cg.esphome_ns.namespace("wmbus_common")
WMBusCommon = wmbus_common_ns.class_("WMBusCommon", cg.Component)


AVAILABLE_DRIVERS = {
    f.stem.removeprefix("driver_") for f in Path(__file__).parent.glob("driver_*.cpp")
}

# Keep ".cc" registered so ESPHome's writer cleans up stale *.cc copies left
# in existing build trees from older versions of this component (sources were
# renamed to .cpp for the native ESP-IDF/CMake build in ESPHome >=2026.7,
# which only compiles *.cpp/*.c).
SOURCE_FILE_EXTENSIONS.add(".cc")


def FILTER_SOURCE_FILES() -> list[str]:
    """Exclude driver_*.cpp files for drivers not selected in the config.

    This replaces the PlatformIO ``extra_scripts`` pre-build filter, which no
    longer runs under the native ESP-IDF/CMake build used by ESPHome >=2026.7.
    Filtering happens at source-copy time, so unselected drivers never reach
    the build tree on either build system.
    """
    selected = _SELECTED_DRIVERS
    if not selected:
        # No explicit selection: keep all drivers (same as the old behaviour).
        return []
    return [
        f"driver_{name}.cpp"
        for name in AVAILABLE_DRIVERS
        if name not in selected
    ]


# Populated in to_code(); read by FILTER_SOURCE_FILES during source copying,
# which ESPHome runs after code generation.
_SELECTED_DRIVERS: set[str] = set()


def validate_driver(value):
    """Validate meter driver name used by wmbus_meter.

    Accepts "auto" (let wmbusmeters pick based on telegram contents) or a
    specific driver from AVAILABLE_DRIVERS.
    """
    if value == "auto":
        return value
    return cv.one_of(*AVAILABLE_DRIVERS, lower=True, space="_")(value)


def _validate_drivers(value):
    if value == "all":
        return set(AVAILABLE_DRIVERS)
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WMBusCommon),
        cv.Optional(CONF_DRIVERS, default=[]): cv.All(
            cv.Any(
                "all",
                cv.ensure_list(cv.one_of(*AVAILABLE_DRIVERS, lower=True, space="_")),
            ),
            _validate_drivers,
        ),
    }
)


async def to_code(config):
    global _SELECTED_DRIVERS
    selected_drivers = set(config.get(CONF_DRIVERS, set()))
    _SELECTED_DRIVERS = selected_drivers

    if selected_drivers:
        selected = ",".join(sorted(selected_drivers))
        # Expose for C++ (defines.h) and for PlatformIO pre-build filtering.
        cg.add_define("ESPHOME_WMBUS_INCLUDE_DRIVERS", selected)
        cg.add_platformio_option("custom_wmbus_include_drivers", selected)
        LOGGER.info("wmbus_common: selected_drivers=%s", sorted(selected_drivers))

    # Pre-build script physically disables non-selected drivers in the build tree.
    script_path = os.path.join(os.path.dirname(__file__), "filter_wmbus_drivers.py")
    if os.path.exists(script_path):
        cg.add_platformio_option("extra_scripts", [f"pre:{script_path}"])

    # Drivers self-register via static initializers (registerDriver in each
    # driver_*.cpp). Under the native ESP-IDF build (ESPHome >=2026.7) sources
    # are linked as a static archive, so the linker drops driver objects that
    # nothing references and their registrations never run. Reference each
    # driver's keep-symbol anchor from generated code to force-link them.
    anchored = sorted(selected_drivers) if selected_drivers else sorted(AVAILABLE_DRIVERS)
    for name in anchored:
        sym = "wmbus_driver_keep_" + re.sub(r"[^0-9A-Za-z_]", "_", name)
        cg.add_global(cg.RawStatement(f'extern "C" void {sym}();'))
        cg.add(cg.RawExpression(f"{sym}()"))

    var = cg.new_Pvariable(config[CONF_ID], sorted(selected_drivers))
    await cg.register_component(var, config)
