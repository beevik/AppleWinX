/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

/****************************************************************************
*
*   Constants
*
***/

constexpr DWORD MF_80STORE    = 0x00000001;
constexpr DWORD MF_ALTZP      = 0x00000002;
constexpr DWORD MF_AUXREAD    = 0x00000004;
constexpr DWORD MF_AUXWRITE   = 0x00000008;
constexpr DWORD MF_BANK2      = 0x00000010;
constexpr DWORD MF_HIGHRAM    = 0x00000020;
constexpr DWORD MF_HIRES      = 0x00000040;
constexpr DWORD MF_PAGE2      = 0x00000080;
constexpr DWORD MF_SLOTC3ROM  = 0x00000100;
constexpr DWORD MF_SLOTCXROM  = 0x00000200;
constexpr DWORD MF_WRITERAM   = 0x00000400;
constexpr DWORD MF_IMAGEMASK  = 0x000003F7;

#define  SW_80STORE()   ((s_memMode & MF_80STORE)   != 0)
#define  SW_ALTZP()     ((s_memMode & MF_ALTZP)     != 0)
#define  SW_AUXREAD()   ((s_memMode & MF_AUXREAD)   != 0)
#define  SW_AUXWRITE()  ((s_memMode & MF_AUXWRITE)  != 0)
#define  SW_BANK2()     ((s_memMode & MF_BANK2)     != 0)
#define  SW_HIGHRAM()   ((s_memMode & MF_HIGHRAM)   != 0)
#define  SW_HIRES()     ((s_memMode & MF_HIRES)     != 0)
#define  SW_PAGE2()     ((s_memMode & MF_PAGE2)     != 0)
#define  SW_SLOTC3ROM() ((s_memMode & MF_SLOTC3ROM) != 0)
#define  SW_SLOTCXROM() ((s_memMode & MF_SLOTCXROM) != 0)
#define  SW_WRITERAM()  ((s_memMode & MF_WRITERAM)  != 0)


/****************************************************************************
*
*   Variables
*
***/

LPBYTE  g_mem      = NULL;
LPBYTE  g_memDirty = NULL;
LPBYTE  g_memWrite[0x100];

static bool   s_lastWriteRam = false;
static LPBYTE s_memAux       = NULL;
static LPBYTE s_memMain      = NULL;
static DWORD  s_memMode      = MF_BANK2 | MF_SLOTCXROM | MF_WRITERAM;
static LPBYTE s_memRom       = NULL;
static LPBYTE s_memShadow[0x100];

BYTE NullIo(WORD pc, BYTE address, BYTE write, BYTE value);

