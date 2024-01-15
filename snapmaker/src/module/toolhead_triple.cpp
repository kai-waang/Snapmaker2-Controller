//
// Created by Kai Wang on 23-11-28.
//

#include "toolhead_triple.h"
#include "common/debug.h"
#include "snapmaker/src/common/config.h"

// marlin headers
#include "Marlin/Configuration.h"
#include "Marlin/src/core/boards.h"
#include "Marlin/src/core/macros.h"
#include "Marlin/src/feature/bedlevel/bedlevel.h"
#include "Marlin/src/inc/MarlinConfig.h"
#include "Marlin/src/module/motion.h"
#include "Marlin/src/module/temperature.h"
#include "Marlin/src/module/tool_change.h"
#include "Marlin/src/pins/pins.h"
// clang-format off
#include HAL_PATH(Marlin/src/HAL, HAL.h)
// clang-format on

ToolHeadTriple printer_triple(MODULE_DEVICE_ID_TRIPLE);

static void CallbackAckProbeState(CanStdDataFrame_t &cmd)
{
    printer_triple.ReportProbeState(cmd.data);
}

static void CallbackAckNozzleTemp(CanStdDataFrame_t &cmd)
{
    if(cmd.id.bits.length < 8)
        return;

    printer_triple.ReportTemperature(cmd.data);
}

static void CallbackAckReportPidTemp(CanStdDataFrame_t &cmd)
{
    printer_triple.ReportPID(cmd.data);
}

static void CallbackAckFilamentState(CanStdDataFrame_t &cmd)
{
    printer_triple.ReportFilamentState(cmd.data);
}

static void CallbackAckNozzleType(CanStdDataFrame_t &cmd)
{
    printer_triple.ReportHotendType(cmd.data);
}

static void CallbackAckExtruderInfo(CanStdDataFrame_t &cmd)
{
    printer_triple.ReportExtruderInfo(cmd.data);
}

static void CallbackAckReportHotendOffset(CanStdDataFrame_t &cmd)
{
    printer_triple.ReportHotendOffset(cmd.data);
}

static void CallbackAckReportProbeSensorCompensation(CanStdDataFrame_t &cmd)
{
    printer_triple.ReportProbeSensorCompensation(cmd.data);
}

static void CallbackAckReportRightExtruderPos(CanStdDataFrame_t &cmd)
{
    printer_triple.ReportRightExtruderPos(cmd.data);
}

static void CallbackAckReportHWVersion(CanStdDataFrame_t &cmd)
{
    printer_triple.ReportHWVersion(cmd.data);
}

ToolHeadTriple::ToolHeadTriple(ModuleDeviceID id) : ToolHeadDualExtruder(id)
{
    //    static_assert(EXTRUDERS == 3, "EXTRUDERS Not Set Correctly");
    for(int i = 0; i < EXTRUDERS; i++)
    {
        hotend_type_[i] = INVALID_HOTEND_TYPE;  // init with invalid values
        target_temp_[i] = 0;
    }

    backup_position_valid = false;
    has_sync              = false;
}

