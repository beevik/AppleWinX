/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

constexpr int BREAKPOINTS     = 5;
constexpr int COMMANDLINES    = 5;
constexpr int COMMANDS        = 61;
constexpr int MAXARGS         = 40;
constexpr int SOURCELINES     = 19;
constexpr int STACKLINES      = 9;
constexpr int WATCHES         = 5;

constexpr LONG SCREENSPLIT1    = 356;
constexpr LONG SCREENSPLIT2    = 456;

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
constexpr int ADDR_ABSIINDX   = 13;
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

typedef BOOL(*cmdfunction)(int);

static BOOL CmdBlackWhite(int args);
static BOOL CmdBreakpointAdd(int args);
static BOOL CmdBreakpointClear(int args);
static BOOL CmdBreakpointDisable(int args);
static BOOL CmdBreakpointEnable(int args);
static BOOL CmdCodeDump(int args);
static BOOL CmdColor(int args);
static BOOL CmdExtBenchmark(int args);
static BOOL CmdFeedKey(int args);
static BOOL CmdFlagSet(int args);
static BOOL CmdGo(int args);
static BOOL CmdInput(int args);
static BOOL CmdInternalCodeDump(int args);
static BOOL CmdInternalMemoryDump(int args);
static BOOL CmdLineDown(int args);
static BOOL CmdLineUp(int args);
static BOOL CmdMemoryDump(int args);
static BOOL CmdMemoryEnter(int args);
static BOOL CmdMemoryFill(int args);
static BOOL CmdOutput(int args);
static BOOL CmdPageDown(int args);
static BOOL CmdPageUp(int args);
static BOOL CmdProfile(int args);
static BOOL CmdRegisterSet(int args);
static BOOL CmdSetupBenchmark(int args);
static BOOL CmdTrace(int args);
static BOOL CmdTraceFile(int args);
static BOOL CmdTraceLine(int args);
static BOOL CmdViewOutput(int args);
static BOOL CmdWatchAdd(int args);
static BOOL CmdWatchClear(int args);
static BOOL CmdZap(int args);

struct addrrec {
    TCHAR format[12];
    int   bytes;
};

struct argrec {
    TCHAR str[12];
    WORD  val1;
    WORD  val2;
};

struct bprec {
    WORD address;
    WORD length;
    BOOL enabled;
};

struct cmdrec {
    TCHAR       name[12];
    cmdfunction function;
};

struct instrec {
    TCHAR mnemonic[4];
    int   addrmode;
};

struct symbolrec {
    WORD  value;
    TCHAR name[14];
};