FIo ioRead[0x100] = {
    KeybReadData,       // $C000
    KeybReadData,       // $C001
    KeybReadData,       // $C002
    KeybReadData,       // $C003
    KeybReadData,       // $C004
    KeybReadData,       // $C005
    KeybReadData,       // $C006
    KeybReadData,       // $C007
    KeybReadData,       // $C008
    KeybReadData,       // $C009
    KeybReadData,       // $C00A
    KeybReadData,       // $C00B
    KeybReadData,       // $C00C
    KeybReadData,       // $C00D
    KeybReadData,       // $C00E
    KeybReadData,       // $C00F
    KeybReadFlag,       // $C010
    MemGetPaging,       // $C011
    MemGetPaging,       // $C012
    MemGetPaging,       // $C013
    MemGetPaging,       // $C014
    MemGetPaging,       // $C015
    MemGetPaging,       // $C016
    MemGetPaging,       // $C017
    MemGetPaging,       // $C018
    VideoCheckVbl,      // $C019
    VideoCheckMode,     // $C01A
    VideoCheckMode,     // $C01B
    MemGetPaging,       // $C01C
    MemGetPaging,       // $C01D
    VideoCheckMode,     // $C01E
    VideoCheckMode,     // $C01F
    NullIo,             // $C020
    NullIo,             // $C021
    NullIo,             // $C022
    NullIo,             // $C023
    NullIo,             // $C024
    NullIo,             // $C025
    NullIo,             // $C026
    NullIo,             // $C027
    NullIo,             // $C028
    NullIo,             // $C029
    NullIo,             // $C02A
    NullIo,             // $C02B
    NullIo,             // $C02C
    NullIo,             // $C02D
    NullIo,             // $C02E
    NullIo,             // $C02F
    SpkrToggle,         // $C030
    SpkrToggle,         // $C031
    SpkrToggle,         // $C032
    SpkrToggle,         // $C033
    SpkrToggle,         // $C034
    SpkrToggle,         // $C035
    SpkrToggle,         // $C036
    SpkrToggle,         // $C037
    SpkrToggle,         // $C038
    SpkrToggle,         // $C039
    SpkrToggle,         // $C03A
    SpkrToggle,         // $C03B
    SpkrToggle,         // $C03C
    SpkrToggle,         // $C03D
    SpkrToggle,         // $C03E
    SpkrToggle,         // $C03F
    NullIo,             // $C040
    NullIo,             // $C041
    NullIo,             // $C042
    NullIo,             // $C043
    NullIo,             // $C044
    NullIo,             // $C045
    NullIo,             // $C046
    NullIo,             // $C047
    NullIo,             // $C048
    NullIo,             // $C049
    NullIo,             // $C04A
    NullIo,             // $C04B
    NullIo,             // $C04C
    NullIo,             // $C04D
    NullIo,             // $C04E
    NullIo,             // $C04F
    VideoSetMode,       // $C050
    VideoSetMode,       // $C051
    VideoSetMode,       // $C052
    VideoSetMode,       // $C053
    MemSetPaging,       // $C054
    MemSetPaging,       // $C055
    MemSetPaging,       // $C056
    MemSetPaging,       // $C057
    NullIo,             // $C058
    NullIo,             // $C059
    NullIo,             // $C05A
    NullIo,             // $C05B
    NullIo,             // $C05C
    NullIo,             // $C05D
    VideoSetMode,       // $C05E
    VideoSetMode,       // $C05F
    NullIo,             // $C060
    JoyReadButton,      // $C061
    JoyReadButton,      // $C062
    JoyReadButton,      // $C063
    JoyReadPosition,    // $C064
    JoyReadPosition,    // $C065
    NullIo,             // $C066
    NullIo,             // $C067
    NullIo,             // $C068
    NullIo,             // $C069
    NullIo,             // $C06A
    NullIo,             // $C06B
    NullIo,             // $C06C
    NullIo,             // $C06D
    NullIo,             // $C06E
    NullIo,             // $C06F
    JoyResetPosition,   // $C070
    NullIo,             // $C071
    NullIo,             // $C072
    NullIo,             // $C073
    NullIo,             // $C074
    NullIo,             // $C075
    NullIo,             // $C076
    NullIo,             // $C077
    NullIo,             // $C078
    NullIo,             // $C079
    NullIo,             // $C07A
    NullIo,             // $C07B
    NullIo,             // $C07C
    NullIo,             // $C07D
    NullIo,             // $C07E
    VideoCheckMode,     // $C07F
    MemSetPaging,       // $C080
    MemSetPaging,       // $C081
    MemSetPaging,       // $C082
    MemSetPaging,       // $C083
    MemSetPaging,       // $C084
    MemSetPaging,       // $C085
    MemSetPaging,       // $C086
    MemSetPaging,       // $C087
    MemSetPaging,       // $C088
    MemSetPaging,       // $C089
    MemSetPaging,       // $C08A
    MemSetPaging,       // $C08B
    MemSetPaging,       // $C08C
    MemSetPaging,       // $C08D
    MemSetPaging,       // $C08E
    MemSetPaging,       // $C08F
    NullIo,             // $C090
    NullIo,             // $C091
    NullIo,             // $C092
    NullIo,             // $C093
    NullIo,             // $C094
    NullIo,             // $C095
    NullIo,             // $C096
    NullIo,             // $C097
    NullIo,             // $C098
    NullIo,             // $C099
    NullIo,             // $C09A
    NullIo,             // $C09B
    NullIo,             // $C09C
    NullIo,             // $C09D
    NullIo,             // $C09E
    NullIo,             // $C09F
    NullIo,             // $C0A0
    CommDipSw,          // $C0A1
    CommDipSw,          // $C0A2
    NullIo,             // $C0A3
    NullIo,             // $C0A4
    NullIo,             // $C0A5
    NullIo,             // $C0A6
    NullIo,             // $C0A7
    CommReceive,        // $C0A8
    CommStatus,         // $C0A9
    CommCommand,        // $C0AA
    CommControl,        // $C0AB
    NullIo,             // $C0AC
    NullIo,             // $C0AD
    NullIo,             // $C0AE
    NullIo,             // $C0AF
    NullIo,             // $C0B0
    NullIo,             // $C0B1
    NullIo,             // $C0B2
    NullIo,             // $C0B3
    NullIo,             // $C0B4
    NullIo,             // $C0B5
    NullIo,             // $C0B6
    NullIo,             // $C0B7
    NullIo,             // $C0B8
    NullIo,             // $C0B9
    NullIo,             // $C0BA
    NullIo,             // $C0BB
    NullIo,             // $C0BC
    NullIo,             // $C0BD
    NullIo,             // $C0BE
    NullIo,             // $C0BF
    NullIo,             // $C0C0
    NullIo,             // $C0C1
    NullIo,             // $C0C2
    NullIo,             // $C0C3
    NullIo,             // $C0C4
    NullIo,             // $C0C5
    NullIo,             // $C0C6
    NullIo,             // $C0C7
    NullIo,             // $C0C8
    NullIo,             // $C0C9
    NullIo,             // $C0CA
    NullIo,             // $C0CB
    NullIo,             // $C0CC
    NullIo,             // $C0CD
    NullIo,             // $C0CE
    NullIo,             // $C0CF
    NullIo,             // $C0D0
    NullIo,             // $C0D1
    NullIo,             // $C0D2
    NullIo,             // $C0D3
    NullIo,             // $C0D4
    NullIo,             // $C0D5
    NullIo,             // $C0D6
    NullIo,             // $C0D7
    NullIo,             // $C0D8
    NullIo,             // $C0D9
    NullIo,             // $C0DA
    NullIo,             // $C0DB
    NullIo,             // $C0DC
    NullIo,             // $C0DD
    NullIo,             // $C0DE
    NullIo,             // $C0DF
    DiskControlStepper, // $C0E0
    DiskControlStepper, // $C0E1
    DiskControlStepper, // $C0E2
    DiskControlStepper, // $C0E3
    DiskControlStepper, // $C0E4
    DiskControlStepper, // $C0E5
    DiskControlStepper, // $C0E6
    DiskControlStepper, // $C0E7
    DiskControlMotor,   // $C0E8
    DiskControlMotor,   // $C0E9
    DiskEnable,         // $C0EA
    DiskEnable,         // $C0EB
    DiskReadWrite,      // $C0EC
    DiskSetLatchValue,  // $C0ED
    DiskSetReadMode,    // $C0EE
    DiskSetWriteMode,   // $C0EF
    NullIo,             // $C0F0
    NullIo,             // $C0F1
    NullIo,             // $C0F2
    NullIo,             // $C0F3
    NullIo,             // $C0F4
    NullIo,             // $C0F5
    NullIo,             // $C0F6
    NullIo,             // $C0F7
    NullIo,             // $C0F8
    NullIo,             // $C0F9
    NullIo,             // $C0FA
    NullIo,             // $C0FB
    NullIo,             // $C0FC
    NullIo,             // $C0FD
    NullIo,             // $C0FE
    NullIo              // $C0FF
};

