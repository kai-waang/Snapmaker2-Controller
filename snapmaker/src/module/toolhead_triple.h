//
// Created by Kai Wang on 23-11-28.
//

#ifndef SNAPMAKER2_CONTROLLER_TOOLHEAD_TRIPLE_H
#define SNAPMAKER2_CONTROLLER_TOOLHEAD_TRIPLE_H

#include "toolhead_dualextruder.h"

class ToolHeadTriple: public ToolHeadDualExtruder {
public:
    explicit ToolHeadTriple(ModuleDeviceID id);

    /**
     * @brief Init module toolhead with triple extruders
     * @param mac
     * @param mac_index
     * @return ErrCode
     */
    ErrCode Init(MAC_t &mac, uint8_t mac_index) override;

    /**
     *
     * @param new_extruder
     * @param use_compensation
     * @return
     */
    ErrCode ToolChange(uint8_t new_extruder, bool use_compensation);

private:
    // some private variables are inherited from ToolHeadDualExtruder
    static constexpr uint8_t EXTRUDER_LEFT = 0;
    static constexpr uint8_t EXTRUDER_RIGHT = 1;
    static constexpr uint8_t EXTRUDER_EXTRA = 2;

};

extern ToolHeadTriple printer_triple;


#endif //SNAPMAKER2_CONTROLLER_TOOLHEAD_TRIPLE_H