static addrrec addressmode[17] = {
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

static const cmdrec command[COMMANDS] = {
    { "BA",           CmdBreakpointAdd        },
    { "BC",           CmdBreakpointClear      },
    { "BD",           CmdBreakpointDisable    },
    { "BE",           CmdBreakpointEnable     },
    { "BENCH",        CmdSetupBenchmark       },
    { "BPM",          CmdBreakpointAdd        },
    { "BW",           CmdBlackWhite           },
    { "COLOR",        CmdColor                },
    { "D",            CmdMemoryDump           },
    { "EXTBENCH",     CmdExtBenchmark         },
    { "GOTO",         CmdGo                   },
    { "I",            CmdInput                },
    { "ICD",          CmdInternalCodeDump     },
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

static const instrec instruction[256] = {
    { "BRK", 0                },  // 00h
    { "ORA", ADDR_INDX        },  // 01h
    { "NOP", INVALID2         },  // 02h
    { "NOP", INVALID1         },  // 03h
    { "TSB", ADDR_ZPG         },  // 04h
    { "ORA", ADDR_ZPG         },  // 05h
    { "ASL", ADDR_ZPG         },  // 06h
    { "NOP", INVALID1         },  // 07h
    { "PHP", 0                },  // 08h
    { "ORA", ADDR_IMM         },  // 09h
    { "ASL", 0                },  // 0Ah
    { "NOP", INVALID1         },  // 0Bh
    { "TSB", ADDR_ABS         },  // 0Ch
    { "ORA", ADDR_ABS         },  // 0Dh
    { "ASL", ADDR_ABS         },  // 0Eh
    { "NOP", INVALID1         },  // 0Fh
    { "BPL", ADDR_REL         },  // 10h
    { "ORA", ADDR_INDY        },  // 11h
    { "ORA", ADDR_IZPG        },  // 12h
    { "NOP", INVALID1         },  // 13h
    { "TRB", ADDR_ZPG         },  // 14h
    { "ORA", ADDR_ZPGX        },  // 15h
    { "ASL", ADDR_ZPGX        },  // 16h
    { "NOP", INVALID1         },  // 17h
    { "CLC", 0                },  // 18h
    { "ORA", ADDR_ABSY        },  // 19h
    { "INA", 0                },  // 1Ah
    { "NOP", INVALID1         },  // 1Bh
    { "TRB", ADDR_ABS         },  // 1Ch
    { "ORA", ADDR_ABSX        },  // 1Dh
    { "ASL", ADDR_ABSX        },  // 1Eh
    { "NOP", INVALID1         },  // 1Fh
    { "JSR", ADDR_ABS         },  // 20h
    { "AND", ADDR_INDX        },  // 21h
    { "NOP", INVALID2         },  // 22h
    { "NOP", INVALID1         },  // 23h
    { "BIT", ADDR_ZPG         },  // 24h
    { "AND", ADDR_ZPG         },  // 25h
    { "ROL", ADDR_ZPG         },  // 26h
    { "NOP", INVALID1         },  // 27h
    { "PLP", 0                },  // 28h
    { "AND", ADDR_IMM         },  // 29h
    { "ROL", 0                },  // 2Ah
    { "NOP", INVALID1         },  // 2Bh
    { "BIT", ADDR_ABS         },  // 2Ch
    { "AND", ADDR_ABS         },  // 2Dh
    { "ROL", ADDR_ABS         },  // 2Eh
    { "NOP", INVALID1         },  // 2Fh
    { "BMI", ADDR_REL         },  // 30h
    { "AND", ADDR_INDY        },  // 31h
    { "AND", ADDR_IZPG        },  // 32h
    { "NOP", INVALID1         },  // 33h
    { "BIT", ADDR_ZPGX        },  // 34h
    { "AND", ADDR_ZPGX        },  // 35h
    { "ROL", ADDR_ZPGX        },  // 36h
    { "NOP", INVALID1         },  // 37h
    { "SEC", 0                },  // 38h
    { "AND", ADDR_ABSY        },  // 39h
    { "DEA", 0                },  // 3Ah
    { "NOP", INVALID1         },  // 3Bh
    { "BIT", ADDR_ABSX        },  // 3Ch
    { "AND", ADDR_ABSX        },  // 3Dh
    { "ROL", ADDR_ABSX        },  // 3Eh
    { "NOP", INVALID1         },  // 3Fh
    { "RTI", 0                },  // 40h
    { "EOR", ADDR_INDX        },  // 41h
    { "NOP", INVALID2         },  // 42h
    { "NOP", INVALID1         },  // 43h
    { "NOP", INVALID2         },  // 44h
    { "EOR", ADDR_ZPG         },  // 45h
    { "LSR", ADDR_ZPG         },  // 46h
    { "NOP", INVALID1         },  // 47h
    { "PHA", 0                },  // 48h
    { "EOR", ADDR_IMM         },  // 49h
    { "LSR", 0                },  // 4Ah
    { "NOP", INVALID1         },  // 4Bh
    { "JMP", ADDR_ABS         },  // 4Ch
    { "EOR", ADDR_ABS         },  // 4Dh
    { "LSR", ADDR_ABS         },  // 4Eh
    { "NOP", INVALID1         },  // 4Fh
    { "BVC", ADDR_REL         },  // 50h
    { "EOR", ADDR_INDY        },  // 51h
    { "EOR", ADDR_IZPG        },  // 52h
    { "NOP", INVALID1         },  // 53h
    { "NOP", INVALID2         },  // 54h
    { "EOR", ADDR_ZPGX        },  // 55h
    { "LSR", ADDR_ZPGX        },  // 56h
    { "NOP", INVALID1         },  // 57h
    { "CLI", 0                },  // 58h
    { "EOR", ADDR_ABSY        },  // 59h
    { "PHY", 0                },  // 5Ah
    { "NOP", INVALID1         },  // 5Bh
    { "NOP", INVALID3         },  // 5Ch
    { "EOR", ADDR_ABSX        },  // 5Dh
    { "LSR", ADDR_ABSX        },  // 5Eh
    { "NOP", INVALID1         },  // 5Fh
    { "RTS", 0                },  // 60h
    { "ADC", ADDR_INDX        },  // 61h
    { "NOP", INVALID2         },  // 62h
    { "NOP", INVALID1         },  // 63h
    { "STZ", ADDR_ZPG         },  // 64h
    { "ADC", ADDR_ZPG         },  // 65h
    { "ROR", ADDR_ZPG         },  // 66h
    { "NOP", INVALID1         },  // 67h
    { "PLA", 0                },  // 68h
    { "ADC", ADDR_IMM         },  // 69h
    { "ROR", 0                },  // 6Ah
    { "NOP", INVALID1         },  // 6Bh
    { "JMP", ADDR_IABS        },  // 6Ch
    { "ADC", ADDR_ABS         },  // 6Dh
    { "ROR", ADDR_ABS         },  // 6Eh
    { "NOP", INVALID1         },  // 6Fh
    { "BVS", ADDR_REL         },  // 70h
    { "ADC", ADDR_INDY        },  // 71h
    { "ADC", ADDR_IZPG        },  // 72h
    { "NOP", INVALID1         },  // 73h
    { "STZ", ADDR_ZPGX        },  // 74h
    { "ADC", ADDR_ZPGX        },  // 75h
    { "ROR", ADDR_ZPGX        },  // 76h
    { "NOP", INVALID1         },  // 77h
    { "SEI", 0                },  // 78h
    { "ADC", ADDR_ABSY        },  // 79h
    { "PLY", 0                },  // 7Ah
    { "NOP", INVALID1         },  // 7Bh
    { "JMP", ADDR_ABSIINDX    },  // 7Ch
    { "ADC", ADDR_ABSX        },  // 7Dh
    { "ROR", ADDR_ABSX        },  // 7Eh
    { "NOP", INVALID1         },  // 7Fh
    { "BRA", ADDR_REL         },  // 80h
    { "STA", ADDR_INDX        },  // 81h
    { "NOP", INVALID2         },  // 82h
    { "NOP", INVALID1         },  // 83h
    { "STY", ADDR_ZPG         },  // 84h
    { "STA", ADDR_ZPG         },  // 85h
    { "STX", ADDR_ZPG         },  // 86h
    { "NOP", INVALID1         },  // 87h
    { "DEY", 0                },  // 88h
    { "BIT", ADDR_IMM         },  // 89h
    { "TXA", 0                },  // 8Ah
    { "NOP", INVALID1         },  // 8Bh
    { "STY", ADDR_ABS         },  // 8Ch
    { "STA", ADDR_ABS         },  // 8Dh
    { "STX", ADDR_ABS         },  // 8Eh
    { "NOP", INVALID1         },  // 8Fh
    { "BCC", ADDR_REL         },  // 90h
    { "STA", ADDR_INDY        },  // 91h
    { "STA", ADDR_IZPG        },  // 92h
    { "NOP", INVALID1         },  // 93h
    { "STY", ADDR_ZPGX        },  // 94h
    { "STA", ADDR_ZPGX        },  // 95h
    { "STX", ADDR_ZPGY        },  // 96h
    { "NOP", INVALID1         },  // 97h
    { "TYA", 0                },  // 98h
    { "STA", ADDR_ABSY        },  // 99h
    { "TXS", 0                },  // 9Ah
    { "NOP", INVALID1         },  // 9Bh
    { "STZ", ADDR_ABS         },  // 9Ch
    { "STA", ADDR_ABSX        },  // 9Dh
    { "STZ", ADDR_ABSX        },  // 9Eh
    { "NOP", INVALID1         },  // 9Fh
    { "LDY", ADDR_IMM         },  // A0h
    { "LDA", ADDR_INDX        },  // A1h
    { "LDX", ADDR_IMM         },  // A2h
    { "NOP", INVALID1         },  // A3h
    { "LDY", ADDR_ZPG         },  // A4h
    { "LDA", ADDR_ZPG         },  // A5h
    { "LDX", ADDR_ZPG         },  // A6h
    { "NOP", INVALID1         },  // A7h
    { "TAY", 0                },  // A8h
    { "LDA", ADDR_IMM         },  // A9h
    { "TAX", 0                },  // AAh
    { "NOP", INVALID1         },  // ABh
    { "LDY", ADDR_ABS         },  // ACh
    { "LDA", ADDR_ABS         },  // ADh
    { "LDX", ADDR_ABS         },  // AEh
    { "NOP", INVALID1         },  // AFh
    { "BCS", ADDR_REL         },  // B0h
    { "LDA", ADDR_INDY        },  // B1h
    { "LDA", ADDR_IZPG        },  // B2h
    { "NOP", INVALID1         },  // B3h
    { "LDY", ADDR_ZPGX        },  // B4h
    { "LDA", ADDR_ZPGX        },  // B5h
    { "LDX", ADDR_ZPGY        },  // B6h
    { "NOP", INVALID1         },  // B7h
    { "CLV", 0                },  // B8h
    { "LDA", ADDR_ABSY        },  // B9h
    { "TSX", 0                },  // BAh
    { "NOP", INVALID1         },  // BBh
    { "LDY", ADDR_ABSX        },  // BCh
    { "LDA", ADDR_ABSX        },  // BDh
    { "LDX", ADDR_ABSY        },  // BEh
    { "NOP", INVALID1         },  // BFh
    { "CPY", ADDR_IMM         },  // C0h
    { "CMP", ADDR_INDX        },  // C1h
    { "NOP", INVALID2         },  // C2h
    { "NOP", INVALID1         },  // C3h
    { "CPY", ADDR_ZPG         },  // C4h
    { "CMP", ADDR_ZPG         },  // C5h
    { "DEC", ADDR_ZPG         },  // C6h
    { "NOP", INVALID1         },  // C7h
    { "INY", 0                },  // C8h
    { "CMP", ADDR_IMM         },  // C9h
    { "DEX", 0                },  // CAh
    { "NOP", INVALID1         },  // CBh
    { "CPY", ADDR_ABS         },  // CCh
    { "CMP", ADDR_ABS         },  // CDh
    { "DEC", ADDR_ABS         },  // CEh
    { "NOP", INVALID1         },  // CFh
    { "BNE", ADDR_REL         },  // D0h
    { "CMP", ADDR_INDY        },  // D1h
    { "CMP", ADDR_IZPG        },  // D2h
    { "NOP", INVALID1         },  // D3h
    { "NOP", INVALID2         },  // D4h
    { "CMP", ADDR_ZPGX        },  // D5h
    { "DEC", ADDR_ZPGX        },  // D6h
    { "NOP", INVALID1         },  // D7h
    { "CLD", 0                },  // D8h
    { "CMP", ADDR_ABSY        },  // D9h
    { "PHX", 0                },  // DAh
    { "NOP", INVALID1         },  // DBh
    { "NOP", INVALID3         },  // DCh
    { "CMP", ADDR_ABSX        },  // DDh
    { "DEC", ADDR_ABSX        },  // DEh
    { "NOP", INVALID1         },  // DFh
    { "CPX", ADDR_IMM         },  // E0h
    { "SBC", ADDR_INDX        },  // E1h
    { "NOP", INVALID2         },  // E2h
    { "NOP", INVALID1         },  // E3h
    { "CPX", ADDR_ZPG         },  // E4h
    { "SBC", ADDR_ZPG         },  // E5h
    { "INC", ADDR_ZPG         },  // E6h
    { "NOP", INVALID1         },  // E7h
    { "INX", 0                },  // E8h
    { "SBC", ADDR_IMM         },  // E9h
    { "NOP", 0                },  // EAh
    { "NOP", INVALID1         },  // EBh
    { "CPX", ADDR_ABS         },  // ECh
    { "SBC", ADDR_ABS         },  // EDh
    { "INC", ADDR_ABS         },  // EEh
    { "NOP", INVALID1         },  // EFh
    { "BEQ", ADDR_REL         },  // F0h
    { "SBC", ADDR_INDY        },  // F1h
    { "SBC", ADDR_IZPG        },  // F2h
    { "NOP", INVALID1         },  // F3h
    { "NOP", INVALID2         },  // F4h
    { "SBC", ADDR_ZPGX        },  // F5h
    { "INC", ADDR_ZPGX        },  // F6h
    { "NOP", INVALID1         },  // F7h
    { "SED", 0                },  // F8h
    { "SBC", ADDR_ABSY        },  // F9h
    { "PLX", 0                },  // FAh
    { "NOP", INVALID1         },  // FBh
    { "NOP", INVALID3         },  // FCh
    { "SBC", ADDR_ABSX        },  // FDh
    { "INC", ADDR_ABSX        },  // FEh
    { "NOP", INVALID1         },  // FFh
};

static const COLORREF color[2][COLORS] = {
    {
        0x800000,
        0xC0C0C0,
        0x00FFFF,
        0x808000,
        0x000080,
        0x800000,
        0x00FFFF,
        0xFFFFFF
    },
    {
        0x000000,
        0xC0C0C0,
        0xFFFFFF,
        0x000000,
        0xC0C0C0,
        0x808080,
        0xFFFFFF,
        0xFFFFFF
    },
};

static TCHAR commandstring[COMMANDLINES][80] = {
    "",
    " ",
    " Apple //e Emulator for Windows",
    " ",
    " "
};

DWORD extbench = 0;

static argrec      arg[MAXARGS];
static bprec       breakpoint[BREAKPOINTS];
static DWORD       profiledata[256];
static int         watch[WATCHES];

static int         colorscheme   = 0;
static HFONT       debugfont     = (HFONT)0;
static BOOL        fulldisp      = FALSE;
static WORD        lastpc        = 0;
static LPBYTE      membank       = NULL;
static WORD        memorydump    = 0;
static BOOL        profiling     = FALSE;
static int         stepcount     = 0;
static BOOL        stepline      = FALSE;
static int         stepstart     = 0;
static int         stepuntil     = -1;
static symbolrec * symboltable   = NULL;
static int         symbolnum     = 0;
static WORD        topoffset     = 0;
static FILE *      tracefile     = NULL;
static BOOL        usingbp       = FALSE;
static BOOL        usingmemdump  = FALSE;
static BOOL        usingwatches  = FALSE;
static BOOL        viewingoutput = FALSE;

static void ComputeTopOffset(WORD centeroffset);
static BOOL DisplayError(LPCTSTR errortext);
static BOOL DisplayHelp(cmdfunction function);
static BOOL InternalSingleStep();
static WORD GetAddress(LPCTSTR symbol);
static LPCTSTR GetSymbol(WORD address, int bytes);
static void GetTargets(int * intermediate, int * final);

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
                    return 1;
                slot++;
            }
        }
    return 0;
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
static BOOL CmdBlackWhite(int args) {
    colorscheme = 1;
    DebugDisplay(1);
    return 0;
}

