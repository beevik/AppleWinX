/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

constexpr int INDICATOR_LAG = 200000;
constexpr int DRIVES        = 2;
constexpr int NIBBLES       = 6384;
constexpr int TRACKS        = 35;

struct floppy {
    char   imageName[16];
    HIMAGE imageHandle;
    int    track;
    LPBYTE trackImage;
    int    phase;
    int    byte;
    bool   writeProtected;
    BOOL   trackImageData;
    BOOL   trackImageDirty;
    DWORD  spinning;
    DWORD  writeLight;
    int    nibbles;
};

bool g_optEnhancedDisk = true;

static int    s_currDrive;
static bool   s_diskAccessed;
static floppy s_floppyDrive[DRIVES];
static BYTE   s_floppyLatch;
static bool   s_floppyMotorOn;
static bool   s_floppyWriteMode;

static uint8_t IoSwitchDisk2(uint8_t address, bool write, uint8_t value);
static void    ReadTrack(int drive);
static void    RemoveDisk(int drive);
static void    WriteTrack(int drive);

//===========================================================================
static void CheckSpinning() {
    DWORD modechange = (s_floppyMotorOn && !s_floppyDrive[s_currDrive].spinning);
    if (s_floppyMotorOn)
        s_floppyDrive[s_currDrive].spinning = INDICATOR_LAG;
    if (modechange)
        FrameRefreshStatus();
}

//===========================================================================
static void GetImageTitle(const char * imagefilename, char * imagename, size_t imagenamechars) {
    char         imagetitle[128] = "";
    const char * startpos        = imagefilename;
    while (StrChr(startpos, '\\'))
        startpos = StrChr(startpos, '\\') + 1;
    StrCopy(imagetitle, startpos, ARRSIZE(imagetitle));
    if (imagetitle[0]) {
        char * dot = imagetitle;
        while (StrChr(dot + 1, '.'))
            dot = StrChr(dot + 1, '.');
        if (dot > imagetitle)
            * dot = 0;
    }
    BOOL found = 0;
    int  loop = 0;
    while (imagetitle[loop] && !found) {
        if (IsCharLower(imagetitle[loop]))
            found = 1;
        else
            loop++;
    }
    if ((!found) && (loop > 2))
        CharLowerBuff(imagetitle + 1, StrLen(imagetitle + 1));
    StrCopy(imagename, imagetitle, imagenamechars);
    imagename[15] = 0;
}

//===========================================================================
static BOOL InsertDisk(int drive, const char * imagefilename) {
    floppy * fptr = &s_floppyDrive[drive];
    if (fptr->imageHandle)
        RemoveDisk(drive);

    memset(fptr, 0, sizeof(floppy));
    bool success = ImageOpen(
        imagefilename,
        &fptr->imageHandle,
        &fptr->writeProtected,
        FALSE
    );
    if (success)
        GetImageTitle(imagefilename, fptr->imageName, ARRSIZE(fptr->imageName));

    return success;
}

//===========================================================================
static void InstallRom(int slot) {
    MemInstallPeripheralRom(slot, "DISK2_ROM", IoSwitchDisk2);
    uint8_t * rom = MemGetSlotRomPtr();
    uint16_t offset = slot << 8;
    rom[slot + 0x4c] = 0xa9;
    rom[slot + 0x4d] = 0x00;
    rom[slot + 0x4e] = 0xea;
}

//===========================================================================
static void ReadTrack(int drive) {
    floppy * fptr = &s_floppyDrive[drive];
    if (fptr->track >= TRACKS) {
        fptr->trackImageData = 0;
        return;
    }
    if (!fptr->trackImage)
        fptr->trackImage = (LPBYTE)new BYTE[0x1A00];
    if (fptr->trackImage && fptr->imageHandle) {
        ImageReadTrack(
            fptr->imageHandle,
            fptr->track,
            fptr->phase,
            fptr->trackImage,
            &fptr->nibbles
        );
        fptr->trackImageData = (fptr->nibbles != 0);
    }
}