FIo ioWrite[0x100] = {
    MemSetPaging,       // $C000
    MemSetPaging,       // $C001
    MemSetPaging,       // $C002
    MemSetPaging,       // $C003
    MemSetPaging,       // $C004
    MemSetPaging,       // $C005
    MemSetPaging,       // $C006
    MemSetPaging,       // $C007
    MemSetPaging,       // $C008
    MemSetPaging,       // $C009
    MemSetPaging,       // $C00A
    MemSetPaging,       // $C00B
    VideoSetMode,       // $C00C
    VideoSetMode,       // $C00D
    VideoSetMode,       // $C00E
    VideoSetMode,       // $C00F
    KeybReadFlag,       // $C010
    KeybReadFlag,       // $C011
    KeybReadFlag,       // $C012
    KeybReadFlag,       // $C013
    KeybReadFlag,       // $C014
    KeybReadFlag,       // $C015
    KeybReadFlag,       // $C016
    KeybReadFlag,       // $C017
    KeybReadFlag,       // $C018
    KeybReadFlag,       // $C019
    KeybReadFlag,       // $C01A
    KeybReadFlag,       // $C01B
    KeybReadFlag,       // $C01C
    KeybReadFlag,       // $C01D
    KeybReadFlag,       // $C01E
    KeybReadFlag,       // $C01F
    NullIo,             // $C020
    NullIo,             // $C021
    NullIo,             // $C022
    NullIo,             // $C023
    NullIo,             // $C024
    NullIo,             // $C025
    NullIo,             // $C026
    NullIo,             // $C027
    NullIo,             // $C028
    NullIo,             // $C029
    NullIo,             // $C02A
    NullIo,             // $C02B
    NullIo,             // $C02C
    NullIo,             // $C02D
    NullIo,             // $C02E
    NullIo,             // $C02F
    SpkrToggle,         // $C030
    SpkrToggle,         // $C031
    SpkrToggle,         // $C032
    SpkrToggle,         // $C033
    SpkrToggle,         // $C034
    SpkrToggle,         // $C035
    SpkrToggle,         // $C036
    SpkrToggle,         // $C037
    SpkrToggle,         // $C038
    SpkrToggle,         // $C039
    SpkrToggle,         // $C03A
    SpkrToggle,         // $C03B
    SpkrToggle,         // $C03C
    SpkrToggle,         // $C03D
    SpkrToggle,         // $C03E
    SpkrToggle,         // $C03F
    NullIo,             // $C040
    NullIo,             // $C041
    NullIo,             // $C042
    NullIo,             // $C043
    NullIo,             // $C044
    NullIo,             // $C045
    NullIo,             // $C046
    NullIo,             // $C047
    NullIo,             // $C048
    NullIo,             // $C049
    NullIo,             // $C04A
    NullIo,             // $C04B
    NullIo,             // $C04C
    NullIo,             // $C04D
    NullIo,             // $C04E
    NullIo,             // $C04F
    VideoSetMode,       // $C050
    VideoSetMode,       // $C051
    VideoSetMode,       // $C052
    VideoSetMode,       // $C053
    MemSetPaging,       // $C054
    MemSetPaging,       // $C055
    MemSetPaging,       // $C056
    MemSetPaging,       // $C057
    NullIo,             // $C058
    NullIo,             // $C059
    NullIo,             // $C05A
    NullIo,             // $C05B
    NullIo,             // $C05C
    NullIo,             // $C05D
    VideoSetMode,       // $C05E
    VideoSetMode,       // $C05F
    NullIo,             // $C060
    NullIo,             // $C061
    NullIo,             // $C062
    NullIo,             // $C063
    NullIo,             // $C064
    NullIo,             // $C065
    NullIo,             // $C066
    NullIo,             // $C067
    NullIo,             // $C068
    NullIo,             // $C069
    NullIo,             // $C06A
    NullIo,             // $C06B
    NullIo,             // $C06C
    NullIo,             // $C06D
    NullIo,             // $C06E
    NullIo,             // $C06F
    JoyResetPosition,   // $C070
    NullIo,             // $C071
    NullIo,             // $C072
    NullIo,             // $C073
    NullIo,             // $C074
    NullIo,             // $C075
    NullIo,             // $C076
    NullIo,             // $C077
    NullIo,             // $C078
    NullIo,             // $C079
    NullIo,             // $C07A
    NullIo,             // $C07B
    NullIo,             // $C07C
    NullIo,             // $C07D
    NullIo,             // $C07E
    NullIo,             // $C07F
    MemSetPaging,       // $C080
    MemSetPaging,       // $C081
    MemSetPaging,       // $C082
    MemSetPaging,       // $C083
    MemSetPaging,       // $C084
    MemSetPaging,       // $C085
    MemSetPaging,       // $C086
    MemSetPaging,       // $C087
    MemSetPaging,       // $C088
    MemSetPaging,       // $C089
    MemSetPaging,       // $C08A
    MemSetPaging,       // $C08B
    MemSetPaging,       // $C08C
    MemSetPaging,       // $C08D
    MemSetPaging,       // $C08E
    MemSetPaging,       // $C08F
    NullIo,             // $C090
    NullIo,             // $C091
    NullIo,             // $C092
    NullIo,             // $C093
    NullIo,             // $C094
    NullIo,             // $C095
    NullIo,             // $C096
    NullIo,             // $C097
    NullIo,             // $C098
    NullIo,             // $C099
    NullIo,             // $C09A
    NullIo,             // $C09B
    NullIo,             // $C09C
    NullIo,             // $C09D
    NullIo,             // $C09E
    NullIo,             // $C09F
    NullIo,             // $C0A0
    NullIo,             // $C0A1
    NullIo,             // $C0A2
    NullIo,             // $C0A3
    NullIo,             // $C0A4
    NullIo,             // $C0A5
    NullIo,             // $C0A6
    NullIo,             // $C0A7
    CommTransmit,       // $C0A8
    CommStatus,         // $C0A9
    CommCommand,        // $C0AA
    CommControl,        // $C0AB
    NullIo,             // $C0AC
    NullIo,             // $C0AD
    NullIo,             // $C0AE
    NullIo,             // $C0AF
    NullIo,             // $C0B0
    NullIo,             // $C0B1
    NullIo,             // $C0B2
    NullIo,             // $C0B3
    NullIo,             // $C0B4
    NullIo,             // $C0B5
    NullIo,             // $C0B6
    NullIo,             // $C0B7
    NullIo,             // $C0B8
    NullIo,             // $C0B9
    NullIo,             // $C0BA
    NullIo,             // $C0BB
    NullIo,             // $C0BC
    NullIo,             // $C0BD
    NullIo,             // $C0BE
    NullIo,             // $C0BF
    NullIo,             // $C0C0
    NullIo,             // $C0C1
    NullIo,             // $C0C2
    NullIo,             // $C0C3
    NullIo,             // $C0C4
    NullIo,             // $C0C5
    NullIo,             // $C0C6
    NullIo,             // $C0C7
    NullIo,             // $C0C8
    NullIo,             // $C0C9
    NullIo,             // $C0CA
    NullIo,             // $C0CB
    NullIo,             // $C0CC
    NullIo,             // $C0CD
    NullIo,             // $C0CE
    NullIo,             // $C0CF
    NullIo,             // $C0D0
    NullIo,             // $C0D1
    NullIo,             // $C0D2
    NullIo,             // $C0D3
    NullIo,             // $C0D4
    NullIo,             // $C0D5
    NullIo,             // $C0D6
    NullIo,             // $C0D7
    NullIo,             // $C0D8
    NullIo,             // $C0D9
    NullIo,             // $C0DA
    NullIo,             // $C0DB
    NullIo,             // $C0DC
    NullIo,             // $C0DD
    NullIo,             // $C0DE
    NullIo,             // $C0DF
    DiskControlStepper, // $C0E0
    DiskControlStepper, // $C0E1
    DiskControlStepper, // $C0E2
    DiskControlStepper, // $C0E3
    DiskControlStepper, // $C0E4
    DiskControlStepper, // $C0E5
    DiskControlStepper, // $C0E6
    DiskControlStepper, // $C0E7
    DiskControlMotor,   // $C0E8
    DiskControlMotor,   // $C0E9
    DiskEnable,         // $C0EA
    DiskEnable,         // $C0EB
    DiskReadWrite,      // $C0EC
    DiskSetLatchValue,  // $C0ED
    DiskSetReadMode,    // $C0EE
    DiskSetWriteMode,   // $C0EF
    NullIo,             // $C0F0
    NullIo,             // $C0F1
    NullIo,             // $C0F2
    NullIo,             // $C0F3
    NullIo,             // $C0F4
    NullIo,             // $C0F5
    NullIo,             // $C0F6
    NullIo,             // $C0F7
    NullIo,             // $C0F8
    NullIo,             // $C0F9
    NullIo,             // $C0FA
    NullIo,             // $C0FB
    NullIo,             // $C0FC
    NullIo,             // $C0FD
    NullIo,             // $C0FE
    NullIo              // $C0FF
};