//===========================================================================
static BOOL CmdBreakpointAdd(int args) {
    if (!args)
        arg[args = 1].val1 = regs.pc;
    BOOL addedone = 0;
    int  loop = 0;
    while (loop++ < args)
        if (arg[loop].val1 || (arg[loop].str[0] == '0') ||
            GetAddress(arg[loop].str)) {
            if (!arg[loop].val1)
                arg[loop].val1 = GetAddress(arg[loop].str);

              // FIND A FREE SLOT FOR THIS NEW BREAKPOINT
            int freeslot = 0;
            while ((freeslot < BREAKPOINTS) && breakpoint[freeslot].length)
                freeslot++;
            if ((freeslot >= BREAKPOINTS) && !addedone)
                return DisplayError("All breakpoint slots are currently in use.");

              // ADD THE BREAKPOINT
            if (freeslot < BREAKPOINTS) {
                breakpoint[freeslot].address = arg[loop].val1;
                breakpoint[freeslot].length = arg[loop].val2
                    ? MIN(0x10000 - arg[loop].val1, arg[loop].val2)
                    : 1;
                breakpoint[freeslot].enabled = 1;
                addedone = 1;
                usingbp = 1;
            }

        }
    if (!addedone)
        return DisplayHelp(CmdBreakpointAdd);
    return 1;
}

//===========================================================================
static BOOL CmdBreakpointClear(int args) {

    // CHECK FOR ERRORS
    if (!args)
        return DisplayHelp(CmdBreakpointClear);
    if (!usingbp)
        return DisplayError("There are no breakpoints defined.");

    // CLEAR EACH BREAKPOINT IN THE LIST
    while (args) {
        if (!StrCmp(arg[args].str, "*")) {
            int loop = BREAKPOINTS;
            while (loop--)
                breakpoint[loop].length = 0;
        }
        else if ((arg[args].val1 >= 1) && (arg[args].val1 <= BREAKPOINTS))
            breakpoint[arg[args].val1 - 1].length = 0;
        args--;
    }

    // IF THERE ARE NO MORE BREAKPOINTS DEFINED, DISABLE THE BREAKPOINT
    // FUNCTIONALITY AND ERASE THE BREAKPOINT DISPLAY FROM THE SCREEN
    int usedslot = 0;
    while ((usedslot < BREAKPOINTS) && !breakpoint[usedslot].length)
        usedslot++;
    if (usedslot >= BREAKPOINTS) {
        usingbp = 0;
        DebugDisplay(1);
        return 0;
    }
    else
        return 1;

}