//===========================================================================
static void NotifyInvalidImage(const char * imagefilename) {
    FILE * file = fopen(imagefilename, "rb");
    char buffer[260 + 128];
    if (!file)
        StrPrintf(buffer, ARRSIZE(buffer), "Unable to open the file %s.", imagefilename);
    else {
        fclose(file);
        StrPrintf(
            buffer,
            ARRSIZE(buffer),
            "%s\nUnable to use the file because the disk "
            "image format is not recognized.",
            imagefilename
        );
    }
    MessageBox(frameWindow, buffer, EmulatorGetTitle(), MB_ICONEXCLAMATION);
}

//===========================================================================
static void RemoveDisk(int drive) {
    floppy * fptr = &s_floppyDrive[drive];
    if (fptr->imageHandle) {
        if (fptr->trackImage && fptr->trackImageDirty)
            WriteTrack(drive);
        ImageClose(fptr->imageHandle);
        fptr->imageHandle = (HIMAGE)0;
    }
    if (fptr->trackImage) {
        delete[] fptr->trackImage;
        fptr->trackImage = NULL;
        fptr->trackImageData = 0;
    }
}

//===========================================================================
static void RemoveStepperDelay() {
    // note: make sure this works for the latest version of prodos
    if (MemReadDword(0xba00) == 0xd0ca11a2 && MemReadByte(0xba04) == 0xfd) {
        MemWriteDword(0xba00, 0xeaeaeaea);
        MemWriteByte(0xba04, 0xea);
    }
    if (MemReadDword(0xbd9e) == 0xd08812a0 && MemReadByte(0xbda2) == 0xfd) {
        MemWriteDword(0xbd9e, 0xeaeaeaea);
        MemWriteByte(0xbda2, 0xea);
    }
}

//===========================================================================
static void WriteTrack(int drive) {
    floppy *fptr = &s_floppyDrive[drive];
    if (fptr->track >= TRACKS)
        return;
    if (fptr->trackImage && fptr->imageHandle) {
        ImageWriteTrack(
            fptr->imageHandle,
            fptr->track,
            fptr->phase,
            fptr->trackImage,
            fptr->nibbles
        );
    }
    fptr->trackImageDirty = 0;
}

//===========================================================================
static void UpdateFloppyMotorOn(bool on) {
    if (on == s_floppyMotorOn)
        return;
    s_floppyMotorOn = on;
    TimerUpdateFullSpeedSetting(FULL_SPEED_SETTING_DISK_MOTOR_ON, s_floppyMotorOn && g_optEnhancedDisk);
}


/****************************************************************************
*
*   I/O switch handlers
*
***/

//===========================================================================
static BYTE DiskControlMotor(WORD, BYTE address, BYTE, BYTE) {
    UpdateFloppyMotorOn((address & 1) != 0);
    return MemReturnRandomData(TRUE);
}

//===========================================================================
static BYTE DiskControlStepper(WORD, BYTE address, BYTE, BYTE) {
    CheckSpinning();
    if (g_optEnhancedDisk)
        RemoveStepperDelay();
    floppy * fptr = &s_floppyDrive[s_currDrive];
    if (address & 1) {
        int phase = (address >> 1) & 3;
        int direction = 0;
        if (phase == ((fptr->phase + 1) & 3))
            direction = 1;
        if (phase == ((fptr->phase + 3) & 3))
            direction = -1;
        if (direction) {
            fptr->phase = MAX(0, MIN(79, fptr->phase + direction));
            if (!(fptr->phase & 1)) {
                int newtrack = MIN(TRACKS - 1, fptr->phase >> 1);
                if (newtrack != fptr->track) {
                    if (fptr->trackImage && fptr->trackImageDirty)
                        WriteTrack(s_currDrive);
                    fptr->track = newtrack;
                    fptr->trackImageData = 0;
                }
            }
        }
    }
    return ((address & 0x0f) == 0x00) ? 0xff : MemReturnRandomData(TRUE);
}

//===========================================================================
static BYTE DiskEnable(WORD, BYTE address, BYTE, BYTE) {
    s_currDrive = address & 1;
    CheckSpinning();
    return 0;
}

