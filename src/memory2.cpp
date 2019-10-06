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

// I/O switch flags
constexpr uint32_t SF_80STORE    = 1 << 0;  // 80-store mode
constexpr uint32_t SF_RAMRD      = 1 << 1;  // read from aux
constexpr uint32_t SF_RAMWRT     = 1 << 2;  // write to aux
constexpr uint32_t SF_INTCXROM   = 1 << 3;  // use internal system rom for $CXXX
constexpr uint32_t SF_ALTZP      = 1 << 4;  // alt zero page
constexpr uint32_t SF_SLOTC3ROM  = 1 << 5;  // use slot rom for $C3XX
constexpr uint32_t SF_PAGE2      = 1 << 6;  // use page 2 display
constexpr uint32_t SF_HIRES      = 1 << 7;  // hi-res mode on
constexpr uint32_t SF_BANK2      = 1 << 8;  // use bank2 for $DXXX hi ram
constexpr uint32_t SF_HRAMRD     = 1 << 9;  // read high memory from ram
constexpr uint32_t SF_HRAMWRT    = 1 << 10; // write high memory to ram
constexpr uint32_t SF_UPDATEMASK = (1 << 11) - 1;
constexpr uint32_t SF_PREWRITE   = 1 << 31; // toggle for high memory mode counter

// I/O switch helper macros
#define  SW_80STORE()   ((s_switches & SF_80STORE)   != 0)
#define  SW_RAMRD()     ((s_switches & SF_RAMRD)     != 0)
#define  SW_RAMWRT()    ((s_switches & SF_RAMWRT)    != 0)
#define  SW_INTCXROM()  ((s_switches & SF_INTCXROM)  != 0)
#define  SW_ALTZP()     ((s_switches & SF_ALTZP)     != 0)
#define  SW_SLOTC3ROM() ((s_switches & SF_SLOTC3ROM) != 0)
#define  SW_PAGE2()     ((s_switches & SF_PAGE2)     != 0)
#define  SW_HIRES()     ((s_switches & SF_HIRES)     != 0)
#define  SW_BANK2()     ((s_switches & SF_BANK2)     != 0)
#define  SW_HRAMRD()    ((s_switches & SF_HRAMRD)    != 0)
#define  SW_HRAMWRT()   ((s_switches & SF_HRAMWRT)   != 0)
#define  SW_PREWRITE()  ((s_switches & SF_PREWRITE)  != 0)


/****************************************************************************
*
*   Variables
*
***/

uint8_t * g_pageRead[0x100];
uint8_t * g_pageWrite[0x100];

static uint8_t * s_memMain;
static uint8_t * s_memAux;
static uint8_t * s_memSystemRom;
static uint8_t * s_memSlotRom;
static uint8_t * s_memNull;
static uint32_t  s_switches;
static uint32_t  s_switchesPrev;


/****************************************************************************
*
*   Local functions
*
***/

static void UpdatePageTables();

//===========================================================================
static void InitializePageTables() {
    s_switchesPrev = ~s_switches; // this forces all pages to update
    UpdatePageTables();
}

//===========================================================================
static uint8_t ReturnRandomData(bool hiBit) {
    static const uint8_t retval[16] = {
        0x00, 0x2D, 0x2D, 0x30, 0x30, 0x32, 0x32, 0x34,
        0x35, 0x39, 0x43, 0x43, 0x43, 0x60, 0x7F, 0x7F
    };

    uint8_t r = (uint8_t)rand();
    if (r <= 170)
        return 0x20 | (hiBit ? 0x80 : 0);
    else
        return retval[r & 15] | (hiBit ? 0x80 : 0);
}

