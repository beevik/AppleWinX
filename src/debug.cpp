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
*   Constants and Types
*
***/

constexpr int BREAKPOINTS     = 5;
constexpr int COMMANDLINES    = 5;
constexpr int COMMANDS        = 61;
constexpr int MAXARGS         = 40;
constexpr int SOURCELINES     = 19;
constexpr int STACKLINES      = 9;
constexpr int WATCHES         = 5;

constexpr LONG SCREENSPLIT1   = 356;
constexpr LONG SCREENSPLIT2   = 456;

constexpr int INVALID1        = 1;
constexpr int INVALID2        = 2;
constexpr int INVALID3        = 3;
constexpr int ADDR_IMM        = 4;
constexpr int ADDR_ABS        = 5;
constexpr int ADDR_ZPG        = 6;
constexpr int ADDR_ABSX       = 7;
constexpr int ADDR_ABSY       = 8;
constexpr int ADDR_ZPGX       = 9;
constexpr int ADDR_ZPGY       = 10;
constexpr int ADDR_REL        = 11;
constexpr int ADDR_INDX       = 12;
constexpr int ADDR_IABSX      = 13;
constexpr int ADDR_INDY       = 14;
constexpr int ADDR_IZPG       = 15;
constexpr int ADDR_IABS       = 16;

constexpr COLORREF COLOR_INSTBKG   = 0;
constexpr COLORREF COLOR_INSTTEXT  = 1;
constexpr COLORREF COLOR_INSTBP    = 2;
constexpr COLORREF COLOR_DATABKG   = 3;
constexpr COLORREF COLOR_DATATEXT  = 4;
constexpr COLORREF COLOR_STATIC    = 5;
constexpr COLORREF COLOR_BPDATA    = 6;
constexpr COLORREF COLOR_COMMAND   = 7;
constexpr COLORREF COLORS          = 8;

typedef BOOL (*fcmd)(int);

static BOOL CmdBlackWhite(int argc);
static BOOL CmdBreakpointAdd(int argc);
static BOOL CmdBreakpointClear(int argc);
static BOOL CmdBreakpointDisable(int argc);
static BOOL CmdBreakpointEnable(int argc);
static BOOL CmdCodeDump(int argc);
static BOOL CmdColor(int argc);
static BOOL CmdExtBenchmark(int argc);
static BOOL CmdFeedKey(int argc);
static BOOL CmdFlagSet(int argc);
static BOOL CmdGo(int argc);
static BOOL CmdInput(int argc);
static BOOL CmdInternalMemoryDump(int argc);
static BOOL CmdLineDown(int argc);
static BOOL CmdLineUp(int argc);
static BOOL CmdMemoryDump(int argc);
static BOOL CmdMemoryEnter(int argc);
static BOOL CmdMemoryFill(int argc);
static BOOL CmdOutput(int argc);
static BOOL CmdPageDown(int argc);
static BOOL CmdPageUp(int argc);
static BOOL CmdProfile(int argc);
static BOOL CmdRegisterSet(int argc);
static BOOL CmdTrace(int argc);
static BOOL CmdTraceFile(int argc);
static BOOL CmdTraceLine(int argc);
static BOOL CmdViewOutput(int argc);
static BOOL CmdWatchAdd(int argc);
static BOOL CmdWatchClear(int argc);
static BOOL CmdZap(int argc);

struct addr {
    char format[12];
    int  bytes;
};

struct arg {
    char str[12];
    WORD val1;
    WORD val2;
};

struct bp {
    WORD address;
    WORD length;
    BOOL enabled;
};

struct cmd {
    char name[12];
    fcmd function;
};

struct inst {
    char  mnemonic[4];
    int   addrmode;
};

static addr addressmode[17] = {
    { "",             1 },    // (implied)
    { "",             1 },    // INVALID1
    { "",             2 },    // INVALID2
    { "",             3 },    // INVALID3
    { "#$%02X",       2 },    // ADDR_IMM
    { "%s",           3 },    // ADDR_ABS
    { "%s",           2 },    // ADDR_ZPG
    { "%s,X",         3 },    // ADDR_ABSX
    { "%s,Y",         3 },    // ADDR_ABSY
    { "%s,X",         2 },    // ADDR_ZPGX
    { "%s,Y",         2 },    // ADDR_ZPGY
    { "%s",           2 },    // ADDR_REL
    { "($%02X,X)",    2 },    // ADDR_INDX
    { "($%04X,X)",    3 },    // ADDR_ABSIINDX
    { "($%02X),Y",    2 },    // ADDR_INDY
    { "($%02X)",      2 },    // ADDR_IZPG
    { "($%04X)",      3 },    // ADDR_IABS
};

static const cmd command[COMMANDS] = {
    { "BA",           CmdBreakpointAdd        },
    { "BC",           CmdBreakpointClear      },
    { "BD",           CmdBreakpointDisable    },
    { "BE",           CmdBreakpointEnable     },
    { "BPM",          CmdBreakpointAdd        },
    { "BW",           CmdBlackWhite           },
    { "COLOR",        CmdColor                },
    { "D",            CmdMemoryDump           },
    { "GOTO",         CmdGo                   },
    { "I",            CmdInput                },
    { "IMD",          CmdInternalMemoryDump   },
    { "INPUT",        CmdInput                },
    { "KEY",          CmdFeedKey              },
    { "MC",           NULL                    }, // CmdMemoryCompare
    { "MD",           CmdMemoryDump           },
    { "MDB",          CmdMemoryDump           },
    { "MDC",          CmdCodeDump             },
    { "ME",           CmdMemoryEnter          },
    { "MEB",          CmdMemoryEnter          },
    { "MEMORY",       CmdMemoryDump           },
    { "MF",           CmdMemoryFill           },
    { "MONO",         CmdBlackWhite           },
    { "MS",           NULL                    }, // CmdMemorySearch
    { "O",            CmdOutput               },
    { "P",            NULL                    }, // CmdStep
    { "PROFILE",      CmdProfile              },
    { "R",            CmdRegisterSet          },
    { "RB",           CmdFlagSet              },
    { "RC",           CmdFlagSet              },
    { "RD",           CmdFlagSet              },
    { "REGISTER",     CmdRegisterSet          },
    { "RET",          NULL                    }, // CmdReturn
    { "RI",           CmdFlagSet              },
    { "RN",           CmdFlagSet              },
    { "RR",           CmdFlagSet              },
    { "RTS",          NULL                    }, // CmdReturn
    { "RV",           CmdFlagSet              },
    { "RZ",           CmdFlagSet              },
    { "SB",           CmdFlagSet              },
    { "SC",           CmdFlagSet              },
    { "SD",           CmdFlagSet              },
    { "SI",           CmdFlagSet              },
    { "SN",           CmdFlagSet              },
    { "SR",           CmdFlagSet              },
    { "SV",           CmdFlagSet              },
    { "SYM",          NULL                    }, // CmdSymbol
    { "SZ",           CmdFlagSet              },
    { "T",            CmdTrace                },
    { "TF",           CmdTraceFile            },
    { "TL",           CmdTraceLine            },
    { "TRACE",        CmdTrace                },
    { "U",            CmdCodeDump             },
    { "W",            CmdWatchAdd             },
    { "W?",           CmdWatchAdd             },
    { "WATCH",        CmdWatchAdd             },
    { "WC",           CmdWatchClear           },
    { "ZAP",          CmdZap                  },
    { "\\",           CmdViewOutput           },
};