//===========================================================================
static BYTE DiskReadWrite(WORD pc, BYTE, BYTE, BYTE) {
    floppy * fptr = &s_floppyDrive[s_currDrive];
    s_diskAccessed = true;
    if ((!fptr->trackImageData) && fptr->imageHandle)
        ReadTrack(s_currDrive);
    if (!fptr->trackImageData)
        return 0xFF;
    BYTE result = 0;
    if (!s_floppyWriteMode || !fptr->writeProtected) {
        if (s_floppyWriteMode) {
            if (s_floppyLatch & 0x80) {
                fptr->trackImage[fptr->byte] = s_floppyLatch;
                fptr->trackImageDirty = 1;
            }
            else
                return 0;
        }
        else {
#if 0
            if (g_optEnhancedDisk
                && (MemReadDword(pc) == 0xd5c9fb10 || MemReadDword(pc) == 0xd549fb10)
                && ((MemReadDword(pc + 4) & 0xffff00ff) != 0xaac900f0)
                && (MemReadByte(pc + 4) != 0xd0 || MemReadByte(pc + 5) == 0xf7 || MemReadByte(pc + 5) == 0xf0))
            {
                int loop = fptr->nibbles;
                while (fptr->trackImage[fptr->byte] != 0xd5 && loop--) {
                    if (++fptr->byte >= fptr->nibbles)
                        fptr->byte = 0;
                }
            }
#endif
            result = fptr->trackImage[fptr->byte];
        }
    }
    if (++fptr->byte >= fptr->nibbles)
        fptr->byte = 0;
    return result;
}

//===========================================================================
static BYTE DiskSetLatchValue(WORD, BYTE, BYTE write, BYTE value) {
    if (write)
        s_floppyLatch = value;
    return s_floppyLatch;
}

//===========================================================================
static BYTE DiskSetReadMode(WORD pc, BYTE address, BYTE write, BYTE value) {
    s_floppyWriteMode = FALSE;
    return MemReturnRandomData(s_floppyDrive[s_currDrive].writeProtected);
}

//===========================================================================
static BYTE DiskSetWriteMode(WORD pc, BYTE address, BYTE write, BYTE value) {
    s_floppyWriteMode = TRUE;
    BOOL modechange = !s_floppyDrive[s_currDrive].writeLight;
    s_floppyDrive[s_currDrive].writeLight = INDICATOR_LAG;
    if (modechange)
        FrameRefreshStatus();
    return MemReturnRandomData(TRUE);
}

//===========================================================================
static uint8_t IoSwitchDisk2(uint8_t address, bool write, uint8_t value) {
    switch (address & 0x0f) {
        case 0x8:
        case 0x9:
            return DiskControlMotor(0, address, write, value);
        case 0xa:
        case 0xb:
            return DiskEnable(0, address, write, value);
        case 0xc:
            return DiskReadWrite(0, address, write, value);
        case 0xd:
            return DiskSetLatchValue(0, address, write, value);
        case 0xe:
            return DiskSetReadMode(0, address, write, value);
        case 0xf:
            return DiskSetWriteMode(0, address, write, value);
        default:
            return DiskControlStepper(0, address, write, value);
    }
}



/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
void DiskBoot() {
    // THIS FUNCTION RELOADS A PROGRAM IMAGE IF ONE IS LOADED IN DRIVE ONE.
    // IF A DISK IMAGE OR NO IMAGE IS LOADED IN DRIVE ONE, IT DOES NOTHING.
    if (s_floppyDrive[0].imageHandle && ImageBoot(s_floppyDrive[0].imageHandle))
        UpdateFloppyMotorOn(false);
}

//===========================================================================
void DiskDestroy() {
    RemoveDisk(0);
    RemoveDisk(1);
}

//===========================================================================
void DiskGetLightStatus(int * drive1, int * drive2) {
    *drive1 = s_floppyDrive[0].spinning
        ? s_floppyDrive[0].writeLight
            ? 2
            : 1
        : 0;
    *drive2 = s_floppyDrive[1].spinning
        ? s_floppyDrive[1].writeLight
            ? 2
            : 1
        : 0;
}

//===========================================================================
const char * DiskGetName(int drive) {
    return s_floppyDrive[drive].imageName;
}

