//
// Created by Kai Wang on 23-11-28.
//

#ifndef SNAPMAKER2_CONTROLLER_TOOLHEAD_TRIPLE_H
#define SNAPMAKER2_CONTROLLER_TOOLHEAD_TRIPLE_H

#include "toolhead_dualextruder.h"

class ToolHeadTriple: public ToolHeadDualExtruder {
public:
    ToolHeadTriple(ModuleDeviceID id);

    /**
     * @brief Init module toolhead with triple extruders
     * @param mac
     * @param mac_index
     * @return ErrCode
     */
    ErrCode Init(MAC_t &mac, uint8_t mac_index);

    /**
     *
     * @param new_extruder
     * @param use_compensation
     * @return
     */
    ErrCode ToolChange(uint8_t new_extruder, bool use_compensation = true);

private:
//    probe_sensor_t active_probe_sensor[EXTRUDERS];
//    uint8_t hotend_type_[EXTRUDERS];
//    uint16_t target_temp_[EXTRUDERS];

};

extern ToolHeadTriple printer_triple;


#endif //SNAPMAKER2_CONTROLLER_TOOLHEAD_TRIPLE_H