static const inst instruction[256] = {
    { "BRK", 0          },  // 00h
    { "ORA", ADDR_INDX  },  // 01h
    { "NOP", INVALID2   },  // 02h
    { "NOP", INVALID1   },  // 03h
    { "TSB", ADDR_ZPG   },  // 04h
    { "ORA", ADDR_ZPG   },  // 05h
    { "ASL", ADDR_ZPG   },  // 06h
    { "NOP", INVALID1   },  // 07h
    { "PHP", 0          },  // 08h
    { "ORA", ADDR_IMM   },  // 09h
    { "ASL", 0          },  // 0Ah
    { "NOP", INVALID1   },  // 0Bh
    { "TSB", ADDR_ABS   },  // 0Ch
    { "ORA", ADDR_ABS   },  // 0Dh
    { "ASL", ADDR_ABS   },  // 0Eh
    { "NOP", INVALID1   },  // 0Fh
    { "BPL", ADDR_REL   },  // 10h
    { "ORA", ADDR_INDY  },  // 11h
    { "ORA", ADDR_IZPG  },  // 12h
    { "NOP", INVALID1   },  // 13h
    { "TRB", ADDR_ZPG   },  // 14h
    { "ORA", ADDR_ZPGX  },  // 15h
    { "ASL", ADDR_ZPGX  },  // 16h
    { "NOP", INVALID1   },  // 17h
    { "CLC", 0          },  // 18h
    { "ORA", ADDR_ABSY  },  // 19h
    { "INA", 0          },  // 1Ah
    { "NOP", INVALID1   },  // 1Bh
    { "TRB", ADDR_ABS   },  // 1Ch
    { "ORA", ADDR_ABSX  },  // 1Dh
    { "ASL", ADDR_ABSX  },  // 1Eh
    { "NOP", INVALID1   },  // 1Fh
    { "JSR", ADDR_ABS   },  // 20h
    { "AND", ADDR_INDX  },  // 21h
    { "NOP", INVALID2   },  // 22h
    { "NOP", INVALID1   },  // 23h
    { "BIT", ADDR_ZPG   },  // 24h
    { "AND", ADDR_ZPG   },  // 25h
    { "ROL", ADDR_ZPG   },  // 26h
    { "NOP", INVALID1   },  // 27h
    { "PLP", 0          },  // 28h
    { "AND", ADDR_IMM   },  // 29h
    { "ROL", 0          },  // 2Ah
    { "NOP", INVALID1   },  // 2Bh
    { "BIT", ADDR_ABS   },  // 2Ch
    { "AND", ADDR_ABS   },  // 2Dh
    { "ROL", ADDR_ABS   },  // 2Eh
    { "NOP", INVALID1   },  // 2Fh
    { "BMI", ADDR_REL   },  // 30h
    { "AND", ADDR_INDY  },  // 31h
    { "AND", ADDR_IZPG  },  // 32h
    { "NOP", INVALID1   },  // 33h
    { "BIT", ADDR_ZPGX  },  // 34h
    { "AND", ADDR_ZPGX  },  // 35h
    { "ROL", ADDR_ZPGX  },  // 36h
    { "NOP", INVALID1   },  // 37h
    { "SEC", 0          },  // 38h
    { "AND", ADDR_ABSY  },  // 39h
    { "DEA", 0          },  // 3Ah
    { "NOP", INVALID1   },  // 3Bh
    { "BIT", ADDR_ABSX  },  // 3Ch
    { "AND", ADDR_ABSX  },  // 3Dh
    { "ROL", ADDR_ABSX  },  // 3Eh
    { "NOP", INVALID1   },  // 3Fh
    { "RTI", 0          },  // 40h
    { "EOR", ADDR_INDX  },  // 41h
    { "NOP", INVALID2   },  // 42h
    { "NOP", INVALID1   },  // 43h
    { "NOP", INVALID2   },  // 44h
    { "EOR", ADDR_ZPG   },  // 45h
    { "LSR", ADDR_ZPG   },  // 46h
    { "NOP", INVALID1   },  // 47h
    { "PHA", 0          },  // 48h
    { "EOR", ADDR_IMM   },  // 49h
    { "LSR", 0          },  // 4Ah
    { "NOP", INVALID1   },  // 4Bh
    { "JMP", ADDR_ABS   },  // 4Ch
    { "EOR", ADDR_ABS   },  // 4Dh
    { "LSR", ADDR_ABS   },  // 4Eh
    { "NOP", INVALID1   },  // 4Fh
    { "BVC", ADDR_REL   },  // 50h
    { "EOR", ADDR_INDY  },  // 51h
    { "EOR", ADDR_IZPG  },  // 52h
    { "NOP", INVALID1   },  // 53h
    { "NOP", INVALID2   },  // 54h
    { "EOR", ADDR_ZPGX  },  // 55h
    { "LSR", ADDR_ZPGX  },  // 56h
    { "NOP", INVALID1   },  // 57h
    { "CLI", 0          },  // 58h
    { "EOR", ADDR_ABSY  },  // 59h
    { "PHY", 0          },  // 5Ah
    { "NOP", INVALID1   },  // 5Bh
    { "NOP", INVALID3   },  // 5Ch
    { "EOR", ADDR_ABSX  },  // 5Dh
    { "LSR", ADDR_ABSX  },  // 5Eh
    { "NOP", INVALID1   },  // 5Fh
    { "RTS", 0          },  // 60h
    { "ADC", ADDR_INDX  },  // 61h
    { "NOP", INVALID2   },  // 62h
    { "NOP", INVALID1   },  // 63h
    { "STZ", ADDR_ZPG   },  // 64h
    { "ADC", ADDR_ZPG   },  // 65h
    { "ROR", ADDR_ZPG   },  // 66h
    { "NOP", INVALID1   },  // 67h
    { "PLA", 0          },  // 68h
    { "ADC", ADDR_IMM   },  // 69h
    { "ROR", 0          },  // 6Ah
    { "NOP", INVALID1   },  // 6Bh
    { "JMP", ADDR_IABS  },  // 6Ch
    { "ADC", ADDR_ABS   },  // 6Dh
    { "ROR", ADDR_ABS   },  // 6Eh
    { "NOP", INVALID1   },  // 6Fh
    { "BVS", ADDR_REL   },  // 70h
    { "ADC", ADDR_INDY  },  // 71h
    { "ADC", ADDR_IZPG  },  // 72h
    { "NOP", INVALID1   },  // 73h
    { "STZ", ADDR_ZPGX  },  // 74h
    { "ADC", ADDR_ZPGX  },  // 75h
    { "ROR", ADDR_ZPGX  },  // 76h
    { "NOP", INVALID1   },  // 77h
    { "SEI", 0          },  // 78h
    { "ADC", ADDR_ABSY  },  // 79h
    { "PLY", 0          },  // 7Ah
    { "NOP", INVALID1   },  // 7Bh
    { "JMP", ADDR_IABSX },  // 7Ch
    { "ADC", ADDR_ABSX  },  // 7Dh
    { "ROR", ADDR_ABSX  },  // 7Eh
    { "NOP", INVALID1   },  // 7Fh
    { "BRA", ADDR_REL   },  // 80h
    { "STA", ADDR_INDX  },  // 81h
    { "NOP", INVALID2   },  // 82h
    { "NOP", INVALID1   },  // 83h
    { "STY", ADDR_ZPG   },  // 84h
    { "STA", ADDR_ZPG   },  // 85h
    { "STX", ADDR_ZPG   },  // 86h
    { "NOP", INVALID1   },  // 87h
    { "DEY", 0          },  // 88h
    { "BIT", ADDR_IMM   },  // 89h
    { "TXA", 0          },  // 8Ah
    { "NOP", INVALID1   },  // 8Bh
    { "STY", ADDR_ABS   },  // 8Ch
    { "STA", ADDR_ABS   },  // 8Dh
    { "STX", ADDR_ABS   },  // 8Eh
    { "NOP", INVALID1   },  // 8Fh
    { "BCC", ADDR_REL   },  // 90h
    { "STA", ADDR_INDY  },  // 91h
    { "STA", ADDR_IZPG  },  // 92h
    { "NOP", INVALID1   },  // 93h
    { "STY", ADDR_ZPGX  },  // 94h
    { "STA", ADDR_ZPGX  },  // 95h
    { "STX", ADDR_ZPGY  },  // 96h
    { "NOP", INVALID1   },  // 97h
    { "TYA", 0          },  // 98h
    { "STA", ADDR_ABSY  },  // 99h
    { "TXS", 0          },  // 9Ah
    { "NOP", INVALID1   },  // 9Bh
    { "STZ", ADDR_ABS   },  // 9Ch
    { "STA", ADDR_ABSX  },  // 9Dh
    { "STZ", ADDR_ABSX  },  // 9Eh
    { "NOP", INVALID1   },  // 9Fh
    { "LDY", ADDR_IMM   },  // A0h
    { "LDA", ADDR_INDX  },  // A1h
    { "LDX", ADDR_IMM   },  // A2h
    { "NOP", INVALID1   },  // A3h
    { "LDY", ADDR_ZPG   },  // A4h
    { "LDA", ADDR_ZPG   },  // A5h
    { "LDX", ADDR_ZPG   },  // A6h
    { "NOP", INVALID1   },  // A7h
    { "TAY", 0          },  // A8h
    { "LDA", ADDR_IMM   },  // A9h
    { "TAX", 0          },  // AAh
    { "NOP", INVALID1   },  // ABh
    { "LDY", ADDR_ABS   },  // ACh
    { "LDA", ADDR_ABS   },  // ADh
    { "LDX", ADDR_ABS   },  // AEh
    { "NOP", INVALID1   },  // AFh
    { "BCS", ADDR_REL   },  // B0h
    { "LDA", ADDR_INDY  },  // B1h
    { "LDA", ADDR_IZPG  },  // B2h
    { "NOP", INVALID1   },  // B3h
    { "LDY", ADDR_ZPGX  },  // B4h
    { "LDA", ADDR_ZPGX  },  // B5h
    { "LDX", ADDR_ZPGY  },  // B6h
    { "NOP", INVALID1   },  // B7h
    { "CLV", 0          },  // B8h
    { "LDA", ADDR_ABSY  },  // B9h
    { "TSX", 0          },  // BAh
    { "NOP", INVALID1   },  // BBh
    { "LDY", ADDR_ABSX  },  // BCh
    { "LDA", ADDR_ABSX  },  // BDh
    { "LDX", ADDR_ABSY  },  // BEh
    { "NOP", INVALID1   },  // BFh
    { "CPY", ADDR_IMM   },  // C0h
    { "CMP", ADDR_INDX  },  // C1h
    { "NOP", INVALID2   },  // C2h
    { "NOP", INVALID1   },  // C3h
    { "CPY", ADDR_ZPG   },  // C4h
    { "CMP", ADDR_ZPG   },  // C5h
    { "DEC", ADDR_ZPG   },  // C6h
    { "NOP", INVALID1   },  // C7h
    { "INY", 0          },  // C8h
    { "CMP", ADDR_IMM   },  // C9h
    { "DEX", 0          },  // CAh
    { "NOP", INVALID1   },  // CBh
    { "CPY", ADDR_ABS   },  // CCh
    { "CMP", ADDR_ABS   },  // CDh
    { "DEC", ADDR_ABS   },  // CEh
    { "NOP", INVALID1   },  // CFh
    { "BNE", ADDR_REL   },  // D0h
    { "CMP", ADDR_INDY  },  // D1h
    { "CMP", ADDR_IZPG  },  // D2h
    { "NOP", INVALID1   },  // D3h
    { "NOP", INVALID2   },  // D4h
    { "CMP", ADDR_ZPGX  },  // D5h
    { "DEC", ADDR_ZPGX  },  // D6h
    { "NOP", INVALID1   },  // D7h
    { "CLD", 0          },  // D8h
    { "CMP", ADDR_ABSY  },  // D9h
    { "PHX", 0          },  // DAh
    { "NOP", INVALID1   },  // DBh
    { "NOP", INVALID3   },  // DCh
    { "CMP", ADDR_ABSX  },  // DDh
    { "DEC", ADDR_ABSX  },  // DEh
    { "NOP", INVALID1   },  // DFh
    { "CPX", ADDR_IMM   },  // E0h
    { "SBC", ADDR_INDX  },  // E1h
    { "NOP", INVALID2   },  // E2h
    { "NOP", INVALID1   },  // E3h
    { "CPX", ADDR_ZPG   },  // E4h
    { "SBC", ADDR_ZPG   },  // E5h
    { "INC", ADDR_ZPG   },  // E6h
    { "NOP", INVALID1   },  // E7h
    { "INX", 0          },  // E8h
    { "SBC", ADDR_IMM   },  // E9h
    { "NOP", 0          },  // EAh
    { "NOP", INVALID1   },  // EBh
    { "CPX", ADDR_ABS   },  // ECh
    { "SBC", ADDR_ABS   },  // EDh
    { "INC", ADDR_ABS   },  // EEh
    { "NOP", INVALID1   },  // EFh
    { "BEQ", ADDR_REL   },  // F0h
    { "SBC", ADDR_INDY  },  // F1h
    { "SBC", ADDR_IZPG  },  // F2h
    { "NOP", INVALID1   },  // F3h
    { "NOP", INVALID2   },  // F4h
    { "SBC", ADDR_ZPGX  },  // F5h
    { "INC", ADDR_ZPGX  },  // F6h
    { "NOP", INVALID1   },  // F7h
    { "SED", 0          },  // F8h
    { "SBC", ADDR_ABSY  },  // F9h
    { "PLX", 0          },  // FAh
    { "NOP", INVALID1   },  // FBh
    { "NOP", INVALID3   },  // FCh
    { "SBC", ADDR_ABSX  },  // FDh
    { "INC", ADDR_ABSX  },  // FEh
    { "NOP", INVALID1   },  // FFh
};

