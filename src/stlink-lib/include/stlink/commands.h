/*
 * File: commands.h
 *
 * stlink commands
 */

#ifndef COMMANDS_H
#define COMMANDS_H

enum stlink_commands {
    STLINK_GET_VERSION                   = 0xF1,
    STLINK_DEBUG_COMMAND                 = 0xF2,
    STLINK_DFU_COMMAND                   = 0xF3,
    STLINK_GET_CURRENT_MODE              = 0xF5,
    STLINK_GET_TARGET_VOLTAGE            = 0xF7,
    STLINK_GET_VERSION_APIV3             = 0xFB
};

enum stlink_debug_commands {
    STLINK_DEBUG_ENTER_JTAG_RESET        = 0x00,
    STLINK_DEBUG_GETSTATUS               = 0x01,
    STLINK_DEBUG_FORCEDEBUG              = 0x02,
    STLINK_DEBUG_APIV1_RESETSYS          = 0x03,
    STLINK_DEBUG_APIV1_READALLREGS       = 0x04,
    STLINK_DEBUG_APIV1_READREG           = 0x05,
    STLINK_DEBUG_APIV1_WRITEREG          = 0x06,
    STLINK_DEBUG_READMEM_32BIT           = 0x07,
    STLINK_DEBUG_WRITEMEM_32BIT          = 0x08,
    STLINK_DEBUG_RUNCORE                 = 0x09,
    STLINK_DEBUG_STEPCORE                = 0x0a,
    STLINK_DEBUG_APIV1_SETFP             = 0x0b,
    STLINK_DEBUG_WRITEMEM_8BIT           = 0x0d,
    STLINK_DEBUG_APIV1_CLEARFP           = 0x0e,
    STLINK_DEBUG_APIV1_WRITEDEBUGREG     = 0x0f,
    STLINK_DEBUG_APIV1_ENTER             = 0x20,
    STLINK_DEBUG_EXIT                    = 0x21,
    STLINK_DEBUG_READCOREID              = 0x22,
    STLINK_DEBUG_APIV2_ENTER             = 0x30,
    STLINK_DEBUG_APIV2_READ_IDCODES      = 0x31,
    STLINK_DEBUG_APIV2_RESETSYS          = 0x32,
    STLINK_DEBUG_APIV2_READREG           = 0x33,
    STLINK_DEBUG_APIV2_WRITEREG          = 0x34,
    STLINK_DEBUG_APIV2_WRITEDEBUGREG     = 0x35,
    STLINK_DEBUG_APIV2_READDEBUGREG      = 0x36,
    STLINK_DEBUG_APIV2_READALLREGS       = 0x3A,
    STLINK_DEBUG_APIV2_GETLASTRWSTATUS   = 0x3B,
    STLINK_DEBUG_APIV2_DRIVE_NRST        = 0x3C,
    STLINK_DEBUG_APIV2_GETLASTRWSTATUS2  = 0x3E,
    STLINK_DEBUG_APIV2_START_TRACE_RX    = 0x40,
    STLINK_DEBUG_APIV2_STOP_TRACE_RX     = 0x41,
    STLINK_DEBUG_APIV2_GET_TRACE_NB      = 0x42,
    STLINK_DEBUG_APIV2_SWD_SET_FREQ      = 0x43,
    STLINK_DEBUG_APIV3_SET_COM_FREQ      = 0x61,
    STLINK_DEBUG_APIV3_GET_COM_FREQ      = 0x62,
    STLINK_DEBUG_ENTER_SWD               = 0xa3,
    STLINK_DEBUG_ENTER_JTAG_NO_RESET     = 0xa4,
};

enum stlink_dfu_commands {
    STLINK_DFU_EXIT                      = 0x07
};

#endif // COMMANDS_H
