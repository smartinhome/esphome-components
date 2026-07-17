/*
 Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

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
        di.setName("microclima_son");
        di.setDefaultFields("name,id,status,total_energy_consumption_kwh,total_volume_m3,timestamp");
        di.setMeterType(MeterType::HeatMeter);
        di.addLinkMode(LinkMode::T1);
        // Sontex/Microclima heat meter, version 0x1c, device 0x04 (heat).
        di.addDetection(MANUFACTURER_SON, 0x04, 0x1c);
        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return std::shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addOptionalLibraryFields("meter_datetime,model_version,parameter_set");
        addOptionalLibraryFields("flow_temperature_c,return_temperature_c");

        addStringFieldWithExtractorAndLookup(
            "status",
            "Meter status from error flags and tpl status field.",
            DEFAULT_PRINT_PROPERTIES  |
            PrintProperty::STATUS | PrintProperty::INCLUDE_TPL_STATUS,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ErrorFlags),
            Translate::Lookup(
                {
                    {
                        {
                            "ERROR_FLAGS",
                            Translate::MapType::BitToString,
                            AlwaysTrigger, MaskBits(0xffff),
                            "OK",
                            {
                            }
                        },
                    },
                }));

        addNumericFieldWithExtractor(
            "total_energy_consumption",
            "The total heat energy consumption recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            );

        addNumericFieldWithExtractor(
            "total_volume",
            "The total heating media volume recorded by this meter.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Volume,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "volume_flow",
            "The current heat media volume flow.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Flow,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::VolumeFlow)
            );

        addNumericFieldWithExtractor(
            "power",
            "The current power consumption.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Power,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::PowerW)
            );

        addNumericFieldWithExtractor(
            "temperature_difference",
            "The difference between flow and return media temperatures.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Temperature,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::TemperatureDifference)
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date_{storage_counter}",
            "The total energy consumption at the historic date.",
            DEFAULT_PRINT_PROPERTIES,
            Quantity::Energy,
            VifScaling::Auto, DifSignedness::Signed,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyEnergyVIF)
            .set(StorageNr(1),StorageNr(31))
            );
    }
}

// The meter sends two types of telegrams: a shorter one with current values
// and a longer one with historical (per storage) values.

// Test: Heat microclima_son 36411298 NOKEY
// telegram=|ae44ee4d981241361c047a8800a0253b284476637cd82b91e5763f0fb7f0cbfed9d4fea319c4e027769b753b941adfc355e2972bbffe8713c2f424852dc53ac3ddbcee61df42fb997614850d9f4cef18611921cf325888f9787890da055244641b1f2b41bd4a87571f8c305eca56c7f735ee23a636a3e9ca7183ae205cffaa536e15b13f76204c13f4673ca672dba71026b4dd685722c7c3957aefc14cdb6a37890d7026ba1fcb6ec988e6cf22e1f8|
// {"media":"heat","meter":"microclima_son","name":"Heat","id":"36411298","status":"OK","total_energy_consumption_kwh":50,"total_volume_m3":8.795,"timestamp":"1111-11-11T11:11:11Z"}
// |Heat;36411298;OK;50;8.795;1111-11-11 11:11.11

// Keep-symbol anchor: referenced from ESPHome-generated main.cpp so the
// linker pulls this object file (and its static driver registration) out of
// the static archive under the native ESP-IDF build (ESPHome >=2026.7).
extern "C" void wmbus_driver_keep_microclima_son() {}