static const COLORREF color[2][COLORS] = {
    {
        0x800000, 0xC0C0C0, 0x00FFFF, 0x808000,
        0x000080, 0x800000, 0x00FFFF, 0xFFFFFF
    },
    {
        0x000000, 0xC0C0C0, 0xFFFFFF, 0x000000,
        0xC0C0C0, 0x808080, 0xFFFFFF, 0xFFFFFF
    },
};

static char commandstring[COMMANDLINES][80] = {
    "",
    " ",
    " Apple //e Emulator for Windows",
    " ",
    " "
};


/****************************************************************************
*
*   Variables
*
***/

static arg         argv[MAXARGS];
static bp          breakpoint[BREAKPOINTS];
static DWORD       profiledata[256];
static int         watch[WATCHES];

static int         colorscheme   = 0;
static HFONT       debugfont     = (HFONT)0;
static WORD        lastpc        = 0;
static LPBYTE      membank       = NULL;
static WORD        memorydump    = 0;
static BOOL        profiling     = FALSE;
static int         stepcount     = 0;
static DWORD       stepline      = 0;
static int         stepstart     = 0;
static int         stepuntil     = -1;
static WORD        topoffset     = 0;
static FILE *      tracefile     = NULL;
static BOOL        usingbp       = FALSE;
static BOOL        usingmemdump  = FALSE;
static BOOL        usingwatches  = FALSE;
static BOOL        viewingoutput = FALSE;

static std::unordered_map<WORD, std::string> addrtosym;
static std::unordered_map<std::string, WORD> symtoaddr;

static void ComputeTopOffset(WORD centeroffset);
static BOOL DisplayError(const char * errortext);
static BOOL DisplayHelp(fcmd function);
static int GetAddress(const char * symbol);
static const char * GetSymbol(WORD address, int bytes, char * tmpbuf, int tmpbufsiz);
static void GetTargets(int * intermediate, int * final);
static BOOL InternalSingleStep();


/****************************************************************************
*
*   Local functions
*
***/

//===========================================================================
static BOOL CheckBreakpoint(WORD address, BOOL memory) {
    int target[3] = { address,-1,-1 };
    if (memory)
        GetTargets(&target[1], &target[2]);
    int targetnum = memory ? 3 : 1;
    while (targetnum--)
        if (target[targetnum] >= 0) {
            WORD targetaddr = (WORD)(target[targetnum] & 0xFFFF);
            int  slot = 0;
            while (slot < BREAKPOINTS) {
                if (breakpoint[slot].length && breakpoint[slot].enabled &&
                    (breakpoint[slot].address <= targetaddr) &&
                    (breakpoint[slot].address + breakpoint[slot].length > targetaddr))
                    return TRUE;
                slot++;
            }
        }
    return FALSE;
}

//===========================================================================
static BOOL CheckJump(WORD targetaddress) {
    WORD savedpc = regs.pc;
    InternalSingleStep();
    BOOL result = (regs.pc == targetaddress);
    regs.pc = savedpc;
    return result;
}

//===========================================================================
static BOOL CmdBlackWhite(int argc) {
    colorscheme = 1;
    DebugDisplay(TRUE);
    return FALSE;
}

//===========================================================================
static BOOL CmdBreakpointAdd(int argc) {
    if (argc == 1)
        argv[argc = 2].val1 = regs.pc;

    BOOL addedone = FALSE;
    for (int loop = 1; loop < argc; ++loop) {
        int addr = GetAddress(argv[loop].str);
        if (argv[loop].val1 || argv[loop].str[0] == '0' || addr >= 0) {
            if (!argv[loop].val1)
                argv[loop].val1 = addr;

            int freeslot = 0;
            while ((freeslot < BREAKPOINTS) && breakpoint[freeslot].length)
                freeslot++;
            if ((freeslot >= BREAKPOINTS) && !addedone)
                return DisplayError("All breakpoint slots are currently in use.");

            if (freeslot < BREAKPOINTS) {
                breakpoint[freeslot].address = argv[loop].val1;
                breakpoint[freeslot].length = argv[loop].val2
                    ? MIN(0x10000 - argv[loop].val1, argv[loop].val2)
                    : 1;
                breakpoint[freeslot].enabled = 1;
                addedone = TRUE;
                usingbp = TRUE;
            }

        }
    }
    if (!addedone)
        return DisplayHelp(CmdBreakpointAdd);
    return TRUE;
}