//===========================================================================
static BOOL CmdBreakpointDisable(int args) {

    // CHECK FOR ERRORS
    if (!args)
        return DisplayHelp(CmdBreakpointDisable);
    if (!usingbp)
        return DisplayError("There are no breakpoints defined.");

    // DISABLE EACH BREAKPOINT IN THE LIST
    while (args) {
        if (!StrCmp(arg[args].str, "*")) {
            int loop = BREAKPOINTS;
            while (loop--)
                breakpoint[loop].enabled = 0;
        }
        else if ((arg[args].val1 >= 1) && (arg[args].val1 <= BREAKPOINTS))
            breakpoint[arg[args].val1 - 1].enabled = 0;
        args--;
    }

    return 1;
}

//===========================================================================
static BOOL CmdBreakpointEnable(int args) {

    // CHECK FOR ERRORS
    if (!args)
        return DisplayHelp(CmdBreakpointEnable);
    if (!usingbp)
        return DisplayError("There are no breakpoints defined.");

    // ENABLE EACH BREAKPOINT IN THE LIST
    while (args) {
        if (!StrCmp(arg[args].str, "*")) {
            int loop = BREAKPOINTS;
            while (loop--)
                breakpoint[loop].enabled = 1;
        }
        else if ((arg[args].val1 >= 1) && (arg[args].val1 <= BREAKPOINTS))
            breakpoint[arg[args].val1 - 1].enabled = 1;
        args--;
    }

    return 1;
}

//===========================================================================
static BOOL CmdCodeDump(int args) {
    if ((!args) ||
        ((arg[1].str[0] != '0') && (!arg[1].val1) && (!GetAddress(arg[1].str))))
        return DisplayHelp(CmdCodeDump);
    topoffset = arg[1].val1;
    if (!topoffset)
        topoffset = GetAddress(arg[1].str);
    return 1;
}

//===========================================================================
static BOOL CmdColor(int args) {
    colorscheme = 0;
    DebugDisplay(1);
    return 0;
}

//===========================================================================
static BOOL CmdExtBenchmark(int args) {
    DebugEnd();
    mode = MODE_RUNNING;
    VideoRedrawScreen();
    DWORD currtime = GetTickCount();
    while ((extbench = GetTickCount()) != currtime);
    KeybQueueKeypress(VK_SPACE, 0);
    resettiming = 1;
    return 0;
}

//===========================================================================
static BOOL CmdFeedKey(int args) {
    KeybQueueKeypress(args ? arg[1].val1 ? arg[1].val1
        : arg[1].str[0]
        : VK_SPACE,
        0);
    return 0;
}

//===========================================================================
static BOOL CmdFlagSet(int args) {
    static const TCHAR flagname[] = "CZIDBRVN";
    int loop = 0;
    while (loop < 8)
        if (flagname[loop] == arg[0].str[1])
            break;
        else
            loop++;
    if (loop < 8)
        if (arg[0].str[0] == 'R')
            regs.ps &= ~(1 << loop);
        else
            regs.ps |= (1 << loop);
    return 1;
}

//===========================================================================
static BOOL CmdGo(int args) {
    stepcount = -1;
    stepline = 0;
    stepstart = regs.pc;
    stepuntil = args ? arg[1].val1 : -1;
    if (!stepuntil)
        stepuntil = GetAddress(arg[1].str);
    mode = MODE_STEPPING;
    return 0;
}

//===========================================================================
static BOOL CmdInput(int args) {
    if ((!args) ||
        ((arg[1].str[0] != '0') && (!arg[1].val1) && (!GetAddress(arg[1].str))))
        return DisplayHelp(CmdInput);
    if (!arg[1].val1)
        arg[1].val1 = GetAddress(arg[1].str);
    ioread[arg[1].val1 & 0xFF](regs.pc, arg[1].val1 & 0xFF, 0, 0);
    return 1;
}