/****************************************************************************
*
*   Local functions
*
***/

static void UpdatePaging(bool initialize);

//===========================================================================
static void BackMainImage() {
    for (int page = 0; page < 256; page++) {
        if (s_memShadow[page] && ((g_memDirty[page] & 1) || (page <= 1)))
            memcpy(s_memShadow[page], g_mem + (page << 8), 256);
        g_memDirty[page] &= ~1;
    }
}

//===========================================================================
static void InitializePaging() {
    BackMainImage();
    UpdatePaging(true);
}

//===========================================================================
static BYTE NullIo(WORD pc, BYTE address, BYTE write, BYTE value) {
    if ((address & 0xF0) == 0xA0) {
        static const BYTE retval[16] = {
            0x58, 0xFC, 0x5B, 0xFF, 0x58, 0xFC, 0x5B, 0xFF,
            0x0B, 0x10, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF
        };
        return retval[address & 15];
    }
    else if ((address >= 0xB0) && (address <= 0xCF)) {
        BYTE r = (BYTE)(rand() & 0xFF);
        if (r >= 0x10)
            return 0xA0;
        else if (r >= 8)
            return (r > 0xC) ? 0xFF : 0x00;
        else
            return (address & 0xF7);
    }
    else if ((address & 0xF0) == 0xD0) {
        BYTE r = (BYTE)(rand() & 0xFF);
        if (r >= 0xC0)
            return 0xC0;
        else if (r >= 0x80)
            return 0x80;
        else if ((address == 0xD0) || (address == 0xDF))
            return 0;
        else if (r >= 0x40)
            return 0x40;
        else if (r >= 0x30)
            return 0x90;
        else
            return 0;
    }
    else
        return MemReturnRandomData(TRUE);
}