ErrCode ToolHeadTriple::Init(MAC_t &mac, uint8_t mac_index)
{
    ErrCode             ret;
    CanExtCmd_t         cmd;
    uint8_t             func_buffer[2 * 50 + 2];
    Function_t          function;
    message_id_t        message_id[50];
    CanStdCmdCallback_t cb = nullptr;

    // use E0_DIR_PIN because all steppers are using the Enable pin
    ret = ModuleBase::InitModule8p(mac, E0_DIR_PIN, mac_index);
    if(ret != E_SUCCESS)
        return ret;

    LOG_I("\t Got toolhead triple module\n");

    cmd.mac    = mac;
    cmd.data   = func_buffer;
    cmd.length = 1;

    cmd.data[MODULE_EXT_CMD_INDEX_ID] = MODULE_EXT_CMD_GET_FUNCID_REQ;

    // try to get function ids from module
    if(canhost.SendExtCmdSync(cmd, 500, 2) != E_SUCCESS)
        return E_FAILURE;

    function.channel   = mac.bits.channel;
    function.mac_index = mac_index;
    function.sub_index = 0;
    function.priority  = MODULE_FUNC_PRIORITY_DEFAULT;

    // assign all callbacks to specific function id of the toolhead
    for(int i = 0; i < cmd.data[MODULE_EXT_CMD_INDEX_DATA]; i++)
    {
        function.id = (cmd.data[i * 2 + 2] << 8 | cmd.data[i * 2 + 3]);
        switch(function.id)
        {
        case MODULE_FUNC_PROBE_STATE:
            cb = CallbackAckProbeState;
            break;

        case MODULE_FUNC_RUNOUT_SENSOR_STATE:
            cb = CallbackAckFilamentState;
            break;

        case MODULE_FUNC_GET_NOZZLE_TEMP:
            cb = CallbackAckNozzleTemp;
            break;

        case MODULE_FUNC_REPORT_3DP_PID:
            cb = CallbackAckReportPidTemp;
            break;

        case MODULE_FUNC_REPORT_NOZZLE_TYPE:
            cb = CallbackAckNozzleType;
            break;

        case MODULE_REPORT_EXTRUDER_INFO:
            cb = CallbackAckExtruderInfo;
            break;

        case MODULE_FUNC_REPORT_HOTEND_OFFSET:
            cb = CallbackAckReportHotendOffset;
            break;

        case MODULE_FUNC_REPORT_PROBE_SENSOR_COMPENSATION:
            cb = CallbackAckReportProbeSensorCompensation;
            break;

        case MODULE_FUNC_REPORT_RIGHT_EXTRUDER_POS:
            cb = CallbackAckReportRightExtruderPos;
            break;

        case MODULE_FUNC_GET_HW_VERSION:
            cb = CallbackAckReportHWVersion;
            break;

        default:
            cb = nullptr;
            break;
        }

        message_id[i] = canhost.RegisterFunction(function, cb);

        if(message_id[i] == MODULE_MESSAGE_ID_INVALID)
        {
            LOG_E("\tfailed to register function!\n");
            break;
        }
    }

    ret = canhost.BindMessageID(cmd, message_id);
    if(ret != E_SUCCESS)
    {
        LOG_E("\tfailed to bind message id!\n");
        return ret;
    }
    // enable all extruders
    E_ENABLE_ON = 1;
    IOInit();

    mac_index_ = mac_index;
    // todo: fix for extra extruders in this function
    UpdateEAxisStepsPerUnit(MODULE_TOOLHEAD_TRIPLE);
    // set toolhead in ModuleBase class
    SetToolhead(MODULE_TOOLHEAD_TRIPLE);
    // set hotend max temp
    UpdateHotendMaxTemp(300, 0);
    UpdateHotendMaxTemp(300, 1);
    // extra extruder, don't care right now
    UpdateHotendMaxTemp(300, 2);

    printer1 = this;  // set global variable to this instance, so we can access it from the marlin code

    // todo: add support for extra extruder
    GetHWVersion();

    // todo: this functions not supported on the extra extruder controller
    ModuleCtrlProbeStateSync();
    ModuleCtrlPidSync();
    ModuleCtrlFilamentStateSync();
    ModuleCtrlHotendOffsetSync();
    ModuleCtrlRightExtruderPosSync();

    CheckLevelingData();

    LOG_I("toolhead triple ready!\n");

    return ret;
}