//===========================================================================
static BOOL CmdBreakpointClear(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdBreakpointClear);
    if (!usingbp)
        return DisplayError("There are no breakpoints defined.");

    for (int i = 1; i < argc; ++i) {
        if (!StrCmp(argv[i].str, "*")) {
            for (int j = 0; j < BREAKPOINTS; ++j)
                breakpoint[j].length = 0;
        }
        else if (argv[i].val1 >= 1 && argv[i].val1 <= BREAKPOINTS)
            breakpoint[argv[i].val1 - 1].length = 0;
    }

    int usedslot = 0;
    while (usedslot < BREAKPOINTS && !breakpoint[usedslot].length)
        usedslot++;
    if (usedslot >= BREAKPOINTS) {
        usingbp = FALSE;
        DebugDisplay(TRUE);
        return FALSE;
    }
    return TRUE;
}

//===========================================================================
static BOOL CmdBreakpointDisable(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdBreakpointDisable);
    if (!usingbp)
        return DisplayError("There are no breakpoints defined.");

    for (int i = 1; i < argc; ++i) {
        if (!StrCmp(argv[i].str, "*")) {
            for (int j = 0; j < BREAKPOINTS; ++j)
                breakpoint[j].enabled = FALSE;
        }
        else if (argv[i].val1 >= 1 && argv[i].val1 <= BREAKPOINTS)
            breakpoint[argv[i].val1 - 1].enabled = FALSE;
    }

    return TRUE;
}

//===========================================================================
static BOOL CmdBreakpointEnable(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdBreakpointEnable);
    if (!usingbp)
        return DisplayError("There are no breakpoints defined.");

    for (int i = 1; i < argc; ++i) {
        if (!StrCmp(argv[i].str, "*")) {
            for (int j = 0; j < BREAKPOINTS; ++j)
                breakpoint[j].enabled = TRUE;
        }
        else if (argv[i].val1 >= 1 && argv[i].val1 <= BREAKPOINTS)
            breakpoint[argv[i].val1 - 1].enabled = 1;
    }

    return TRUE;
}

//===========================================================================
static BOOL CmdCodeDump(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdCodeDump);

    int addr = GetAddress(argv[1].str);
    if (argv[1].str[0] != '0' && !argv[1].val1 && addr < 0)
        return DisplayHelp(CmdCodeDump);

    topoffset = argv[1].val1 ? argv[1].val1 : (WORD)addr;
    return TRUE;
}

//===========================================================================
static BOOL CmdColor(int argc) {
    colorscheme = 0;
    DebugDisplay(TRUE);
    return FALSE;
}

//===========================================================================
static BOOL CmdFeedKey(int argc) {
    int key = argc > 1
        ? argv[1].val1
            ? argv[1].val1
            : argv[1].str[0]
        : VK_SPACE;
    KeybQueueKeypress(key, 0);
    return FALSE;
}

//===========================================================================
static BOOL CmdFlagSet(int argc) {
    static const char flagname[] = "CZIDBRVN";
    int loop = 0;
    for (; loop < 8; ++loop) {
        if (flagname[loop] == argv[0].str[1]) {
            if (argv[0].str[0] == 'R')
                regs.ps &= ~(1 << loop);
            else
                regs.ps |= (1 << loop);
            return TRUE;
        }
    }
    return FALSE;
}

//===========================================================================
static BOOL CmdGo(int argc) {
    stepcount = -1;
    stepline  = 0;
    stepstart = regs.pc;
    stepuntil = argc ? argv[1].val1 : -1;
    if (!stepuntil) {
        int addr = GetAddress(argv[1].str);
        if (addr >= 0)
            stepuntil = addr;
    }
    EmulatorSetMode(EMULATOR_MODE_STEPPING);
    return FALSE;
}

//===========================================================================
static BOOL CmdInput(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdInput);

    int addr = GetAddress(argv[1].str);
    if (argv[1].str[0] != '0' && !argv[1].val1 && addr < 0)
        return DisplayHelp(CmdInput);

    if (!argv[1].val1)
        argv[1].val1 = (WORD)addr;

    ioRead[argv[1].val1 & 0xFF](regs.pc, argv[1].val1 & 0xFF, 0, 0);
    return TRUE;
}

//===========================================================================
static BOOL CmdInternalMemoryDump(int argc) {
    char filename[MAX_PATH];
    StrCopy(filename, EmulatorGetProgramDirectory(), ARRSIZE(filename));
    StrCat(filename, "memory.bin", ARRSIZE(filename));

    FILE * fp = fopen(filename, "wb");
    if (fp) {
        fwrite(g_mem, 0x30000, 1, fp);
        fclose(fp);
    }
    return FALSE;
}

//===========================================================================
static BOOL CmdLineDown(int argc) {
    const inst & in   = instruction[g_mem[topoffset]];
    const addr & mode = addressmode[in.addrmode];
    topoffset += mode.bytes;
    return TRUE;
}

//===========================================================================
static BOOL CmdLineUp(int argc) {
    WORD savedoffset = topoffset;
    ComputeTopOffset(topoffset);
    WORD newoffset = topoffset;
    while (newoffset < savedoffset) {
        topoffset = newoffset;
        const inst & in   = instruction[g_mem[newoffset]];
        const addr & mode = addressmode[in.addrmode];
        newoffset += mode.bytes;
    }
    topoffset = MIN(topoffset, savedoffset - 1);
    return TRUE;
}

//===========================================================================
static BOOL CmdMemoryDump(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdMemoryDump);

    int addr = GetAddress(argv[1].str);
    if (argv[1].str[0] != '0' && !argv[1].val1 && addr < 0)
        return DisplayHelp(CmdMemoryDump);

    memorydump = addr >= 0 ? (WORD)addr : argv[1].val1;
    usingmemdump = TRUE;
    return TRUE;
}

//===========================================================================
static BOOL CmdMemoryEnter(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdMemoryEnter);

    int addr = GetAddress(argv[1].str);
    if (argv[1].str[0] != '0' && !argv[1].val1 && addr < 0)
        return DisplayHelp(CmdMemoryEnter);
    if (argv[2].str[0] != '0' && !argv[2].val1)
        return DisplayHelp(CmdMemoryEnter);

    WORD address = argv[1].val1 ? argv[1].val1 : (WORD)addr;
    for (int i = 0; i < argc - 2; ++i) {
        membank[address + i] = (BYTE)argv[i + 2].val1;
        g_memDirty[(address + i) >> 8] = 1;
    }

    return TRUE;
}

//===========================================================================
static BOOL CmdMemoryFill(int argc) {
    if (argc < 4)
        return DisplayHelp(CmdMemoryFill);

    int addr = GetAddress(argv[1].str);
    if (argv[1].str[0] != '0' && !argv[1].val1 && addr < 0)
        return DisplayHelp(CmdMemoryFill);

    WORD address = argv[1].val1 ? argv[1].val1 : (WORD)addr;
    WORD bytes = MAX(1, argv[2].val1);
    BYTE value = (BYTE)argv[3].val1;
    while (bytes--) {
        if (address < 0xC000 || address > 0xC0FF) {
            membank[address] = value;
            g_memDirty[address >> 8] = 1;
        }
        address++;
    }
    return TRUE;
}

//===========================================================================
static BOOL CmdOutput(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdInput);

    int addr = GetAddress(argv[1].str);
    if (argv[1].str[0] != '0' && !argv[1].val1 && addr < 0)
        return DisplayHelp(CmdInput);

    if (!argv[1].val1)
        argv[1].val1 = (WORD)addr;
    ioWrite[argv[1].val1 & 0xFF](regs.pc, argv[1].val1 & 0xFF, 1, argv[2].val1 & 0xFF);
    return TRUE;
}

//===========================================================================
static BOOL CmdPageDown(int argc) {
    for (int loop = 0; loop < SOURCELINES; ++loop)
        CmdLineDown(0);
    return TRUE;
}

//===========================================================================
static BOOL CmdPageUp(int argc) {
    for (int loop = 0; loop < SOURCELINES; ++loop)
        CmdLineUp(0);
    return TRUE;
}

//===========================================================================
static BOOL CmdProfile(int argc) {
    ZeroMemory(profiledata, 256 * sizeof(DWORD));
    profiling = TRUE;
    return FALSE;
}

//===========================================================================
static BOOL CmdRegisterSet(int argc) {
    if (argc == 3 && argv[1].str[0] == 'P' && argv[2].str[0] == 'L')
        regs.pc = lastpc;
    else if (argc < 3 || (argv[2].str[0] != '0' && !argv[2].val1))
        return DisplayHelp(CmdMemoryEnter);
    else {
        switch (argv[1].str[0]) {
            case 'A':
                regs.a = (BYTE)argv[2].val1;
                break;
            case 'P':
                regs.pc = argv[2].val1;
                break;
            case 'S':
                regs.sp = 0x100 | (argv[2].val1 & 0xFF);
                break;
            case 'X':
                regs.x = (BYTE)argv[2].val1;
                break;
            case 'Y':
                regs.y = (BYTE)argv[2].val1;
                break;
            default:
                return DisplayHelp(CmdMemoryEnter);
        }
    }
    ComputeTopOffset(regs.pc);
    return TRUE;
}