//===========================================================================
static BOOL CmdInternalCodeDump(int args) {
    if ((!args) ||
        ((arg[1].str[0] != '0') && (!arg[1].val1) && (!GetAddress(arg[1].str))))
        return DisplayHelp(CmdInternalCodeDump);
    if (!arg[1].val1)
        arg[1].val1 = GetAddress(arg[1].str);
    TCHAR filename[MAX_PATH];
    StrCopy(filename, progdir, ARRSIZE(filename));
    StrCat(filename, "output.bin", ARRSIZE(filename));
    HANDLE file = CreateFile(filename,
        GENERIC_WRITE,
        0,
        (LPSECURITY_ATTRIBUTES)NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    if (file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
    return 0;
}

//===========================================================================
static BOOL CmdInternalMemoryDump(int args) {
    TCHAR filename[MAX_PATH];
    StrCopy(filename, progdir, ARRSIZE(filename));
    StrCat(filename, "output.bin", ARRSIZE(filename));
    HANDLE file = CreateFile(filename,
        GENERIC_WRITE,
        0,
        (LPSECURITY_ATTRIBUTES)NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD byteswritten;
        WriteFile(file,
            mem,
            0x30000,
            &byteswritten,
            NULL);
        CloseHandle(file);
    }
    return 0;
}

//===========================================================================
static BOOL CmdLineDown(int args) {
    topoffset += addressmode[instruction[*(mem + topoffset)].addrmode].bytes;
    return 1;
}

//===========================================================================
static BOOL CmdLineUp(int args) {
    WORD savedoffset = topoffset;
    ComputeTopOffset(topoffset);
    WORD newoffset = topoffset;
    while (newoffset < savedoffset) {
        topoffset = newoffset;
        newoffset += addressmode[instruction[*(mem + newoffset)].addrmode].bytes;
    }
    topoffset = MIN(topoffset, savedoffset - 1);
    return 1;
}

//===========================================================================
static BOOL CmdMemoryDump(int args) {
    if ((!args) ||
        ((arg[1].str[0] != '0') && (!arg[1].val1) && (!GetAddress(arg[1].str))))
        return DisplayHelp(CmdMemoryDump);
    memorydump = arg[1].val1;
    if (!memorydump)
        memorydump = GetAddress(arg[1].str);
    usingmemdump = 1;
    return 1;
}

//===========================================================================
static BOOL CmdMemoryEnter(int args) {
    if ((args < 2) ||
        ((arg[1].str[0] != '0') && (!arg[1].val1) && (!GetAddress(arg[1].str))) ||
        ((arg[2].str[0] != '0') && !arg[2].val1))
        return DisplayHelp(CmdMemoryEnter);
    WORD address = arg[1].val1;
    if (!address)
        address = GetAddress(arg[1].str);
    while (args >= 2) {
        *(membank + address + args - 2) = (BYTE)arg[args].val1;
        *(memdirty + (address >> 8)) = 1;
        args--;
    }
    return 1;
}

//===========================================================================
static BOOL CmdMemoryFill(int args) {
    if ((!args) ||
        ((arg[1].str[0] != '0') && (!arg[1].val1) && (!GetAddress(arg[1].str))))
        return DisplayHelp(CmdMemoryFill);
    WORD address = arg[1].val1 ? arg[1].val1 : GetAddress(arg[1].str);
    WORD bytes = MAX(1, arg[1].val2);
    while (bytes--) {
        if ((address < 0xC000) || (address > 0xC0FF))
            * (membank + address) = (BYTE)(arg[2].val1 & 0xFF);
        address++;
    }
    return 1;
}

//===========================================================================
static BOOL CmdOutput(int args) {
    if ((!args) ||
        ((arg[1].str[0] != '0') && (!arg[1].val1) && (!GetAddress(arg[1].str))))
        return DisplayHelp(CmdInput);
    if (!arg[1].val1)
        arg[1].val1 = GetAddress(arg[1].str);
    iowrite[arg[1].val1 & 0xFF](regs.pc, arg[1].val1 & 0xFF, 1, arg[2].val1 & 0xFF);
    return 1;
}

//===========================================================================
static BOOL CmdPageDown(int args) {
    int loop = 0;
    while (loop++ < SOURCELINES)
        CmdLineDown(args);
    return 1;
}

//===========================================================================
static BOOL CmdPageUp(int args) {
    int loop = 0;
    while (loop++ < SOURCELINES)
        CmdLineUp(args);
    return 1;
}

//===========================================================================
static BOOL CmdProfile(int args) {
    ZeroMemory(profiledata, 256 * sizeof(DWORD));
    profiling = 1;
    return 0;
}

//===========================================================================
static BOOL CmdSetupBenchmark(int args) {
    CpuSetupBenchmark();
    ComputeTopOffset(regs.pc);
    return 1;
}

//===========================================================================
static BOOL CmdRegisterSet(int args) {
    if ((args == 2) &&
        (arg[1].str[0] == 'P') && (arg[2].str[0] == 'L'))
        regs.pc = lastpc;
    else if ((args < 2) || ((arg[2].str[0] != '0') && !arg[2].val1))
        return DisplayHelp(CmdMemoryEnter);
    else switch (arg[1].str[0]) {
        case 'A':
            regs.a = (BYTE)(arg[2].val1 & 0xFF);
            break;
        case 'P':
            regs.pc = arg[2].val1;
            break;
        case 'S':
            regs.sp = 0x100 | (arg[2].val1 & 0xFF);
            break;
        case 'X':
            regs.x = (BYTE)(arg[2].val1 & 0xFF);
            break;
        case 'Y':
            regs.y = (BYTE)(arg[2].val1 & 0xFF);
            break;
        default:
            return DisplayHelp(CmdMemoryEnter);
    }
    ComputeTopOffset(regs.pc);
    return 1;
}

//===========================================================================
static BOOL CmdTrace(int args) {
    stepcount = args ? arg[1].val1 : 1;
    stepline = 0;
    stepstart = regs.pc;
    stepuntil = -1;
    mode = MODE_STEPPING;
    DebugContinueStepping();
    return 0;
}

//===========================================================================
static BOOL CmdTraceFile(int args) {
    if (tracefile)
        fclose(tracefile);
    TCHAR filename[MAX_PATH];
    StrCopy(filename, progdir, ARRSIZE(filename));
    StrCat(filename, (args && arg[1].str[0]) ? arg[1].str : "trace.txt", ARRSIZE(filename));
    tracefile = fopen(filename, "wt");
    return 0;
}

//===========================================================================
static BOOL CmdTraceLine(int args) {
    stepcount = args ? arg[1].val1 : 1;
    stepline = 1;
    stepstart = regs.pc;
    stepuntil = -1;
    mode = MODE_STEPPING;
    DebugContinueStepping();
    return 0;
}

//===========================================================================
static BOOL CmdViewOutput(int args) {
    VideoRedrawScreen();
    viewingoutput = 1;
    return 0;
}

//===========================================================================
static BOOL CmdWatchAdd(int args) {
    if (!args)
        return DisplayHelp(CmdWatchAdd);
    BOOL addedone = 0;
    int  loop = 0;
    while (loop++ < args)
        if (arg[loop].val1 || (arg[loop].str[0] == '0') ||
            GetAddress(arg[loop].str)) {
            if (!arg[loop].val1)
                arg[loop].val1 = GetAddress(arg[loop].str);

            // FIND A FREE SLOT FOR THIS NEW WATCH
            int freeslot = 0;
            while ((freeslot < WATCHES) && (watch[freeslot] >= 0))
                freeslot++;
            if ((freeslot >= WATCHES) && !addedone)
                return DisplayError("All watch slots are currently in use.");

            // VERIFY THAT THE WATCH IS NOT ON AN I/O LOCATION
            if ((arg[loop].val1 >= 0xC000) && (arg[loop].val1 <= 0xC0FF))
                return DisplayError("You may not watch an I/O location.");

            // ADD THE WATCH
            if (freeslot < WATCHES) {
                watch[freeslot] = arg[loop].val1;
                addedone = 1;
                usingwatches = 1;
            }

        }
    if (!addedone)
        return DisplayHelp(CmdWatchAdd);
    return 1;
}

//===========================================================================
static BOOL CmdWatchClear(int args) {

    // CHECK FOR ERRORS
    if (!args)
        return DisplayHelp(CmdWatchAdd);
    if (!usingwatches)
        return DisplayError("There are no watches defined.");

    // CLEAR EACH WATCH IN THE LIST
    while (args) {
        if (StrCmp(arg[args].str, "*") == 0) {
            int loop = WATCHES;
            while (loop--)
                watch[loop] = -1;
        }
        else if ((arg[args].val1 >= 1) && (arg[args].val1 <= WATCHES))
            watch[arg[args].val1 - 1] = -1;
        args--;
    }

    // IF THERE ARE NO MORE WATCHES DEFINED, ERASE THE WATCH DISPLAY
    int usedslot = 0;
    while ((usedslot < WATCHES) && (watch[usedslot] < 0))
        usedslot++;
    if (usedslot >= WATCHES) {
        usingwatches = 0;
        DebugDisplay(1);
        return 0;
    }
    else
        return 1;

}

//===========================================================================
static BOOL CmdZap(int args) {
    int loop = addressmode[instruction[*(mem + regs.pc)].addrmode].bytes;
    while (loop--)
        * (mem + regs.pc + loop) = 0xEA;
    return 1;
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
            int addrmode = instruction[*(mem + currofs)].addrmode;
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
static BOOL DisplayError(LPCTSTR errortext) {
    return 0;
}

//===========================================================================
static BOOL DisplayHelp(cmdfunction function) {
    return 0;
}

//===========================================================================
static void DrawBreakpoints(HDC dc, int line) {
    RECT linerect;
    linerect.left = SCREENSPLIT2;
    linerect.top = (line << 4);
    linerect.right = 560;
    linerect.bottom = linerect.top + 16;
    TCHAR fulltext[16] = "Breakpoints";
    SetTextColor(dc, color[colorscheme][COLOR_STATIC]);
    SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
    int loop = 0;
    do {
        ExtTextOut(dc, linerect.left, linerect.top,
            ETO_CLIPPED | ETO_OPAQUE, &linerect,
            fulltext, StrLen(fulltext), NULL);
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
    linerect.left = 0;
    linerect.top = 368 - (line << 4);
    linerect.right = 12;
    linerect.bottom = linerect.top + 16;
    if (!title) {
        ExtTextOut(dc, 1, linerect.top,
            ETO_CLIPPED | ETO_OPAQUE, &linerect,
            ">", 1, NULL);
        linerect.left = 12;
    }
    linerect.right = 560;
    ExtTextOut(dc, linerect.left, linerect.top,
        ETO_CLIPPED | ETO_OPAQUE, &linerect,
        commandstring[line] + title, StrLen(commandstring[line] + title), NULL);
}

//===========================================================================
static WORD DrawDisassembly(HDC dc, int line, WORD offset, LPTSTR text, size_t textchars) {
    TCHAR addresstext[40] = "";
    TCHAR bytestext[10] = "";
    TCHAR fulltext[50] = "";
    BYTE  inst = *(mem + offset);
    int   addrmode = instruction[inst].addrmode;
    WORD  bytes = addressmode[addrmode].bytes;

    // BUILD A STRING CONTAINING THE TARGET ADDRESS OR SYMBOL
    if (addressmode[addrmode].format[0]) {
        WORD address = *(LPWORD)(mem + offset + 1);
        if (bytes == 2)
            address &= 0xFF;
        if (addrmode == ADDR_REL)
            address = offset + 2 + (int)(signed char)address;
        if (StrStr(addressmode[addrmode].format, "%s"))
            StrPrintf(
                addresstext,
                ARRSIZE(addresstext),
                addressmode[addrmode].format,
                (LPCTSTR)GetSymbol(address, bytes)
            );
        else
            StrPrintf(
                addresstext,
                ARRSIZE(addresstext),
                addressmode[addrmode].format,
                (unsigned)address
            );
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
                (unsigned) * (mem + offset + (loop++))
            );
        }
        while (StrLen(bytestext) < 6)
            StrCat(bytestext, " ", ARRSIZE(bytestext));
    }

    // PUT TOGETHER ALL OF THE DIFFERENT ELEMENTS THAT WILL MAKE UP THE LINE
    StrPrintf(
        fulltext,
        ARRSIZE(fulltext),
        "%04X  %s  %-9s %s %s",
        (unsigned)offset,
        (LPCTSTR)bytestext,
        (LPCTSTR)GetSymbol(offset, 0),
        (LPCTSTR)instruction[inst].mnemonic,
        (LPCTSTR)addresstext
    );
    if (text)
        StrCopy(text, fulltext, textchars);

    // DRAW THE LINE
    if (dc) {
        RECT linerect;
        linerect.left = 0;
        linerect.top = line << 4;
        linerect.right = SCREENSPLIT1 - 14;
        linerect.bottom = linerect.top + 16;
        BOOL bp = usingbp && CheckBreakpoint(offset, offset == regs.pc);
        SetTextColor(dc, color[colorscheme][(offset == regs.pc) ? COLOR_INSTBKG
            : bp ? COLOR_INSTBP
            : COLOR_INSTTEXT]);
        SetBkColor(dc, color[colorscheme][(offset == regs.pc) ? bp ? COLOR_INSTBP
            : COLOR_INSTTEXT
            : COLOR_INSTBKG]);
        ExtTextOut(dc, 12, linerect.top,
            ETO_CLIPPED | ETO_OPAQUE, &linerect,
            fulltext, StrLen(fulltext), NULL);
    }

    return bytes;
}

//===========================================================================
static void DrawFlags(HDC dc, int line, WORD value, LPTSTR text, size_t textchars) {
    TCHAR mnemonic[9] = "NVRBDIZC";
    TCHAR fulltext[2] = "?";
    RECT  linerect;
    if (dc) {
        linerect.left = SCREENSPLIT1 + 63;
        linerect.top = line << 4;
        linerect.right = SCREENSPLIT1 + 72;
        linerect.bottom = linerect.top + 16;
        SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
    }
    int loop = 8;
    while (loop--) {
        if (dc) {
            fulltext[0] = mnemonic[loop];
            SetTextColor(dc, color[colorscheme][(value & 1) ? COLOR_DATATEXT
                : COLOR_STATIC]);
            ExtTextOut(dc, linerect.left, linerect.top,
                ETO_CLIPPED | ETO_OPAQUE, &linerect,
                fulltext, 1, NULL);
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
    linerect.left = SCREENSPLIT2;
    linerect.top = (line << 4);
    linerect.right = 560;
    linerect.bottom = linerect.top + 16;
    TCHAR fulltext[16];
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
                        *(LPBYTE)(membank + curraddr)
                    );
                }
                curraddr++;
            }
        }
        SetTextColor(dc, color[colorscheme][COLOR_DATATEXT]);
    } while (loop++ < 4);
}