//===========================================================================
static void ResetPaging(bool initialize) {
    if (!initialize)
        InitializePaging();
    s_lastWriteRam = false;
    s_memMode = MF_BANK2 | MF_SLOTCXROM | MF_WRITERAM;
    UpdatePaging(initialize);
}

//===========================================================================
static void UpdatePaging(bool initialize) {
    LPBYTE oldShadow[256];
    memcpy(oldShadow, s_memShadow, ARRSIZE(oldShadow) * sizeof(LPBYTE));

    if (initialize) {
        for (int page = 0; page < 0xC0; page++)
            g_memWrite[page] = g_mem + (page << 8);
        for (int page = 0xC0; page < 0xD0; page++)
            g_memWrite[page] = NULL;
    }
    for (int page = 0; page < 2; page++) {
        s_memShadow[page] = SW_ALTZP()
            ? s_memAux  + (page << 8)
            : s_memMain + (page << 8);
    }
    for (int page = 2; page < 0xC0; page++) {
        s_memShadow[page] = SW_AUXREAD()
            ? s_memAux  + (page << 8)
            : s_memMain + (page << 8);
        g_memWrite[page] = (SW_AUXREAD() == SW_AUXWRITE())
            ? g_mem + (page << 8)
            : SW_AUXWRITE()
                ? s_memAux  + (page << 8)
                : s_memMain + (page << 8);
    }
    for (int page = 0xC0; page < 0xC3; page++) {
        s_memShadow[page] = SW_SLOTCXROM()
            ? s_memRom + (page << 8) - 0xC000
            : s_memRom + (page << 8) - 0xB000;
    }
    s_memShadow[0xC3] = (SW_SLOTC3ROM() && SW_SLOTCXROM())
        ? s_memRom + 0x0300
        : s_memRom + 0x1300;
    for (int page = 0xC4; page < 0xC8; page++) {
        s_memShadow[page] = SW_SLOTCXROM()
            ? s_memRom + (page << 8) - 0xC000
            : s_memRom + (page << 8) - 0xB000;
    }
    for (int page = 0xC8; page < 0xD0; page++)
        s_memShadow[page] = s_memRom + (page << 8) - 0xB000;
    for (int page = 0xD0; page < 0xE0; page++) {
        int bankOffset = SW_BANK2() ? 0 : 0x1000;
        s_memShadow[page] = SW_HIGHRAM()
            ? SW_ALTZP()
                ? s_memAux  + (page << 8) - bankOffset
                : s_memMain + (page << 8) - bankOffset
            : s_memRom + (page << 8) - 0xB000;
        g_memWrite[page] = SW_WRITERAM()
            ? SW_HIGHRAM()
                ? g_mem + (page << 8)
                : SW_ALTZP()
                    ? s_memAux  + (page << 8) - bankOffset
                    : s_memMain + (page << 8) - bankOffset
            : NULL;
    }
    for (int page = 0xE0; page < 0x100; page++) {
        s_memShadow[page] = SW_HIGHRAM()
            ? SW_ALTZP()
                ? s_memAux  + (page << 8)
                : s_memMain + (page << 8)
            : s_memRom + (page << 8) - 0xB000;
        g_memWrite[page] = SW_WRITERAM()
            ? SW_HIGHRAM()
                ? g_mem + (page << 8)
                : SW_ALTZP()
                    ? s_memAux  + (page << 8)
                    : s_memMain + (page << 8)
            : NULL;
    }

    if (SW_80STORE()) {
        for (int page = 4; page < 8; page++) {
            s_memShadow[page] = SW_PAGE2()
                ? s_memAux  + (page << 8)
                : s_memMain + (page << 8);
            g_memWrite[page] = g_mem + (page << 8);
        }
        if (SW_HIRES()) {
            for (int page = 0x20; page < 0x40; page++) {
                s_memShadow[page] = SW_PAGE2()
                    ? s_memAux  + (page << 8)
                    : s_memMain + (page << 8);
                g_memWrite[page] = g_mem + (page << 8);
            }
        }
    }

    // MOVE MEMORY BACK AND FORTH AS NECESSARY BETWEEN THE SHADOW AREAS AND
    // THE MAIN RAM IMAGE TO KEEP BOTH SETS OF MEMORY CONSISTENT WITH THE NEW
    // PAGING SHADOW TABLE
    for (int page = 0; page < 0x100; page++) {
        if (initialize || (oldShadow[page] != s_memShadow[page])) {
            if (!initialize && ((g_memDirty[page] & 1) || page <= 1)) {
                g_memDirty[page] &= ~1;
                memcpy(oldShadow[page], g_mem + (page << 8), 256);
            }
            memcpy(g_mem + (page << 8), s_memShadow[page], 256);
        }
    }
}


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
void MemDestroy() {
    delete[] g_mem;
    delete[] g_memDirty;
    delete[] s_memAux;
    delete[] s_memMain;
    delete[] s_memRom;
    g_mem      = NULL;
    g_memDirty = NULL;
    s_memAux   = NULL;
    s_memMain  = NULL;
    s_memRom   = NULL;
}