//===========================================================================
void DiskInitialize() {
    s_currDrive = 0;
    s_floppyLatch = 0;
    s_diskAccessed = s_floppyMotorOn = s_floppyWriteMode = false;

    // Check the config for slots containing Disk2 controllers.
    bool slotUsed[8] = {};
    bool installed = false;
    for (int slot = 1; slot <= 7; ++slot) {
        char slotConfig[16];
        StrPrintf(slotConfig, ARRSIZE(slotConfig), "Slot%d", slot);

        char peripheral[16];
        if (!ConfigGetString(slotConfig, peripheral, ARRSIZE(peripheral), "")) {
            slotUsed[slot] = false;
            continue;
        }

        if (!StrCmp(peripheral, "Disk][")) {
            InstallRom(slot);
            installed = true;
        }
    }

    // If no slots are configured for Disk2 controllers, find the first empty
    // slot starting from slot 6 and install one there.
    if (!installed) {
        for (int slot = 6; slot >= 4; --slot) {
            if (!slotUsed[slot]) {
                InstallRom(slot);
                char slotConfig[16];
                StrPrintf(slotConfig, ARRSIZE(slotConfig), "Slot%d", slot);
                ConfigSetString(slotConfig, "Disk][");
                break;
            }
        }
    }

    // Use config to load the most recently used disk images.
    for (int drive = 0; drive < 2; ++drive) {
        memset(&s_floppyDrive[drive], 0, sizeof(floppy));

        char startDiskConfig[16];
        StrPrintf(startDiskConfig, ARRSIZE(startDiskConfig), "Disk%d", drive);

        char filename[260];
        ConfigGetString(startDiskConfig, filename, ARRSIZE(filename), "");

        if (filename[0] != '\0') {
            bool success = InsertDisk(drive, filename);
            if (!success) {
                ConfigRemoveString(startDiskConfig);
                NotifyInvalidImage(filename);
            }
        }
    }
}

//===========================================================================
bool DiskIsSpinning() {
    return s_floppyDrive[0].spinning || s_floppyDrive[1].spinning;
}

//===========================================================================
void DiskSelect(int drive) {
    char directory[260];
    ConfigGetString("Directory", directory, ARRSIZE(directory), ".");

    char filename[260];
    filename[0] = '\0';

    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner       = frameWindow;
    ofn.hInstance       = g_instance;
    ofn.lpstrFilter     = "All Images\0*.apl;*.bin;*.do;*.dsk;*.iie;*.nib;*.po\0"
                          "Disk Images (*.bin,*.do,*.dsk,*.iie,*.nib,*.po)\0*.bin;*.do;*.dsk;*.iie;*.nib;*.po\0"
                          "All Files\0*.*\0";
    ofn.lpstrFile       = filename;
    ofn.nMaxFile        = ARRSIZE(filename);
    ofn.lpstrInitialDir = directory;
    ofn.Flags           = OFN_CREATEPROMPT | OFN_HIDEREADONLY;
    ofn.lpTemplateName  = "INSERT_DIALOG";

    if (GetOpenFileName(&ofn)) {
        if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
            StrCat(filename, ".dsk", ARRSIZE(filename));

        if (InsertDisk(drive, filename)) {
            char diskConfig[16];
            StrPrintf(diskConfig, ARRSIZE(diskConfig), "Disk%d", drive);
            ConfigSetString(diskConfig, filename);
        }
        else
            NotifyInvalidImage(filename);

        StrCopy(directory, filename, ARRSIZE(directory));
        directory[ofn.nFileOffset] = '\0';
        ConfigSetString("Directory", directory);
    }
}

//===========================================================================
void DiskUpdatePosition(DWORD cycles) {
    for (int i = 0; i < 2; ++i) {
        floppy * fptr = &s_floppyDrive[i];
        if (fptr->spinning && !s_floppyMotorOn) {
            if (!(fptr->spinning -= MIN(fptr->spinning, cycles)))
                FrameRefreshStatus();
        }
        if (s_floppyWriteMode && (s_currDrive == i) && fptr->spinning)
            fptr->writeLight = INDICATOR_LAG;
        else if (fptr->writeLight) {
            if (!(fptr->writeLight -= MIN(fptr->writeLight, cycles)))
                FrameRefreshStatus();
        }
        if (!g_optEnhancedDisk && !s_diskAccessed && fptr->spinning) {
            fptr->byte += (cycles >> 5);
            if (fptr->byte >= fptr->nibbles)
                fptr->byte -= fptr->nibbles;
        }
    }
    s_diskAccessed = false;
}