//===========================================================================
static void UpdatePageTables() {
    // Figure out which I/O switches changed since the last update.
    uint32_t updateMask = (s_switches ^ s_switchesPrev) & SF_UPDATEMASK;
    if (!updateMask)
        return;

    // Zero page and stack ($0000..$01FF)
    uint8_t * zpBank = SW_ALTZP() ? s_memAux : s_memMain;
    if (updateMask & SF_ALTZP) {
        for (size_t page = 0x00; page < 0x02; ++page)
            g_pageRead[page] = g_pageWrite[page] = zpBank + (page << 8);
    }

    // Low memory excluding first text and hi-res pages ($0200..$03FF,
    // $0800..$1FFF, $4000..$BFFF)
    uint8_t * lowBankRead  = SW_RAMRD()  ? s_memAux : s_memMain;
    uint8_t * lowBankWrite = SW_RAMWRT() ? s_memAux : s_memMain;
    if (updateMask & (SF_RAMRD | SF_RAMWRT)) {
        for (size_t page = 0x02; page < 0x04; ++page) {
            g_pageRead[page]  = lowBankRead + (page << 8);
            g_pageWrite[page] = lowBankWrite + (page << 8);
        }
        for (size_t page = 0x08; page < 0x20; ++page) {
            g_pageRead[page]  = lowBankRead + (page << 8);
            g_pageWrite[page] = lowBankWrite + (page << 8);
        }
        for (size_t page = 0x40; page < 0xc0; ++page) {
            g_pageRead[page]  = lowBankRead + (page << 8);
            g_pageWrite[page] = lowBankWrite + (page << 8);
        }
    }

    // Text memory page 1 ($0400..$07FF)
    if (updateMask & (SF_RAMRD | SF_RAMWRT | SF_80STORE | SF_PAGE2)) {
        uint8_t * pageBankRead  = lowBankRead;
        uint8_t * pageBankWrite = lowBankWrite;
        if (SW_80STORE())
            pageBankRead = pageBankWrite = SW_PAGE2() ? s_memAux : s_memMain;
        for (size_t page = 0x04; page < 0x08; ++page) {
            g_pageRead[page]  = pageBankRead + (page << 8);
            g_pageWrite[page] = pageBankWrite + (page << 8);
        }
    }

    // Hi-res memory page 1 ($2000..$3FFF)
    if (updateMask & (SF_RAMRD | SF_RAMWRT | SF_80STORE | SF_PAGE2 | SF_HIRES)) {
        uint8_t * pageBankRead  = lowBankRead;
        uint8_t * pageBankWrite = lowBankWrite;
        if ((s_switches & (SF_80STORE | SF_HIRES)) == (SF_80STORE | SF_HIRES))
            pageBankRead = pageBankWrite = SW_PAGE2() ? s_memAux : s_memMain;
        for (size_t page = 0x20; page < 0x40; ++page) {
            g_pageRead[page]  = pageBankRead + (page << 8);
            g_pageWrite[page] = pageBankWrite + (page << 8);
        }
    }

    // I/O range ($C000..$CFFF)
    if (updateMask & (SF_INTCXROM | SF_SLOTC3ROM)) {
        for (int page = 0xc0; page < 0xd0; ++page)
            g_pageWrite[page] = s_memNull;
        if (SW_INTCXROM()) {
            for (size_t page = 0xc0; page < 0xd0; ++page)
                g_pageRead[page] = s_memSystemRom + ((page - 0xc0) << 8);
        }
        else {
            for (size_t page = 0xc0; page < 0xc8; ++page)
                g_pageRead[page] = s_memSlotRom + ((page - 0xc0) << 8);
            if (!SW_SLOTC3ROM())
                g_pageRead[0xc3] = s_memSystemRom + 0x0300;
            for (size_t page = 0xc8; page < 0xd0; ++page)
                g_pageRead[page] = s_memSystemRom + ((page - 0xc0) << 8); // need to handle slot strobe remapping here
        }
    }

    // High memory ($D000..$FFFF)
    if (updateMask & (SF_BANK2 | SF_ALTZP | SF_HRAMRD | SF_HRAMWRT)) {

        // First 4K of high memory ($D000..$DFFF)
        for (size_t page = 0xd0; page < 0xe0; ++page) {
            size_t bankOffset = SW_BANK2() ? 0 : 0x1000;
            if (SW_HRAMRD())
                g_pageRead[page] = zpBank - bankOffset + (page << 8);
            else
                g_pageRead[page] = s_memSystemRom + ((page - 0xc0) << 8);
            if (SW_HRAMWRT())
                g_pageWrite[page] = zpBank - bankOffset + (page << 8);
            else
                g_pageWrite[page] = s_memNull;
        }

        // Last 8K of high memory ($E000..$FFFF)
        for (size_t page = 0xe0; page < 0x100; ++page) {
            if (SW_HRAMRD())
                g_pageRead[page] = zpBank + (page << 8);
            else
                g_pageRead[page] = s_memSystemRom + ((page - 0xc0) << 8);
            if (SW_HRAMWRT())
                g_pageWrite[page] = zpBank + (page << 8);
            else
                g_pageWrite[page] = s_memNull;
        }
    }

    s_switchesPrev = s_switches;
}


/****************************************************************************
*
*   I/O switch handlers
*
***/