//===========================================================================
LPBYTE MemGetAuxPtr(WORD offset) {
    return (s_memShadow[(offset >> 8)] == (s_memAux + (offset & 0xFF00)))
        ? g_mem + offset
        : s_memAux + offset;
}

//===========================================================================
LPBYTE MemGetMainPtr(WORD offset) {
    return (s_memShadow[(offset >> 8)] == (s_memMain + (offset & 0xFF00)))
        ? g_mem + offset
        : s_memMain + offset;
}

//===========================================================================
BYTE MemGetPaging(WORD, BYTE address, BYTE, BYTE) {
    bool result;
    switch (address) {
        case 0x11: result = SW_BANK2();      break;
        case 0x12: result = SW_HIGHRAM();    break;
        case 0x13: result = SW_AUXREAD();    break;
        case 0x14: result = SW_AUXWRITE();   break;
        case 0x15: result = !SW_SLOTCXROM(); break;
        case 0x16: result = SW_ALTZP();      break;
        case 0x17: result = SW_SLOTC3ROM();  break;
        case 0x18: result = SW_80STORE();    break;
        case 0x1C: result = SW_PAGE2();      break;
        case 0x1D: result = SW_HIRES();      break;
        default:   result = false;           break;
    }
    return KeybGetKeycode() | (result ? 0x80 : 0);
}

