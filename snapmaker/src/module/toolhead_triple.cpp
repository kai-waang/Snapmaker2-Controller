//
// Created by Kai Wang on 23-11-28.
//
#include "module_base.h"
#include "toolhead_triple.h"
#include "can_host.h"

#include "common/config.h"
#include "common/debug.h"

ToolHeadTriple::ToolHeadTriple(ModuleDeviceID id) : ToolHeadDualExtruder(id)
{
//    static_assert(EXTRUDERS == 3, "EXTRUDERS Not Set Correctly");
    for(int i = 0; i < EXTRUDERS; i++)
    {
        hotend_type_[i] = INVALID_HOTEND_TYPE; // init with invalid values
        target_temp_[i] = 0;
    }

    backup_position_valid = false;
    has_sync = false;

}