//===========================================================================
static BOOL CmdTrace(int argc) {
    stepcount = argc > 1 ? argv[1].val1 : 1;
    stepline  = 0;
    stepstart = regs.pc;
    stepuntil = -1;
    EmulatorSetMode(EMULATOR_MODE_STEPPING);
    DebugAdvance();
    return FALSE;
}

//===========================================================================
static BOOL CmdTraceFile(int argc) {
    if (tracefile)
        fclose(tracefile);
    char filename[MAX_PATH];
    StrCopy(filename, EmulatorGetProgramDirectory(), ARRSIZE(filename));
    StrCat(filename, (argc && argv[1].str[0]) ? argv[1].str : "trace.txt", ARRSIZE(filename));
    tracefile = fopen(filename, "wt");
    return FALSE;
}

//===========================================================================
static BOOL CmdTraceLine(int argc) {
    stepcount = argc > 1 ? argv[1].val1 : 1;
    stepline  = 1;
    stepstart = regs.pc;
    stepuntil = -1;
    EmulatorSetMode(EMULATOR_MODE_STEPPING);
    DebugAdvance();
    return FALSE;
}

//===========================================================================
static BOOL CmdViewOutput(int argc) {
    VideoRefreshScreen();
    viewingoutput = TRUE;
    return FALSE;
}

//===========================================================================
static BOOL CmdWatchAdd(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdWatchAdd);

    BOOL addedone = FALSE;
    for (int loop = 1; loop < argc; ++loop) {
        int addr = GetAddress(argv[loop].str);
        if (argv[loop].val1 || (argv[loop].str[0] == '0') || addr >= 0) {
            if (!argv[loop].val1)
                argv[loop].val1 = (WORD)addr;

            int freeslot = 0;
            while ((freeslot < WATCHES) && (watch[freeslot] >= 0))
                freeslot++;
            if ((freeslot >= WATCHES) && !addedone)
                return DisplayError("All watch slots are currently in use.");

            if ((argv[loop].val1 >= 0xC000) && (argv[loop].val1 <= 0xC0FF))
                return DisplayError("You may not watch an I/O location.");

            if (freeslot < WATCHES) {
                watch[freeslot] = argv[loop].val1;
                addedone = TRUE;
                usingwatches = TRUE;
            }
        }
    }
    if (!addedone)
        return DisplayHelp(CmdWatchAdd);
    return TRUE;
}

//===========================================================================
static BOOL CmdWatchClear(int argc) {
    if (argc < 2)
        return DisplayHelp(CmdWatchAdd);
    if (!usingwatches)
        return DisplayError("There are no watches defined.");

    for (int i = 1; i < argc; ++i) {
        if (!StrCmp(argv[i].str, "*")) {
            for (int j = 0; j < WATCHES; ++j)
                watch[j] = -1;
        }
        else if ((argv[i].val1 >= 1) && (argv[i].val1 <= WATCHES))
            watch[argv[i].val1 - 1] = -1;
    }

    int usedslot = 0;
    while (usedslot < WATCHES && watch[usedslot] < 0)
        usedslot++;
    if (usedslot >= WATCHES) {
        usingwatches = FALSE;
        DebugDisplay(TRUE);
        return FALSE;
    }
    return TRUE;
}

//===========================================================================
static BOOL CmdZap(int argc) {
    int loop = addressmode[instruction[g_mem[regs.pc]].addrmode].bytes;
    while (loop--)
        g_mem[regs.pc + loop] = 0xEA;
    return TRUE;
}

//===========================================================================
static void ComputeTopOffset(WORD centeroffset) {
    topoffset = centeroffset - 0x30;
    BOOL invalid;
    do {
        invalid = 0;
        WORD instofs[0x30];
        WORD currofs = topoffset;
        int  currnum = 0;
        do {
            int addrmode = instruction[g_mem[currofs]].addrmode;
            if ((addrmode >= 1) && (addrmode <= 3))
                invalid = 1;
            else {
                instofs[currnum++] = currofs;
                currofs += addressmode[addrmode].bytes;
            }
        } while ((!invalid) && (currofs < centeroffset));
        if (invalid)
            topoffset++;
        else if (currnum > (SOURCELINES >> 1))
            topoffset = instofs[currnum - (SOURCELINES >> 1)];
    } while (invalid);
}

//===========================================================================
static BOOL DisplayError(const char * errortext) {
    return FALSE;
}

//===========================================================================
static BOOL DisplayHelp(fcmd function) {
    return FALSE;
}

//===========================================================================
static void DrawBreakpoints(HDC dc, int line) {
    RECT linerect;
    linerect.left   = SCREENSPLIT2;
    linerect.top    = (line << 4);
    linerect.right  = 560;
    linerect.bottom = linerect.top + 16;
    char fulltext[16] = "Breakpoints";
    SetTextColor(dc, color[colorscheme][COLOR_STATIC]);
    SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
    int loop = 0;
    do {
        ExtTextOut(
            dc,
            linerect.left,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            fulltext,
            StrLen(fulltext),
            NULL
        );
        linerect.top += 16;
        linerect.bottom += 16;
        if ((loop < BREAKPOINTS) && breakpoint[loop].length) {
            StrPrintf(
                fulltext,
                ARRSIZE(fulltext),
                "%d: %04X",
                loop + 1,
                (unsigned)breakpoint[loop].address
            );
            if (breakpoint[loop].length > 1) {
                int offset = StrLen(fulltext);
                StrPrintf(
                    fulltext + offset,
                    ARRSIZE(fulltext) - offset,
                    "-%04X",
                    (unsigned)(breakpoint[loop].address + breakpoint[loop].length - 1)
                );
            }
            SetTextColor(dc, color[colorscheme][breakpoint[loop].enabled ? COLOR_BPDATA
                : COLOR_STATIC]);
        }
        else
            fulltext[0] = 0;
    } while (loop++ < BREAKPOINTS);
}

//===========================================================================
static void DrawCommandLine(HDC dc, int line) {
    BOOL title = (commandstring[line][0] == ' ');
    SetTextColor(dc, color[colorscheme][COLOR_COMMAND]);
    SetBkColor(dc, 0);
    RECT linerect;
    linerect.left   = 0;
    linerect.top    = 368 - (line << 4);
    linerect.right  = 12;
    linerect.bottom = linerect.top + 16;
    if (!title) {
        ExtTextOut(
            dc,
            1,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            ">",
            1,
            NULL
        );
        linerect.left = 12;
    }
    linerect.right = 560;
    ExtTextOut(
        dc,
        linerect.left,
        linerect.top,
        ETO_CLIPPED | ETO_OPAQUE,
        &linerect,
        commandstring[line] + title,
        StrLen(commandstring[line] + title),
        NULL
    );
}