//===========================================================================
void MemInitialize() {
    g_mem      = new BYTE[0x10000];
    s_memAux   = new BYTE[0x10000];
    s_memMain  = new BYTE[0x10000];
    s_memRom   = new BYTE[0x5000];
    g_memDirty = new BYTE[0x100];
    if (!g_mem || !s_memAux || !g_memDirty || !s_memMain || !s_memRom) {
        MessageBox(
            GetDesktopWindow(),
            "The emulator was unable to allocate the memory it "
            "requires.  Further execution is not possible.",
            EmulatorGetTitle(),
            MB_ICONSTOP | MB_SETFOREGROUND
        );
        ExitProcess(1);
    }
    memset(g_mem, 0, 0x10000);
    memset(g_memDirty, 0, 0x100);

    int size;
    const char * name = EmulatorGetAppleType() == APPLE_TYPE_IIE ? "APPLE2E_ROM" : "APPLE2_ROM";
    const void * rom  = ResourceLoad(name, "ROM", &size);
    if (!rom) {
        MessageBox(
            GetDesktopWindow(),
            "The emulator was unable to load the ROM firmware"
            "into memory.  Further execution is not possible.",
            EmulatorGetTitle(),
            MB_ICONSTOP | MB_SETFOREGROUND
        );
        ExitProcess(1);
    }
    if (size != 0x5000) {
        MessageBox(
            GetDesktopWindow(),
            "Firmware ROM file was not the correct size.",
            EmulatorGetTitle(),
            MB_ICONSTOP | MB_SETFOREGROUND
        );
        ExitProcess(1);
    }
    memcpy(s_memRom, rom, size);
    ResourceFree(rom);

    // Remove wait routine from the disk controller firmware.
    s_memRom[0x064C] = 0xA9;
    s_memRom[0x064D] = 0x00;
    s_memRom[0x064E] = 0xEA;

    MemReset();
}

//===========================================================================
void MemReset() {
    InitializePaging();

    memset(s_memShadow, 0, 256 * sizeof(LPBYTE));
    memset(g_memWrite, 0, 256 * sizeof(LPBYTE));
    memset(s_memAux, 0, 0x10000);
    memset(s_memMain, 0, 0x10000);

    CpuInitialize();
    ResetPaging(true);
    regs.pc = *(LPWORD)(g_mem + 0xFFFC);
}