//===========================================================================
static void DrawRegister(HDC dc, int line, LPCTSTR name, int bytes, WORD value) {
    RECT linerect;
    linerect.left = SCREENSPLIT1;
    linerect.top = line << 4;
    linerect.right = SCREENSPLIT1 + 40;
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
    TCHAR valuestr[8];
    if (bytes == 2)
        StrPrintf(valuestr, ARRSIZE(valuestr), "%04X", (unsigned)value);
    else
        StrPrintf(valuestr, ARRSIZE(valuestr), "%02X", (unsigned)value);
    linerect.left = SCREENSPLIT1 + 40;
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
        linerect.left = SCREENSPLIT1;
        linerect.top = (loop + line) << 4;
        linerect.right = SCREENSPLIT1 + 40;
        linerect.bottom = linerect.top + 16;
        SetTextColor(dc, color[colorscheme][COLOR_STATIC]);
        SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
        TCHAR outtext[8] = "";
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
        linerect.left = SCREENSPLIT1 + 40;
        linerect.right = SCREENSPLIT2;
        SetTextColor(dc, color[colorscheme][COLOR_DATATEXT]);
        if (curraddr <= 0x1FF)
            StrPrintf(outtext, ARRSIZE(outtext), "%02X", (unsigned) * (LPBYTE)(mem + curraddr));
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
        TCHAR addressstr[8] = "";
        TCHAR valuestr[8] = "";
        if (address[loop] >= 0) {
            StrPrintf(addressstr, ARRSIZE(addressstr), "%04X", address[loop]);
            if (loop)
                StrPrintf(valuestr, ARRSIZE(valuestr), "%02X", *(LPBYTE)(mem + address[loop]));
            else
                StrPrintf(valuestr, ARRSIZE(valuestr), "%04X", *(LPWORD)(mem + address[loop]));
        }
        RECT linerect;
        linerect.left = SCREENSPLIT1;
        linerect.top = (line + loop) << 4;
        linerect.right = SCREENSPLIT1 + 40;
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
        linerect.left = SCREENSPLIT1 + 40;
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
    linerect.left = SCREENSPLIT2;
    linerect.top = (line << 4);
    linerect.right = 560;
    linerect.bottom = linerect.top + 16;
    TCHAR outstr[16] = "Watches";
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
    int loop = 0;
    while (loop < WATCHES) {
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
        loop++;
    }
    linerect.left = SCREENSPLIT2 + 64;
    linerect.top = (line << 4);
    linerect.right = 560;
    linerect.bottom = linerect.top + 16;
    SetTextColor(dc, color[colorscheme][COLOR_DATATEXT]);
    loop = 0;
    while (loop < WATCHES) {
        if (watch[loop] >= 0)
            StrPrintf(outstr, ARRSIZE(outstr), "%02X", (unsigned) * (LPBYTE)(mem + watch[loop]));
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
        loop++;
    }
}