//===========================================================================
static uint8_t IoNull(uint8_t address, bool write, uint8_t value) {
    if (write)
        return 0;

    if ((address & 0xf0) == 0xa0) {
        static const uint8_t retval[16] = {
            0x58, 0xfc, 0x5b, 0xff, 0x58, 0xfc, 0x5b, 0xff,
            0x0b, 0x10, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
        };
        return retval[address & 0x0f];
    }

    if (address >= 0xb0 && address <= 0xcf) {
        uint8_t r = (uint8_t)rand();
        if (r >= 0x10)
            return 0xa0;
        else if (r >= 0x08)
            return (r > 0x0C) ? 0xff : 0x00;
        else
            return address & 0xf7;
    }

    if ((address & 0xf0) == 0xd0) {
        uint8_t r = (uint8_t)rand();
        if (r >= 0xc0)
            return 0xc0;
        else if (r >= 0x80)
            return 0x80;
        else if (address == 0xd0 || address == 0xdf)
            return 0x00;
        else if (r >= 0x40)
            return 0x40;
        else if (r >= 0x30)
            return 0x90;
        else
            return 0x00;
    }

    return ReturnRandomData(true);
}

//===========================================================================
static uint8_t IoSwitchReadC00x(uint8_t address, bool, uint8_t value) {
    return KeybReadData(0, address, 0, 0);
}

//===========================================================================
static uint8_t IoSwitchWriteC00x(uint8_t address, bool, uint8_t value) {
    bool updateVideo = false;
    switch (address) {
        case 0x00: s_switches &= ~SF_80STORE; updateVideo = true; break;
        case 0x01: s_switches |=  SF_80STORE; updateVideo = true; break;
        case 0x02: s_switches &= ~SF_RAMRD;     break;
        case 0x03: s_switches |=  SF_RAMRD;     break;
        case 0x04: s_switches &= ~SF_RAMWRT;    break;
        case 0x05: s_switches |=  SF_RAMWRT;    break;
        case 0x06: s_switches &= ~SF_INTCXROM;  break;
        case 0x07: s_switches |=  SF_INTCXROM;  break;
        case 0x08: s_switches &= ~SF_ALTZP;     break;
        case 0x09: s_switches |=  SF_ALTZP;     break;
        case 0x0a: s_switches &= ~SF_SLOTC3ROM; break;
        case 0x0b: s_switches |=  SF_SLOTC3ROM; break;
        default:   updateVideo = true;          break;
    }

    UpdatePageTables();
    if (updateVideo)
        VideoSetMode(0, address, 1, value);
    return 0;
}

//===========================================================================
static uint8_t IoSwitchReadC01x(uint8_t address, bool, uint8_t value) {
    bool result = false;
    switch (address) {
        case 0x11: result = SW_BANK2();      break;
        case 0x12: result = SW_HRAMRD();     break;
        case 0x13: result = SW_RAMRD();      break;
        case 0x14: result = SW_RAMWRT();     break;
        case 0x15: result = SW_INTCXROM();   break;
        case 0x16: result = SW_ALTZP();      break;
        case 0x17: result = SW_SLOTC3ROM();  break;
        case 0x18: result = SW_80STORE();    break;
        case 0x1c: result = SW_PAGE2();      break;
        case 0x1d: result = SW_HIRES();      break;

        case 0x10:
            return KeybReadFlag(0, address, 0, 0);
        case 0x19:
            return VideoCheckVbl(0, address, 0, 0);
        case 0x1a:
        case 0x1b:
        case 0x1e:
        case 0x1f:
            return VideoCheckMode(0, address, 0, 0);
    }
    return KeybGetKeycode() | (result ? 0x80 : 0);
}

//===========================================================================
static uint8_t IoSwitchWriteC01x(uint8_t address, bool, uint8_t value) {
    return KeybReadFlag(0, address, 0, 0);
}

//===========================================================================
static uint8_t IoSwitchC03x(uint8_t address, bool write, uint8_t value) {
    return SpkrToggle(0, address, write, value);
}

//===========================================================================
static uint8_t IoSwitchC05x(uint8_t address, bool write, uint8_t value) {
    if (address >= 0x54 && address <= 0x57) {
        bool hiBit = true;
        switch (address) {
            case 0x54: s_switches &= ~SF_PAGE2; hiBit = SW_PAGE2(); break;
            case 0x55: s_switches |=  SF_PAGE2; hiBit = SW_PAGE2(); break;
            case 0x56: s_switches &= ~SF_HIRES; break;
            case 0x57: s_switches |=  SF_HIRES; break;
        }
        UpdatePageTables();
        VideoSetMode(0, address, write, value);
        return write ? 0 : ReturnRandomData(hiBit);
    }

    switch (address) {
        case 0x50:
        case 0x51:
        case 0x52:
        case 0x53:
        case 0x5e:
        case 0x5f:
            return VideoSetMode(0, address, write, value);
    }

    return IoNull(address, write, value);
}