//===========================================================================
static WORD DrawDisassembly(HDC dc, int line, WORD offset, char * text, size_t textchars) {
    char addresstext[40] = "";
    char bytestext[10]   = "";
    char fulltext[50]    = "";
    BYTE inst            = g_mem[offset];
    int  addrmode        = instruction[inst].addrmode;
    WORD bytes           = addressmode[addrmode].bytes;

    // BUILD A STRING CONTAINING THE TARGET ADDRESS OR SYMBOL
    if (addressmode[addrmode].format[0]) {
        WORD address = *(LPWORD)(g_mem + offset + 1);
        if (bytes == 2)
            address &= 0xFF;
        if (addrmode == ADDR_REL)
            address = offset + 2 + (int)(signed char)address;
        if (StrStr(addressmode[addrmode].format, "%s")) {
            char tmpbuf[16];
            StrPrintf(
                addresstext,
                ARRSIZE(addresstext),
                addressmode[addrmode].format,
                GetSymbol(address, bytes, tmpbuf, ARRSIZE(tmpbuf))
            );
        }
        else {
            StrPrintf(
                addresstext,
                ARRSIZE(addresstext),
                addressmode[addrmode].format,
                (unsigned)address
            );
        }
        if ((addrmode == ADDR_REL) && (offset == regs.pc) && CheckJump(address))
            if (address > offset)
                StrCat(addresstext, " \x19", ARRSIZE(addresstext));
            else
                StrCat(addresstext, " \x18", ARRSIZE(addresstext));
    }

    // BUILD A STRING CONTAINING THE ACTUAL BYTES THAT MAKE UP THIS
    // INSTRUCTION
    {
        int loop = 0;
        while (loop < bytes) {
            int offset = StrLen(bytestext);
            StrPrintf(
                bytestext + offset,
                ARRSIZE(bytestext) - offset,
                "%02X",
                (unsigned) * (g_mem + offset + (loop++))
            );
        }
        while (StrLen(bytestext) < 6)
            StrCat(bytestext, " ", ARRSIZE(bytestext));
    }

    // PUT TOGETHER ALL OF THE DIFFERENT ELEMENTS THAT WILL MAKE UP THE LINE
    char tmpbuf[16];
    StrPrintf(
        fulltext,
        ARRSIZE(fulltext),
        "%04X  %s  %-9s %s %s",
        (unsigned)offset,
        bytestext,
        GetSymbol(offset, 0, tmpbuf, ARRSIZE(tmpbuf)),
        instruction[inst].mnemonic,
        addresstext
    );
    if (text)
        StrCopy(text, fulltext, textchars);

    // DRAW THE LINE
    if (dc) {
        RECT linerect;
        linerect.left   = 0;
        linerect.top    = line << 4;
        linerect.right  = SCREENSPLIT1 - 14;
        linerect.bottom = linerect.top + 16;
        BOOL bp = usingbp && CheckBreakpoint(offset, offset == regs.pc);
        SetTextColor(
            dc,
            color[colorscheme][(offset == regs.pc) ? COLOR_INSTBKG : bp ? COLOR_INSTBP : COLOR_INSTTEXT]
        );
        SetBkColor(
            dc,
            color[colorscheme][(offset == regs.pc) ? bp ? COLOR_INSTBP : COLOR_INSTTEXT : COLOR_INSTBKG]
        );
        ExtTextOut(
            dc,
            12,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            fulltext,
            StrLen(fulltext),
            NULL
        );
    }

    return bytes;
}

//===========================================================================
static void DrawFlags(HDC dc, int line, WORD value, char * text, size_t textchars) {
    char mnemonic[9] = "NVRBDIZC";
    char fulltext[2] = "?";
    RECT  linerect;
    if (dc) {
        linerect.left   = SCREENSPLIT1 + 63;
        linerect.top    = line << 4;
        linerect.right  = SCREENSPLIT1 + 72;
        linerect.bottom = linerect.top + 16;
        SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
    }
    int loop = 8;
    while (loop--) {
        if (dc) {
            fulltext[0] = mnemonic[loop];
            SetTextColor(dc, color[colorscheme][(value & 1) ? COLOR_DATATEXT : COLOR_STATIC]);
            ExtTextOut(
                dc,
                linerect.left,
                linerect.top,
                ETO_CLIPPED | ETO_OPAQUE,
                &linerect,
                fulltext,
                1,
                NULL
            );
            linerect.left -= 9;
            linerect.right -= 9;
        }
        if (!(value & 1))
            mnemonic[loop] = '.';
        value >>= 1;
    }
    if (text)
        StrCopy(text, mnemonic, textchars);
}

//===========================================================================
static void DrawMemory(HDC dc, int line) {
    RECT linerect;
    linerect.left   = SCREENSPLIT2;
    linerect.top    = (line << 4);
    linerect.right  = 560;
    linerect.bottom = linerect.top + 16;
    char fulltext[16];
    StrPrintf(fulltext, ARRSIZE(fulltext), "Mem at %04X", (unsigned)memorydump);
    SetTextColor(dc, color[colorscheme][COLOR_STATIC]);
    SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
    WORD curraddr = memorydump;
    int  loop = 0;
    do {
        ExtTextOut(
            dc,
            linerect.left,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            fulltext,
            StrLen(fulltext),
            NULL
        );
        linerect.top += 16;
        linerect.bottom += 16;
        fulltext[0] = 0;
        if (loop < 4) {
            int loop2 = 0;
            while (loop2++ < 4) {
                if ((curraddr >= 0xC000) && (curraddr <= 0xC0FF)) {
                    int offset = StrLen(fulltext);
                    StrCopy(fulltext + offset, "IO ", ARRSIZE(fulltext) - offset);
                }
                else {
                    int offset = StrLen(fulltext);
                    StrPrintf(
                        fulltext + offset,
                        ARRSIZE(fulltext) - offset,
                        "%02X ",
                        membank[curraddr]
                    );
                }
                curraddr++;
            }
        }
        SetTextColor(dc, color[colorscheme][COLOR_DATATEXT]);
    } while (loop++ < 4);
}

//===========================================================================
static void DrawRegister(HDC dc, int line, const char * name, int bytes, WORD value) {
    RECT linerect;
    linerect.left   = SCREENSPLIT1;
    linerect.top    = line << 4;
    linerect.right  = SCREENSPLIT1 + 40;
    linerect.bottom = linerect.top + 16;
    SetTextColor(dc, color[colorscheme][COLOR_STATIC]);
    SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
    ExtTextOut(
        dc,
        linerect.left,
        linerect.top,
        ETO_CLIPPED | ETO_OPAQUE,
        &linerect,
        name,
        StrLen(name),
        NULL
    );
    char valuestr[8];
    if (bytes == 2)
        StrPrintf(valuestr, ARRSIZE(valuestr), "%04X", (unsigned)value);
    else
        StrPrintf(valuestr, ARRSIZE(valuestr), "%02X", (unsigned)value);
    linerect.left  = SCREENSPLIT1 + 40;
    linerect.right = SCREENSPLIT2;
    SetTextColor(dc, color[colorscheme][COLOR_DATATEXT]);
    ExtTextOut(
        dc,
        linerect.left,
        linerect.top,
        ETO_CLIPPED | ETO_OPAQUE,
        &linerect,
        valuestr,
        StrLen(valuestr),
        NULL
    );
}

//===========================================================================
static void DrawStack(HDC dc, int line) {
    unsigned curraddr = regs.sp;
    int      loop = 0;
    while (loop < STACKLINES) {
        curraddr++;
        RECT linerect;
        linerect.left   = SCREENSPLIT1;
        linerect.top    = (loop + line) << 4;
        linerect.right  = SCREENSPLIT1 + 40;
        linerect.bottom = linerect.top + 16;
        SetTextColor(dc, color[colorscheme][COLOR_STATIC]);
        SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
        char outtext[8] = "";
        if (curraddr <= 0x1FF)
            StrPrintf(outtext, ARRSIZE(outtext), "%04X", curraddr);
        ExtTextOut(
            dc,
            linerect.left,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            outtext,
            StrLen(outtext),
            NULL
        );
        linerect.left  = SCREENSPLIT1 + 40;
        linerect.right = SCREENSPLIT2;
        SetTextColor(dc, color[colorscheme][COLOR_DATATEXT]);
        if (curraddr <= 0x1FF)
            StrPrintf(outtext, ARRSIZE(outtext), "%02X", (unsigned) * (LPBYTE)(g_mem + curraddr));
        ExtTextOut(
            dc,
            linerect.left,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            outtext,
            StrLen(outtext),
            NULL
        );
        loop++;
    }
}

//===========================================================================
static void DrawTargets(HDC dc, int line) {
    int address[2];
    GetTargets(&address[0], &address[1]);
    int loop = 2;
    while (loop--) {
        if ((address[loop] >= 0xC000) && (address[loop] <= 0xC0FF))
            address[loop] = -1;
        char addressstr[8] = "";
        char valuestr[8]   = "";
        if (address[loop] >= 0) {
            StrPrintf(addressstr, ARRSIZE(addressstr), "%04X", address[loop]);
            if (loop)
                StrPrintf(valuestr, ARRSIZE(valuestr), "%02X", *(LPBYTE)(g_mem + address[loop]));
            else
                StrPrintf(valuestr, ARRSIZE(valuestr), "%04X", *(LPWORD)(g_mem + address[loop]));
        }
        RECT linerect;
        linerect.left   = SCREENSPLIT1;
        linerect.top    = (line + loop) << 4;
        linerect.right  = SCREENSPLIT1 + 40;
        linerect.bottom = linerect.top + 16;
        SetTextColor(dc, color[colorscheme][COLOR_STATIC]);
        SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
        ExtTextOut(
            dc,
            linerect.left,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            addressstr,
            StrLen(addressstr),
            NULL
        );
        linerect.left  = SCREENSPLIT1 + 40;
        linerect.right = SCREENSPLIT2;
        SetTextColor(dc, color[colorscheme][COLOR_DATATEXT]);
        ExtTextOut(
            dc,
            linerect.left,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            valuestr,
            StrLen(valuestr),
            NULL
        );
    }
}