//===========================================================================
static BOOL ExecuteCommand(int args) {
    char * context = NULL;
    char * name = StrTok(commandstring[0], " ,-=", &context);
    if (!name)
        name = commandstring[0];
    int         found = 0;
    cmdfunction function = NULL;
    int         length = StrLen(name);
    int         loop = 0;
    while ((loop < COMMANDS) && (name[0] >= command[loop].name[0])) {
        if (!StrCmpLen(name, command[loop].name, length)) {
            function = command[loop].function;
            if (!StrCmp(name, command[loop].name)) {
                found = 1;
                loop = COMMANDS;
            }
            else
                found++;
        }
        loop++;
    }
    if (found > 1)
        return DisplayError("Ambiguous command");
    else if (function)
        return function(args);
    else
        return DisplayError("Illegal command");
}

//===========================================================================
static void FreeSymbolTable() {
    if (symboltable)
        VirtualFree(symboltable, 0, MEM_RELEASE);
    symbolnum = 0;
    symboltable = NULL;
}

//===========================================================================
static WORD GetAddress(LPCTSTR symbol) {
    int loop = symbolnum;
    while (loop--)
        if (!StrCmpI(symboltable[loop].name, symbol))
            return symboltable[loop].value;
    return 0;
}

//===========================================================================
static LPCTSTR GetSymbol(WORD address, int bytes) {

    // PERFORM A BINARY SEARCH THROUGH THE SYMBOL TABLE LOOKING FOR A VALUE
    // MATCHING THIS ADDRESS
    {
        int lowlimit = -1;
        int highlimit = symbolnum;
        int curr = symbolnum >> 1;
        do {
            int diff = ((int)address) - ((int)symboltable[curr].value);
            if (diff < 0) {
                highlimit = curr;
                curr = lowlimit + ((curr - lowlimit) >> 1);
            }
            else if (diff > 0) {
                lowlimit = curr;
                curr = curr + ((highlimit + 1 - curr) >> 1);
            }
            else
                return symboltable[curr].name;
        } while ((curr > lowlimit) && (curr < highlimit));
    }

    // IF THERE IS NO SYMBOL FOR THIS ADDRESS, THEN JUST RETURN A STRING
    // CONTAINING THE ADDRESS NUMBER
    static TCHAR buffer[16];
    switch (bytes) {
        case 2:
            StrPrintf(buffer, ARRSIZE(buffer), "$%02X", (unsigned)address);
            break;
        case 3:
            StrPrintf(buffer, ARRSIZE(buffer), "$%04X", (unsigned)address);
            break;
        default:
            buffer[0] = 0;
            break;
    }
    return buffer;

}