//===========================================================================
static uint8_t IoSwitchReadC06x(uint8_t address, bool, uint8_t value) {
    switch (address) {
        case 0x61:
        case 0x62:
        case 0x63:
            return JoyReadButton(0, address, 0, 0);
        case 0x64:
        case 0x65:
            return JoyReadPosition(0, address, 0, 0);
        default:
            return IoNull(address, 0, 0);
    }
}

//===========================================================================
static uint8_t IoSwitchC07x(uint8_t address, bool write, uint8_t value) {
    switch (address) {
        case 0x70:
            return JoyResetPosition(0, address, 0, 0);
        case 0x7f:
            return write ? IoNull(address, true, value) : VideoCheckMode(0, address, 0, 0);
        default:
            return IoNull(address, 0, 0);
    }
}

//===========================================================================
static uint8_t IoSwitchC08x(uint8_t address, bool write, uint8_t value) {
    uint8_t a0 = address & (1 << 0);
    uint8_t a1 = address & (1 << 1);
    uint8_t a3 = address & (1 << 3);

    // Only enable hi-ram reads if a0 and a1 are the same.
    if (a0 == (a1 >> 1))
        s_switches |= SF_HRAMRD;
    else
        s_switches &= ~SF_HRAMRD;

    // If reading an odd address, increment the "prewrite" counter. When the
    // prewrite counter reaches 2, enable hi-ram writes. Reset the prewrite
    // counter if a write is performed or if an even address is read. Disable
    // writes to hi-ram only if an even address is accessed.
    if (a0 && !write) {
        if (SW_PREWRITE())
            s_switches |= SF_HRAMWRT;
        s_switches |= SF_PREWRITE;
    }
    else
        s_switches &= ~SF_PREWRITE;
    if (!a0)
        s_switches &= ~SF_HRAMWRT;

    // Update the hi-ram memory bank selection based on address bit 3.
    if (a3)
        s_switches &= ~SF_BANK2;
    else
        s_switches |= SF_BANK2;

    UpdatePageTables();

    return write ? 0 : ReturnRandomData(true);
}

//===========================================================================
static uint8_t IoSwitchC0Ax(uint8_t address, bool write, uint8_t value) {
    switch (address) {
        case 0xa1:
        case 0xa2:
            return write ? IoNull(address, 1, value) : CommDipSw(0, address, write, value);
        case 0xa8:
            return write ? CommTransmit(0, address, 1, value) : CommReceive(0, address, 0, value);
        case 0xa9:
            return CommStatus(0, address, write, value);
        case 0xaa:
            return CommCommand(0, address, write, value);
        case 0xab:
            return CommControl(0, address, write, value);
        default:
            return IoNull(address, 0, 0);
    }
}

//===========================================================================
static uint8_t IoSwitchC0Ex(uint8_t address, bool write, uint8_t value) {
    switch (address) {
        case 0xe8:
        case 0xe9:
            return DiskControlMotor(0, address, write, value);
        case 0xea:
        case 0xeb:
            return DiskEnable(0, address, write, value);
        case 0xec:
            return DiskReadWrite(0, address, write, value);
        case 0xed:
            return DiskSetLatchValue(0, address, write, value);
        case 0xee:
            return DiskSetReadMode(0, address, write, value);
        case 0xef:
            return DiskSetWriteMode(0, address, write, value);
        default:
            return DiskControlStepper(0, address, write, value);
    }
}


/****************************************************************************
*
*   I/O switch tables
*
***/

using FIoSwitch = uint8_t (*)(uint8_t offset, bool write, uint8_t value);

static const FIoSwitch s_switchRead[] = {
    IoSwitchReadC00x,
    IoSwitchReadC01x,
    IoNull,         // $C02x
    IoSwitchC03x,
    IoNull,         // $C04x
    IoSwitchC05x,
    IoSwitchReadC06x,
    IoSwitchC07x,
    IoSwitchC08x,
    IoNull,         // $C09x
    IoSwitchC0Ax,
    IoNull,         // $C0Bx
    IoNull,         // $C0Cx
    IoNull,         // $C0Dx
    IoSwitchC0Ex,
    IoNull,         // $C0Fx
};