//===========================================================================
static void DrawWatches(HDC dc, int line) {
    RECT linerect;
    linerect.left   = SCREENSPLIT2;
    linerect.top    = (line << 4);
    linerect.right  = 560;
    linerect.bottom = linerect.top + 16;
    char outstr[16] = "Watches";
    SetTextColor(dc, color[colorscheme][COLOR_STATIC]);
    SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
    ExtTextOut(
        dc,
        linerect.left,
        linerect.top,
        ETO_CLIPPED | ETO_OPAQUE,
        &linerect,
        outstr,
        StrLen(outstr),
        NULL
    );

    linerect.right = SCREENSPLIT2 + 64;
    for (int loop = 0; loop < WATCHES; ++loop) {
        if (watch[loop] >= 0)
            StrPrintf(outstr, ARRSIZE(outstr), "%d: %04X", loop + 1, watch[loop]);
        else
            outstr[0] = 0;
        linerect.top += 16;
        linerect.bottom += 16;
        ExtTextOut(
            dc,
            linerect.left,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            outstr,
            StrLen(outstr),
            NULL
        );
    }

    linerect.left   = SCREENSPLIT2 + 64;
    linerect.top    = (line << 4);
    linerect.right  = 560;
    linerect.bottom = linerect.top + 16;
    SetTextColor(dc, color[colorscheme][COLOR_DATATEXT]);
    for (int loop = 0; loop < WATCHES; ++loop) {
        if (watch[loop] >= 0)
            StrPrintf(outstr, ARRSIZE(outstr), "%02X", (unsigned)g_mem[watch[loop]]);
        else
            outstr[0] = 0;
        linerect.top += 16;
        linerect.bottom += 16;
        ExtTextOut(
            dc,
            linerect.left,
            linerect.top,
            ETO_CLIPPED | ETO_OPAQUE,
            &linerect,
            outstr,
            StrLen(outstr),
            NULL
        );
    }
}

//===========================================================================
static BOOL ExecuteCommand(int argc) {
    char * context = NULL;
    char * name = StrTok(commandstring[0], " ,-=", &context);
    if (!name)
        name = commandstring[0];

    int  found    = 0;
    fcmd function = NULL;
    int  length   = StrLen(name);
    int  loop     = 0;
    while ((loop < COMMANDS) && (name[0] >= command[loop].name[0])) {
        if (!StrCmpLen(name, command[loop].name, length)) {
            function = command[loop].function;
            if (!StrCmp(name, command[loop].name)) {
                found = 1;
                break;
            }
            found++;
        }
        loop++;
    }
    if (found > 1)
        return DisplayError("Ambiguous command");
    else if (function)
        return function(argc);
    else
        return DisplayError("Illegal command");
}

//===========================================================================
static void FreeSymbolTable() {
    symtoaddr.clear();
    addrtosym.clear();
}

//===========================================================================
static int GetAddress(const char * symbol) {
    auto iter = symtoaddr.find(symbol);
    if (iter == symtoaddr.end())
        return -1;
    return (int)iter->second;
}

//===========================================================================
static const char * GetSymbol(WORD address, int bytes, char * tmpbuf, int tmpbufsiz) {
    auto iter = addrtosym.find(address);
    if (iter != addrtosym.end())
        return iter->second.c_str();

    // IF THERE IS NO SYMBOL FOR THIS ADDRESS, THEN JUST RETURN A STRING
    // CONTAINING THE ADDRESS NUMBER
    switch (bytes) {
        case 2:
            StrPrintf(tmpbuf, tmpbufsiz, "$%02X", (unsigned)address);
            break;
        case 3:
            StrPrintf(tmpbuf, tmpbufsiz, "$%04X", (unsigned)address);
            break;
        default:
            tmpbuf[0] = '\0';
            break;
    }
    return tmpbuf;
}

//===========================================================================
static void GetTargets(int * intermediate, int * final) {
    *intermediate = -1;
    *final        = -1;
    int  addrmode   = instruction[g_mem[regs.pc]].addrmode;
    BYTE argument8  = *(LPBYTE)(g_mem + regs.pc + 1);
    WORD argument16 = *(LPWORD)(g_mem + regs.pc + 1);
    switch (addrmode) {
        case ADDR_ABS:
            *final = argument16;
            break;

        case ADDR_IABSX:
            argument16 += regs.x;
            *intermediate = argument16;
            *final = *(LPWORD)(g_mem + *intermediate);
            break;

        case ADDR_ABSX:
            argument16 += regs.x;
            *final = argument16;
            break;

        case ADDR_ABSY:
            argument16 += regs.y;
            *final = argument16;
            break;

        case ADDR_IABS:
            *intermediate = argument16;
            *final = *(LPWORD)(g_mem + *intermediate);
            break;

        case ADDR_INDX:
            argument8 += regs.x;
            *intermediate = argument8;
            *final = *(LPWORD)(g_mem + *intermediate);
            break;

        case ADDR_INDY:
            *intermediate = argument8;
            *final = (*(LPWORD)(g_mem + *intermediate)) + regs.y;
            break;

        case ADDR_IZPG:
            *intermediate = argument8;
            *final = *(LPWORD)(g_mem + *intermediate);
            break;

        case ADDR_ZPG:
            *final = argument8;
            break;

        case ADDR_ZPGX:
            *final = argument8 + regs.x;
            break;

        case ADDR_ZPGY:
            *final = argument8 + regs.y;
            break;
    }

    if (*final >= 0 && (!StrCmp(instruction[g_mem[regs.pc]].mnemonic, "JMP") ||
        !StrCmp(instruction[g_mem[regs.pc]].mnemonic, "JSR")))
    {
        *final = -1;
    }
}

//===========================================================================
static BOOL InternalSingleStep() {
    BOOL result = 0;
    _try {
        ++profiledata[g_mem[regs.pc]];
        g_cyclesEmulated += CpuStep6502();
        result = 1;
    }
    _except(EXCEPTION_EXECUTE_HANDLER) {
        result = 0;
    }
    return result;
}

//===========================================================================
static void OutputTraceLine() {
    char disassembly[50];
    char flags[9];
    DrawDisassembly((HDC)0, 0, regs.pc, disassembly, ARRSIZE(disassembly));
    DrawFlags((HDC)0, 0, regs.ps, flags, ARRSIZE(flags));
    fprintf(
        tracefile,
        "a=%02x x=%02x y=%02x sp=%03x ps=%s %s\n",
        (unsigned)regs.a,
        (unsigned)regs.x,
        (unsigned)regs.y,
        (unsigned)regs.sp,
        flags,
        disassembly
    );
}

//===========================================================================
static int ParseCommandString() {
    int    argc = 0;
    char * currptr = commandstring[0];
    char * context = NULL;
    while (*currptr) {
        char * endptr = NULL;
        int    length = StrLen(currptr);
        StrTok(currptr, " ,-=", &context);
        StrCopy(argv[argc].str, currptr, ARRSIZE(argv[argc].str));
        argv[argc].val1 = (WORD)StrToUnsigned(currptr, &endptr, 16);
        if (endptr) {
            if (*endptr == 'L') {
                argv[argc].val2 = (WORD)StrToUnsigned(endptr + 1, &endptr, 16);
                if (endptr && *endptr)
                    argv[argc].val2 = 0;
            }
            else {
                argv[argc].val2 = 0;
                if (*endptr)
                    argv[argc].val1 = 0;
            }
        }
        else
            argv[argc].val2 = 0;

        int toklength = StrLen(currptr);
        int more = *currptr && length > toklength ? 1 : 0;
        ++argc;
        currptr += toklength + more;
    }

    for (int loop = argc; loop < MAXARGS; loop++) {
        argv[loop].str[0] = '\0';
        argv[loop].val1 = 0;
        argv[loop].val2 = 0;
    }

    return argc;
}

//===========================================================================
static void ParseSymbolData(const uint8_t * data, int bytes) {
    const uint8_t * term = data + bytes;
    while (data < term) {
        while (data < term && CharIsWhitespace(*data))
            ++data;

        char addr[16];
        char * addrptr  = addr;
        char * addrterm = addr + ARRSIZE(addr) - 1;
        while (data < term && !CharIsWhitespace(*data)) {
            if (addrptr < addrterm)
                *addrptr++ = *data;
            ++data;
        }
        if (addrptr == addr)
            break;
        *addrptr = '\0';

        while (data < term && CharIsWhitespace(*data))
            ++data;

        const uint8_t * symptr = data;
        while (data < term && !CharIsWhitespace(*data))
            ++data;
        if (symptr == data)
            continue;
        std::string symbol((const char *)symptr, (int)(data - symptr));

        bool error = false;
        WORD addrvalue = 0;
        for (char * addrptr = addr; *addrptr; ++addrptr) {
            addrvalue = addrvalue << 4;
            if (*addrptr >= '0' && *addrptr <= '9')
                addrvalue += *addrptr - '0';
            else if (*addrptr >= 'A' && *addrptr <= 'F')
                addrvalue += *addrptr - 'A' + 10;
            else if (*addrptr >= 'a' && *addrptr <= 'f')
                addrvalue += *addrptr - 'a' + 10;
            else
                error = true;
        }
        if (error)
            continue;

        if (addrvalue == 0) {
            int foo = 0;
        }
        symtoaddr[symbol] = addrvalue;
        addrtosym[addrvalue] = std::move(symbol);
    }
}