ErrCode ToolHeadTriple::ToolChange(uint8_t new_extruder, bool use_compensation)
{
    // wtf is this?
    volatile int32_t x_diff_scaled, y_diff_scaled, z_diff_scaled;
    // wtf is this?
    volatile float x_diff, y_diff, z_diff;
    // wtf is this?
    volatile float hotend_offset_tmp[XYZ][HOTENDS] {{0}};
    // wtf is this?
    volatile float z_raise = 0;

    float             pre_position[X_TO_E];
    volatile uint32_t old_extruder;

    // prepare for switching tool
    // Block until all buffered steps are executed / cleaned
    Planner::synchronize();
    const bool leveling_was_active = Planner::leveling_active;

    if(new_extruder >= EXTRUDERS)
    {
        LOG_E("Illegal ToolChange! new_extruder >= EXTRUDERS\n");
        return E_PARAM;  // request illegal
    }

    if(!all_axes_homed())
    {
        LOG_I("need go home before changing tool\n");
        return E_FAILURE;
    }

    if(new_extruder != active_extruder)
    {
        // first, store current offsets,
        // but what is hotend offset?
        LOOP_XYZ(i)
        {
            HOTEND_LOOP()
            {
                hotend_offset_tmp[i][e] = hotend_offset[i][e];
            }
        }

        // not going to happen if it's dual-extruder head
        // but what if it's a triple extruder head?
        if(!use_compensation)
        {
            hotend_offset_tmp[Z_AXIS][1] = 0;
        }

        Planner::synchronize();  // why?
        // store current position
        // because we don't want program to mess up the position
        taskENTER_CRITICAL();
        LOOP_X_TO_EN(i) backup_current_position[i] = current_position[i];
        backup_position_valid                      = true;
        taskEXIT_CRITICAL();

        // disable leveling
        // we want to adjust it according to the offsets
        // from different toolhead
        set_bed_leveling_enabled(false);
        LOG_I(
            "\norigin pos: %.3f, %.3f, %.3f\n changing started \n",
            current_position[X_AXIS],
            current_position[Y_AXIS],
            current_position[Z_AXIS]
        );

        // calculate the difference between the current position
        // preparing to switch toolhead
        z_raise = current_position[Z_AXIS] + toolchange_settings.z_raise;
        NOMORE(z_raise, soft_endstop[Z_AXIS].max);
        z_raise = z_raise - current_position[Z_AXIS];
        // moving to target position
        LOG_I(
            "raise: %.3f, endstop max: %.3f, z offset: %.3f\n",
            z_raise,
            soft_endstop[Z_AXIS].max,
            hotend_offset_tmp[Z_AXIS][1]
        );
        do_blocking_move_to_z(current_position[Z_AXIS] + z_raise, 30);
        LOG_I(
            "raised pos: %.3f, %.3f, %.3f\n",
            current_position[X_AXIS],
            current_position[Y_AXIS],
            current_position[Z_AXIS]
        );

        // remove live z offset of old extruder after raise Z, cause Z will fall in un-applying live offset
        levelservice.UnapplyLiveZOffset(active_extruder);
        // to avoid power-loss, we record the new extruder after unapply z offset!
        old_extruder    = active_extruder;
        active_extruder = new_extruder;
        actual_extruder = new_extruder;

        // set_destination_from_current();
        COPY(pre_position, current_position);

        if(old_extruder == EXTRUDER_LEFT && new_extruder == EXTRUDER_RIGHT)
        {
            // left-->right
            // make sure there is enough space in the left for the moving
            if(current_position[X_AXIS] < X_MIN_POS + hotend_offset_tmp[X_AXIS][1])
            {
                do_blocking_move_to_xy(X_MIN_POS + hotend_offset_tmp[X_AXIS][1], current_position[Y_AXIS], 50);
            }
        }
        else if(old_extruder == EXTRUDER_RIGHT && new_extruder == EXTRUDER_LEFT)
        {
            // right -> left
            // make sure there is enough space in right for the moving
            if(current_position[X_AXIS] > X_MAX_POS - hotend_offset_tmp[X_AXIS][1])
            {
                do_blocking_move_to_xy(X_MAX_POS - hotend_offset_tmp[X_AXIS][1], current_position[Y_AXIS], 50);
            }
        }
        else
        {
            // todo: support for triple extruder
            // a possible solution is to split the motion into 2 separate moves
        }

        // these offsets are relative to toolhead 0, or say extruder 0
        update_software_endstops(X_AXIS, old_extruder, new_extruder);
        update_software_endstops(Y_AXIS, old_extruder, new_extruder);
        update_software_endstops(Z_AXIS, old_extruder, new_extruder);

        // this is the reference position for the all extruders,
        // so we don't need to consider the offsets
        if(new_extruder == 0)
        {
            ModuleCtrlToolChange(new_extruder);
        }

        x_diff = hotend_offset_tmp[X_AXIS][new_extruder] - hotend_offset_tmp[X_AXIS][old_extruder];
        y_diff = hotend_offset_tmp[Y_AXIS][new_extruder] - hotend_offset_tmp[Y_AXIS][old_extruder];
        z_diff = hotend_offset_tmp[Z_AXIS][new_extruder] - hotend_offset_tmp[Z_AXIS][old_extruder];
        // wtf?
        x_diff_scaled = static_cast<int>(x_diff * Planner::settings.axis_steps_per_mm[X_AXIS]);
        y_diff_scaled = static_cast<int>(y_diff * Planner::settings.axis_steps_per_mm[Y_AXIS]);
        z_diff_scaled = static_cast<int>(z_diff * Planner::settings.axis_steps_per_mm[Z_AXIS]);
        x_diff        = static_cast<float>(x_diff_scaled) / Planner::settings.axis_steps_per_mm[X_AXIS];
        y_diff        = static_cast<float>(y_diff_scaled) / Planner::settings.axis_steps_per_mm[Y_AXIS];
        z_diff        = static_cast<float>(z_diff_scaled) / Planner::settings.axis_steps_per_mm[Z_AXIS];

        // set planner position
        current_position[X_AXIS] += x_diff;
        current_position[Y_AXIS] += y_diff;
        current_position[Z_AXIS] += z_diff;
        LOG_I(
            "offset pos: %.3f, %.3f, %.3f\n",
            current_position[X_AXIS],
            current_position[Y_AXIS],
            current_position[Z_AXIS]
        );
        // and move to target
        sync_plan_position();

        apply_motion_limits(pre_position);
        do_blocking_move_to(pre_position);

        // 1
        if(new_extruder == EXTRUDER_RIGHT)
        {
            ModuleCtrlToolChange(new_extruder);
        }

        if(new_extruder == EXTRUDER_EXTRA)
        {
            ModuleCtrlToolChange(new_extruder);
        }

        // here we should apply live z offset of new extruder!
        levelservice.ApplyLiveZOffset(active_extruder);
        do_blocking_move_to_z(current_position[Z_AXIS] - z_raise, 30);
        LOG_I("changing finished, going back\n");
        LOG_I(
            "descent pos: %.3f, %.3f, %.3f\n\n",
            current_position[X_AXIS],
            current_position[Y_AXIS],
            current_position[Z_AXIS]
        );

        // after switch extruder, select relative OPTOCOUPLER
        SelectProbeSensor((probe_sensor_t)(PROBE_SENSOR_LEFT_OPTOCOUPLER + new_extruder));
    }

    // enable leveling again
    set_bed_leveling_enabled(leveling_was_active);
    taskENTER_CRITICAL();
    backup_position_valid = false;
    taskEXIT_CRITICAL();

    return E_SUCCESS;
}

void ToolHeadTriple::ReportProbeState(const uint8_t *state)
{
    // 1 bit indicates one sensor
    // todo: but extra extruder doesn't have a probe sensor
    for(auto i = 0; i < EXTRUDERS; i++)
    {
        if(state[i])
            probe_state_ |= (1 << i);
        else
            probe_state_ &= ~(1 << i);
    }
}

void ToolHeadTriple::ReportFilamentState(uint8_t *state)
{
    if (!state[0])
        filament_state_ |= 0x01;
    else
        filament_state_ &= ~0x01;

    if (!state[1])
        filament_state_ |= 0x02;
    else
        filament_state_ &= ~0x02;

    // extra extruder
    if (!state[2])
        filament_state_ |= 0x04;
    else
        filament_state_ &= ~0x04;
}