static const FIoSwitch s_switchWrite[] = {
    IoSwitchWriteC00x,
    IoSwitchWriteC01x,
    IoNull,         // $C02x
    IoSwitchC03x,
    IoNull,         // $C04x
    IoSwitchC05x,
    IoNull,         // $C06x
    IoSwitchC07x,
    IoSwitchC08x,
    IoNull,         // $C09x
    IoSwitchC0Ax,
    IoNull,         // $C0Bx
    IoNull,         // $C0Cx
    IoNull,         // $C0Dx
    IoSwitchC0Ex,
    IoNull,         // $C0Fx
};


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
void MemDestroy2() {
    delete[] s_memMain;
    delete[] s_memAux;
    delete[] s_memSystemRom;
    delete[] s_memSlotRom;
    delete[] s_memNull;
    s_memMain      = nullptr;
    s_memAux       = nullptr;
    s_memSystemRom = nullptr;
    s_memSlotRom   = nullptr;
    s_memNull      = nullptr;
}

//===========================================================================
void MemInitialize2() {
    s_memMain       = new uint8_t[0x10000];
    s_memAux        = new uint8_t[0x10000];
    s_memSystemRom  = new uint8_t[0x4000];
    s_memSlotRom    = new uint8_t[0x800];
    s_memNull       = new uint8_t[0x100];

    // Select a system rom resource.
    static const char * s_romFiles[] = {
        "APPLE2_SYSTEM_ROM",
        "APPLE2PLUS_SYSTEM_ROM",
        "APPLE2ENHANCED_SYSTEM_ROM",
    };
    static_assert(ARRSIZE(s_romFiles) == APPLE_TYPES, "Rom file array mismatch");

    // Load the system rom.
    int size;
    const char * name = s_romFiles[EmulatorGetAppleType()];
    const void * rom = ResourceLoad(name, "ROM", &size);
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
    if (size == 0x4000)
        memcpy(s_memSystemRom, rom, 0x4000);
    else if (size == 0x3000)
        memcpy(s_memSystemRom + 0x1000, rom, 0x3000);
    else {
        MessageBox(
            GetDesktopWindow(),
            "Firmware ROM file was not the correct size.",
            EmulatorGetTitle(),
            MB_ICONSTOP | MB_SETFOREGROUND
        );
        ExitProcess(1);
    }
    ResourceFree(rom);

    // Initialize the slot rom.
    memset(s_memSlotRom, 0, 0x800);

    // Install the rom for a Disk ][ in slot 6, and patch out the wait call.
    MemInstallPeripheralRom("DISK2_ROM", 6);
    s_memSlotRom[0x064c] = 0xa9;
    s_memSlotRom[0x064d] = 0x00;
    s_memSlotRom[0x064e] = 0xea;

    MemReset2();
}

//===========================================================================
void MemInstallPeripheralRom(const char * romResourceName, int slot) {
    int size;
    const void * rom = ResourceLoad(romResourceName, "ROM", &size);
    if (rom == nullptr) {
        char msg[256];
        StrPrintf(
            msg,
            ARRSIZE(msg),
            "The emulator was unable to load the ROM resource"
            "'%s' into memory.",
            romResourceName
        );
        MessageBox(
            GetDesktopWindow(),
            msg,
            EmulatorGetTitle(),
            MB_ICONSTOP | MB_SETFOREGROUND
        );
        return;
    }

    if (size == 0x100)
        memcpy(s_memSlotRom + (size_t)slot * 0x100, rom, 0x100);
    else {
        MessageBox(
            GetDesktopWindow(),
            "Firmware ROM file was not the correct size.",
            EmulatorGetTitle(),
            MB_ICONSTOP | MB_SETFOREGROUND
        );
    }

    ResourceFree(rom);
}

//===========================================================================
uint8_t MemIoRead(uint16_t address) {
    uint8_t offset = uint8_t(address);
    return s_switchRead[offset >> 4](offset, false, 0);
}

//===========================================================================
void MemIoWrite(uint16_t address, uint8_t value) {
    uint8_t offset = uint8_t(address);
    s_switchWrite[offset >> 4](offset, true, 0);
}

//===========================================================================
void MemReset2() {
    // TODO: Call this from an EmulatorReset function, which also initializes
    // the CPU.

    memset(s_memMain,   0, 0x10000);
    memset(s_memAux,    0, 0x10000);
    memset(s_memNull,   0, 0x100);
    memset(g_pageRead,  0, sizeof(g_pageRead));
    memset(g_pageWrite, 0, sizeof(g_pageWrite));

    // Initialize I/O switches and update memory page tables.
    s_switches = SF_BANK2 | SF_HRAMWRT;
    InitializePageTables();

    // Initialize the CPU.
    CpuInitialize();
}