//===========================================================================
static void WriteProfileData() {
    char filename[MAX_PATH];
    StrCopy(filename, EmulatorGetProgramDirectory(), ARRSIZE(filename));
    StrCat(filename, "Profile.txt", ARRSIZE(filename));
    FILE * file = fopen(filename, "wt");
    if (file) {
        DWORD maxvalue;
        do {
            maxvalue = 0;
            DWORD maxitem = 0;
            for (int loop = 0; loop < 256; ++loop) {
                if (profiledata[loop] > maxvalue) {
                    maxvalue = profiledata[loop];
                    maxitem = loop;
                }
            }
            if (maxvalue) {
                fprintf(
                    file,
                    "%9u %02X %s\n",
                    (unsigned)maxvalue,
                    (unsigned)maxitem,
                    instruction[maxitem].mnemonic
                );
                profiledata[maxitem] = 0;
            }
        } while (maxvalue);
        fclose(file);
    }
}


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
void DebugAdvance() {
    static unsigned stepstaken = 0;
    if (stepcount) {
        if (tracefile)
            OutputTraceLine();
        lastpc = regs.pc;
        InternalSingleStep();
        if (regs.pc == stepuntil || CheckBreakpoint(regs.pc, 1))
            stepcount = 0;
        else if (stepcount > 0)
            stepcount--;
    }
    if (stepcount) {
        if ((++stepstaken & 0xFFFF) == 0)
            VideoRefreshScreen();
    }
    else {
        EmulatorSetMode(EMULATOR_MODE_DEBUG);
        if (stepstart < regs.pc && stepstart + 3 >= regs.pc)
            topoffset += addressmode[instruction[g_mem[topoffset]].addrmode].bytes;
        else
            ComputeTopOffset(regs.pc);
        DebugDisplay(stepstaken >= 0x10000);
        stepstaken = 0;
    }
}

//===========================================================================
void DebugBegin() {
    if (!membank)
        membank = g_mem;
    EmulatorSetMode(EMULATOR_MODE_DEBUG);
    addressmode[INVALID2].bytes = CpuGetType() == CPU_TYPE_65C02 ? 2 : 1;
    addressmode[INVALID3].bytes = CpuGetType() == CPU_TYPE_65C02 ? 3 : 1;
    ComputeTopOffset(regs.pc);
    DebugDisplay(TRUE);
}

//===========================================================================
void DebugDestroy() {
    DebugEnd();
    DeleteObject(debugfont);
    FreeSymbolTable();
}

//===========================================================================
void DebugDisplay(BOOL drawbackground) {
    HDC dc = FrameGetDC();
    SelectObject(dc, debugfont);
    SetTextAlign(dc, TA_TOP | TA_LEFT);

    // DRAW THE BACKGROUND
    if (drawbackground) {
        RECT viewportrect;
        viewportrect.left   = 0;
        viewportrect.top    = 0;
        viewportrect.right  = SCREENSPLIT1 - 14;
        viewportrect.bottom = 304;
        SetBkColor(dc, color[colorscheme][COLOR_INSTBKG]);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &viewportrect, "", 0, NULL);
        viewportrect.left  = SCREENSPLIT1 - 14;
        viewportrect.right = 560;
        SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &viewportrect, "", 0, NULL);
    }

    // DRAW DISASSEMBLED LINES
    {
        WORD offset = topoffset;
        for (int line = 0; line < SOURCELINES; ++line)
            offset += DrawDisassembly(dc, line, offset, NULL, 0);
    }

    // DRAW THE DATA AREA
    DrawStack(dc, 0);
    DrawTargets(dc, 10);
    DrawRegister(dc, 13, "A", 1, regs.a);
    DrawRegister(dc, 14, "X", 1, regs.x);
    DrawRegister(dc, 15, "Y", 1, regs.y);
    DrawRegister(dc, 16, "PC", 2, regs.pc);
    DrawRegister(dc, 17, "SP", 2, regs.sp);
    DrawFlags(dc, 18, regs.ps, NULL, 0);
    if (usingbp)
        DrawBreakpoints(dc, 0);
    if (usingwatches)
        DrawWatches(dc, 7);
    if (usingmemdump)
        DrawMemory(dc, 14);

    // DRAW THE COMMAND LINE
    for (int line = COMMANDLINES - 1; line >= 0; --line)
        DrawCommandLine(dc, line);

    FrameReleaseDC(dc);
}

//===========================================================================
void DebugEnd() {
    if (profiling)
        WriteProfileData();
    if (tracefile) {
        fclose(tracefile);
        tracefile = NULL;
    }
}

//===========================================================================
void DebugInitialize() {
    ZeroMemory(breakpoint, BREAKPOINTS * sizeof(bp));
    for (int loop = 0; loop < WATCHES; ++loop)
        watch[loop] = -1;

    int bytes;
    const uint8_t * resource = (const uint8_t *)ResourceLoad("APPLE2E_SYM", "SYMBOLS", &bytes);
    if (resource) {
        ParseSymbolData(resource, bytes);
        ResourceFree(resource);
    }

    debugfont = CreateFont(
        15,
        0,
        0,
        0,
        FW_MEDIUM,
        0,
        0,
        0,
        OEM_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH | 4 | FF_MODERN,
        "Courier New"
    );
}

//===========================================================================
void DebugProcessChar(char ch) {
    if (EmulatorGetMode() != EMULATOR_MODE_DEBUG)
        return;
    if (ch == ' ' && !commandstring[0][0])
        return;
    if (ch >= 32 && ch <= 126) {
        ch = (ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch;
        int length = StrLen(commandstring[0]);
        if (length < 68) {
            commandstring[0][length] = ch;
            commandstring[0][length + 1] = 0;
        }
        HDC dc = FrameGetDC();
        DrawCommandLine(dc, 0);
        FrameReleaseDC(dc);
    }
}

//===========================================================================
void DebugProcessCommand(int keycode) {
    EEmulatorMode mode = EmulatorGetMode();
    if (mode == EMULATOR_MODE_STEPPING && keycode == VK_ESCAPE)
        stepcount = 0;
    if (mode != EMULATOR_MODE_DEBUG)
        return;
    if (viewingoutput) {
        DebugDisplay(TRUE);
        viewingoutput = FALSE;
        return;
    }
    BOOL needscmdrefresh = 0;
    BOOL needsfullrefresh = 0;
    if (keycode == VK_SPACE && commandstring[0][0])
        return;
    if (keycode == VK_BACK) {
        int length = StrLen(commandstring[0]);
        if (length)
            commandstring[0][length - 1] = 0;
        needscmdrefresh = 1;
    }
    else if (keycode == VK_RETURN) {
        if (!commandstring[0][0] && commandstring[1][0] != ' ')
            StrCopy(commandstring[0], commandstring[1], ARRSIZE(commandstring[0]));
        int loop = COMMANDLINES - 1;
        while (loop--)
            StrCopy(commandstring[loop + 1], commandstring[loop], 80);
        needscmdrefresh = COMMANDLINES;
        needsfullrefresh = ExecuteCommand(ParseCommandString());
        commandstring[0][0] = 0;
    }
    else switch (keycode) {
        case VK_SPACE:  needsfullrefresh = CmdTrace(0);     break;
        case VK_PRIOR:  needsfullrefresh = CmdPageUp(0);    break;
        case VK_NEXT:   needsfullrefresh = CmdPageDown(0);  break;
        case VK_UP:     needsfullrefresh = CmdLineUp(0);    break;
        case VK_DOWN:   needsfullrefresh = CmdLineDown(0);  break;
    }
    if (needsfullrefresh)
        DebugDisplay(FALSE);
    else if (needscmdrefresh && !viewingoutput && (mode == EMULATOR_MODE_DEBUG || mode == EMULATOR_MODE_STEPPING)) {
        HDC dc = FrameGetDC();
        while (needscmdrefresh--)
            DrawCommandLine(dc, needscmdrefresh);
        FrameReleaseDC(dc);
    }
}
