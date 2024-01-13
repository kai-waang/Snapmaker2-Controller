//
// Created by Kai Wang on 23-11-28.
//

#include "toolhead_triple.h"
#include "../common/config.h"
#include "common/debug.h"

// marlin headers
#include "src/core/macros.h"
#include "src/core/boards.h"
#include "Configuration.h"
#include "src/pins/pins.h"
#include "src/inc/MarlinConfig.h"
#include HAL_PATH(src/HAL, HAL.h)
#include "../../../Marlin/src/module/temperature.h"
#include "../../../Marlin/src/feature/bedlevel/bedlevel.h"
#include "../../../Marlin/src/module/tool_change.h"

ToolHeadTriple printer_triple(MODULE_DEVICE_ID_TRIPLE);

static void CallbackAckProbeState(CanStdDataFrame_t &cmd) {
    printer_triple.ReportProbeState(cmd.data);
}

static void CallbackAckNozzleTemp(CanStdDataFrame_t &cmd) {
    if (cmd.id.bits.length < 8)
        return;

    printer_triple.ReportTemperature(cmd.data);
}

static void CallbackAckReportPidTemp(CanStdDataFrame_t &cmd) {
    printer_triple.ReportPID(cmd.data);
}

static void CallbackAckFilamentState(CanStdDataFrame_t &cmd) {
    printer_triple.ReportFilamentState(cmd.data);
}

static void CallbackAckNozzleType(CanStdDataFrame_t &cmd) {
    printer_triple.ReportHotendType(cmd.data);
}

static void CallbackAckExtruderInfo(CanStdDataFrame_t &cmd) {
    printer_triple.ReportExtruderInfo(cmd.data);
}

static void CallbackAckReportHotendOffset(CanStdDataFrame_t &cmd) {
    printer_triple.ReportHotendOffset(cmd.data);
}

static void CallbackAckReportProbeSensorCompensation(CanStdDataFrame_t &cmd) {
    printer_triple.ReportProbeSensorCompensation(cmd.data);
}

static void CallbackAckReportRightExtruderPos(CanStdDataFrame_t &cmd) {
    printer_triple.ReportRightExtruderPos(cmd.data);
}

static void CallbackAckReportHWVersion(CanStdDataFrame_t &cmd) {
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
    CanStdCmdCallback_t cb = NULL;

    ret = ModuleBase::InitModule8p(mac, E0_DIR_PIN, mac_index);
    if(ret != E_SUCCESS)
        return ret;

    LOG_I("\t Got toolhead triple module\n");

    cmd.mac    = mac;
    cmd.data   = func_buffer;
    cmd.length = 1;

    cmd.data[MODULE_EXT_CMD_INDEX_ID]   = MODULE_EXT_CMD_GET_FUNCID_REQ;

    // try to get function ids from module
    if (canhost.SendExtCmdSync(cmd, 500, 2) != E_SUCCESS)
        return E_FAILURE;

    function.channel   = mac.bits.channel;
    function.mac_index = mac_index;
    function.sub_index = 0;
    function.priority  = MODULE_FUNC_PRIORITY_DEFAULT;

    using cb_func_type = void (*)(CanStdDataFrame &cmd);

    for (int i = 0; i < cmd.data[MODULE_EXT_CMD_INDEX_DATA]; i++)
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
            cb = NULL;
            break;
        }
    }

    return ret;
}