//===========================================================================
void MemResetPaging() {
    ResetPaging(false);
}

//===========================================================================
BYTE MemReturnRandomData(BOOL highbit) {
    static const BYTE retval[16] = {
        0x00, 0x2D, 0x2D, 0x30, 0x30, 0x32, 0x32, 0x34,
        0x35, 0x39, 0x43, 0x43, 0x43, 0x60, 0x7F, 0x7F
    };

    BYTE r = (BYTE)(rand() & 0xFF);
    if (r <= 170)
        return 0x20 | (highbit ? 0x80 : 0);
    else
        return retval[r & 15] | (highbit ? 0x80 : 0);
}

//===========================================================================
BYTE MemSetPaging(WORD pc, BYTE address, BYTE write, BYTE value) {
    DWORD lastMemMode = s_memMode;

    if ((address >= 0x80) && (address <= 0x8F)) {
        bool writeRam = (address & 1) != 0;
        s_memMode &= ~(MF_BANK2 | MF_HIGHRAM | MF_WRITERAM);
        s_lastWriteRam = true; // note: because diags.do doesn't set switches twice!
        if (s_lastWriteRam && writeRam)
            s_memMode |= MF_WRITERAM;
        if (!(address & 8))
            s_memMode |= MF_BANK2;
        if (((address & 2) >> 1) == (address & 1))
            s_memMode |= MF_HIGHRAM;
        s_lastWriteRam = writeRam;
    }
    else if (EmulatorGetAppleType() == APPLE_TYPE_IIE) {
        switch (address) {
            case 0x00: s_memMode &= ~MF_80STORE;    break;
            case 0x01: s_memMode |=  MF_80STORE;    break;
            case 0x02: s_memMode &= ~MF_AUXREAD;    break;
            case 0x03: s_memMode |=  MF_AUXREAD;    break;
            case 0x04: s_memMode &= ~MF_AUXWRITE;   break;
            case 0x05: s_memMode |=  MF_AUXWRITE;   break;
            case 0x06: s_memMode |=  MF_SLOTCXROM;  break;
            case 0x07: s_memMode &= ~MF_SLOTCXROM;  break;
            case 0x08: s_memMode &= ~MF_ALTZP;      break;
            case 0x09: s_memMode |=  MF_ALTZP;      break;
            case 0x0A: s_memMode &= ~MF_SLOTC3ROM;  break;
            case 0x0B: s_memMode |=  MF_SLOTC3ROM;  break;
            case 0x54: s_memMode &= ~MF_PAGE2;      break;
            case 0x55: s_memMode |=  MF_PAGE2;      break;
            case 0x56: s_memMode &= ~MF_HIRES;      break;
            case 0x57: s_memMode |=  MF_HIRES;      break;
        }
    }

    // IF THE EMULATED PROGRAM HAS JUST UPDATED THE MEMORY WRITE MODE AND IS
    // ABOUT TO UPDATE THE MEMORY READ MODE, HOLD OFF ON ANY PROCESSING UNTIL IT
    // DOES SO.
    BOOL modeChanging = FALSE;
    if ((address >= 4) && (address <= 5) &&
        ((*(LPDWORD)(g_mem + pc) & 0x00FFFEFF) == 0x00C0028D)) {
        modeChanging = TRUE;
        return write ? 0 : MemReturnRandomData(TRUE);
    }
    if ((address >= 0x80) && (address <= 0x8F) && (pc < 0xC000) &&
        (((*(LPDWORD)(g_mem + pc) & 0x00FFFEFF) == 0x00C0048D) ||
        ((*(LPDWORD)(g_mem + pc) & 0x00FFFEFF) == 0x00C0028D))) {
        modeChanging = TRUE;
        return write ? 0 : MemReturnRandomData(TRUE);
    }

    // IF THE MEMORY PAGING MODE HAS CHANGED, UPDATE OUR MEMORY IMAGES AND
    // WRITE TABLES.
    if ((lastMemMode != s_memMode) || modeChanging) {
        // KEEP ONLY ONE MEMORY IMAGE AND WRITE TABLE, AND UPDATE THEM
        // EVERY TIME PAGING IS CHANGED.
        UpdatePaging(false);
    }

    if ((address <= 1) || (address >= 0x54 && address <= 0x57))
        VideoSetMode(pc, address, write, value);

    return write
        ? 0
        : MemReturnRandomData((address == 0x54 || address == 0x55) ? SW_PAGE2() : TRUE);
}
