/*
 Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"meters_common_implementation.h"

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        [[maybe_unused]] Driver(MeterInfo &mi, DriverInfo &di);
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("op041a");
        di.setDefaultFields("name,id,total_m3,meter_datetime,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        // Apator water meter, version 0x1a, device 0x07 (water).
        di.addDetection(MANUFACTURER_APA, 0x07, 0x1a);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return std::shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("meter_datetime");

        addNumericFieldWithExtractor(
            "total",
            "The total water consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );
    }
}

// Test: Water op041a 11567199 NOKEY
// telegram=|3e440106997156111a077af10030a57b9d224befc4cefe88af183cf60f426d30e9abf4d67f2996245ba3d228c991d04e12785552c8640c31441c7d36b3d3e1|
// {"media":"water","meter":"op041a","name":"Water","id":"11567199","total_m3":1.308,"timestamp":"1111-11-11T11:11:11Z"}
// |Water;11567199;1.308;1111-11-11 11:11.11

// Keep-symbol anchor: referenced from ESPHome-generated main.cpp so the
// linker pulls this object file (and its static driver registration) out of
// the static archive under the native ESP-IDF build (ESPHome >=2026.7).
extern "C" void wmbus_driver_keep_op041a() {}