//===========================================================================
static void GetTargets(int * intermediate, int * final) {
    *intermediate = -1;
    *final        = -1;
    int  addrmode   = instruction[*(mem + regs.pc)].addrmode;
    BYTE argument8  = *(LPBYTE)(mem + regs.pc + 1);
    WORD argument16 = *(LPWORD)(mem + regs.pc + 1);
    switch (addrmode) {

        case ADDR_ABS:
            *final = argument16;
            break;

        case ADDR_ABSIINDX:
            argument16 += regs.x;
            *intermediate = argument16;
            *final = *(LPWORD)(mem + *intermediate);
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
            *final = *(LPWORD)(mem + *intermediate);
            break;

        case ADDR_INDX:
            argument8 += regs.x;
            *intermediate = argument8;
            *final = *(LPWORD)(mem + *intermediate);
            break;

        case ADDR_INDY:
            *intermediate = argument8;
            *final = (*(LPWORD)(mem + *intermediate)) + regs.y;
            break;

        case ADDR_IZPG:
            *intermediate = argument8;
            *final = *(LPWORD)(mem + *intermediate);
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
    if ((*final >= 0) &&
        ((!StrCmp(instruction[*(mem + regs.pc)].mnemonic, "JMP")) ||
        (!StrCmp(instruction[*(mem + regs.pc)].mnemonic, "JSR"))))
        * final = -1;
}

//===========================================================================
static BOOL InternalSingleStep() {
    BOOL result = 0;
    _try {
        ++profiledata[mem[regs.pc]];
        CpuExecute(stepline);
        result = 1;
    }
    _except(EXCEPTION_EXECUTE_HANDLER) {
        result = 0;
    }
    return result;
}

//===========================================================================
static void OutputTraceLine() {
    TCHAR disassembly[50];
    TCHAR flags[9];
    DrawDisassembly((HDC)0, 0, regs.pc, disassembly, ARRSIZE(disassembly));
    DrawFlags((HDC)0, 0, regs.ps, flags, ARRSIZE(flags));
    fprintf(
        tracefile,
        "a=%02x x=%02x y=%02x sp=%03x ps=%s %s\n",
        (unsigned)regs.a,
        (unsigned)regs.x,
        (unsigned)regs.y,
        (unsigned)regs.sp,
        (LPCTSTR)flags,
        (LPCTSTR)disassembly
    );
}

//===========================================================================
static int ParseCommandString() {
    int    args = 0;
    LPTSTR currptr = commandstring[0];
    char * context = NULL;
    while (*currptr) {
        LPTSTR endptr = NULL;
        int    length = StrLen(currptr);
        StrTok(currptr, " ,-=", &context);
        StrCopy(arg[args].str, currptr, ARRSIZE(arg[args].str));
        arg[args].val1 = (WORD)(StrToUnsigned(currptr, &endptr, 16) & 0xFFFF);
        if (endptr)
            if (*endptr == 'L') {
                arg[args].val2 = (WORD)(StrToUnsigned(endptr + 1, &endptr, 16) & 0xFFFF);
                if (endptr && *endptr)
                    arg[args].val2 = 0;
            }
            else {
                arg[args].val2 = 0;
                if (*endptr)
                    arg[args].val1 = 0;
            }
        else
            arg[args].val2 = 0;
        BOOL more = ((*currptr) && (length > StrLen(currptr)));
        args += more;
        currptr += StrLen(currptr) + more;
    }
    int loop = args;
    while (loop++ < MAXARGS - 1) {
        arg[loop].str[0] = 0;
        arg[loop].val1 = 0;
        arg[loop].val2 = 0;
    }
    return args;
}

//===========================================================================
static void WriteProfileData() {
    TCHAR filename[MAX_PATH];
    StrCopy(filename, progdir, ARRSIZE(filename));
    StrCat(filename, "Profile.txt", ARRSIZE(filename));
    FILE * file = fopen(filename, "wt");
    if (file) {
        DWORD maxvalue;
        do {
            maxvalue = 0;
            DWORD maxitem;
            DWORD loop;
            for (loop = 0; loop < 256; ++loop)
                if (profiledata[loop] > maxvalue) {
                    maxvalue = profiledata[loop];
                    maxitem = loop;
                }
            if (maxvalue) {
                fprintf(file,
                    "%9u  %02X  %s\n",
                    (unsigned)maxvalue,
                    (unsigned)maxitem,
                    (LPCTSTR)instruction[maxitem].mnemonic);
                profiledata[maxitem] = 0;
            }
        } while (maxvalue);
        fclose(file);
    }
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void DebugBegin() {
    if (!membank)
        membank = mem;
    mode = MODE_DEBUG;
    addressmode[INVALID2].bytes = apple2e ? 2 : 1;
    addressmode[INVALID3].bytes = apple2e ? 3 : 1;
    ComputeTopOffset(regs.pc);
    DebugDisplay(1);
}

//===========================================================================
void DebugContinueStepping() {
    static unsigned stepstaken = 0;
    if (stepcount) {
        if (tracefile)
            OutputTraceLine();
        lastpc = regs.pc;
        InternalSingleStep();
        if ((regs.pc == stepuntil) || CheckBreakpoint(regs.pc, 1))
            stepcount = 0;
        else if (stepcount > 0)
            stepcount--;
    }
    if (stepcount) {
        if (!((++stepstaken) & 0xFFFF))
            if (stepstaken == 0x10000)
                VideoRedrawScreen();
            else
                VideoRefreshScreen();
    }
    else {
        mode = MODE_DEBUG;
        if ((stepstart < regs.pc) && (stepstart + 3 >= regs.pc))
            topoffset += addressmode[instruction[*(mem + topoffset)].addrmode].bytes;
        else
            ComputeTopOffset(regs.pc);
        DebugDisplay(stepstaken >= 0x10000);
        stepstaken = 0;
    }
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
        viewportrect.left = 0;
        viewportrect.top = 0;
        viewportrect.right = SCREENSPLIT1 - 14;
        viewportrect.bottom = 304;
        SetBkColor(dc, color[colorscheme][COLOR_INSTBKG]);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &viewportrect, "", 0, NULL);
        viewportrect.left = SCREENSPLIT1 - 14;
        viewportrect.right = 560;
        SetBkColor(dc, color[colorscheme][COLOR_DATABKG]);
        ExtTextOut(dc, 0, 0, ETO_OPAQUE, &viewportrect, "", 0, NULL);
    }

    // DRAW DISASSEMBLED LINES
    {
        int  line = 0;
        WORD offset = topoffset;
        while (line < SOURCELINES) {
            offset += DrawDisassembly(dc, line, offset, NULL, 0);
            line++;
        }
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
    {
        int line = COMMANDLINES;
        while (line--)
            DrawCommandLine(dc, line);
    }

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
    // CLEAR THE BREAKPOINT AND WATCH TABLES
    ZeroMemory(breakpoint, BREAKPOINTS * sizeof(bprec));
    {
        int loop = 0;
        while (loop < WATCHES)
            watch[loop++] = -1;
    }

    // READ IN THE SYMBOL TABLE
    {
        TCHAR filename[MAX_PATH];
        StrCopy(filename, progdir, ARRSIZE(filename));
        StrCat(filename, "apple2e.sym", ARRSIZE(filename));
        int    symbolalloc = 0;
        FILE * infile      = fopen(filename, "rt");
        WORD   lastvalue   = 0;
        if (infile) {
            while (!feof(infile)) {

                // READ IN THE NEXT LINE, AND MAKE SURE IT IS SORTED CORRECTLY IN
                // VALUE ORDER
                DWORD value = 0;
                char  name[80] = "";
                char  line[256];
                if (fscanf(infile, "%x %13s", &value, name) != 2)
                    value = 0;
                fgets(line, 255, infile);
                if (name[0] != '\0') {
                    if (value < lastvalue) {
                        MessageBox(GetDesktopWindow(),
                            "The symbol file is not sorted correctly.  "
                            "Symbols will not be loaded.",
                            TITLE,
                            MB_ICONEXCLAMATION | MB_SETFOREGROUND);
                        FreeSymbolTable();
                        return;
                    }
                    else {
                        // IF OUR CURRENT SYMBOL TABLE IS NOT BIG ENOUGH TO HOLD THIS
                        // ADDITIONAL SYMBOL, THEN ALLOCATE A BIGGER TABLE AND COPY THE
                        // CURRENT DATA ACROSS
                        if ((!symboltable) || (symbolalloc <= symbolnum)) {
                            symbolalloc += 8192 / sizeof(symbolrec);
                            symbolrec * newtable = (symbolrec *)VirtualAlloc(NULL,
                                symbolalloc * sizeof(symbolrec),
                                MEM_COMMIT,
                                PAGE_READWRITE);
                            if (newtable) {
                                if (symboltable) {
                                    CopyMemory(newtable, symboltable, symbolnum * sizeof(symbolrec));
                                    VirtualFree(symboltable, 0, MEM_RELEASE);
                                }
                                symboltable = newtable;
                            }
                            else {
                                MessageBox(GetDesktopWindow(),
                                    "There is not enough memory available to load "
                                    "the symbol file.",
                                    TITLE,
                                    MB_ICONEXCLAMATION | MB_SETFOREGROUND);
                                FreeSymbolTable();
                            }
                        }

                        // SAVE THE NEW SYMBOL IN THE SYMBOL TABLE
                        if (symboltable) {
                            symboltable[symbolnum].value = (WORD)(value & 0xFFFF);
                            StrCopy(symboltable[symbolnum].name, name, ARRSIZE(symboltable[symbolnum].name));
                            symboltable[symbolnum].name[13] = 0;
                            symbolnum++;
                        }

                        lastvalue = (WORD)value;
                    }
                }
            }
            fclose(infile);
        }
    }

    // CREATE A FONT FOR THE DEBUGGING SCREEN
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
void DebugProcessChar(TCHAR ch) {
    if (mode != MODE_DEBUG)
        return;
    if ((ch == ' ') && !commandstring[0][0])
        return;
    if ((ch >= 32) && (ch <= 126)) {
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
    if ((mode == MODE_STEPPING) && (keycode == VK_ESCAPE))
        stepcount = 0;
    if (mode != MODE_DEBUG)
        return;
    if (viewingoutput) {
        DebugDisplay(1);
        viewingoutput = 0;
        return;
    }
    BOOL needscmdrefresh = 0;
    BOOL needsfullrefresh = 0;
    if ((keycode == VK_SPACE) && commandstring[0][0])
        return;
    if (keycode == VK_BACK) {
        int length = StrLen(commandstring[0]);
        if (length)
            commandstring[0][length - 1] = 0;
        needscmdrefresh = 1;
    }
    else if (keycode == VK_RETURN) {
        if ((!commandstring[0][0]) &&
            (commandstring[1][0] != ' '))
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
        DebugDisplay(0);
    else if (needscmdrefresh && (!viewingoutput) &&
        ((mode == MODE_DEBUG) || (mode == MODE_STEPPING))) {
        HDC dc = FrameGetDC();
        while (needscmdrefresh--)
            DrawCommandLine(dc, needscmdrefresh);
        FrameReleaseDC(dc);
    }
}
