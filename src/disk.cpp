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
*   Constants and types
*
***/

constexpr int INDICATOR_LAG = 200000;
constexpr int DRIVES        = 2;
constexpr int NIBBLES       = 6384;
constexpr int TRACKS        = 35;

struct Floppy {
    char      imageName[128];
    Image *   image;
    int       track;
    uint8_t * trackImage;
    int       phase;
    int       byte;
    bool      writeProtected;
    bool      trackImageData;
    bool      trackImageDirty;
    int32_t   spinCounter;
    int32_t   writeCounter;
    int       nibbles;
};


/****************************************************************************
*
*   Variables
*
***/

bool g_optEnhancedDisk = true;

static int     s_currDrive;
static bool    s_diskAccessed;
static Floppy  s_floppyDrive[DRIVES];
static uint8_t s_floppyLatch;
static bool    s_floppyMotorOn;
static bool    s_floppyWriteMode;

static uint8_t IoSwitchDisk2(uint8_t address, bool write, uint8_t value);
static void    ReadTrack(int drive);
static void    RemoveDisk(int drive);
static void    WriteTrack(int drive);


/****************************************************************************
*
*   Helper functions
*
***/

//===========================================================================
static void CheckSpinning() {
    bool modeChange = (s_floppyMotorOn && !s_floppyDrive[s_currDrive].spinCounter);
    if (s_floppyMotorOn)
        s_floppyDrive[s_currDrive].spinCounter = INDICATOR_LAG;
    if (modeChange)
        FrameRefreshStatus();
}

//===========================================================================
static void GetImageTitle(const char * filename, char * imageName, size_t imageNameChars) {
    const char * startPos = filename;
    {
        const char * ptr;
        while ((ptr = StrChrConst(startPos, '\\')) != nullptr)
            startPos = ptr + 1;
    }

    // Strip the directory and extension.
    char imageTitle[128];
    StrCopy(imageTitle, startPos, ARRSIZE(imageTitle));
    if (imageTitle[0]) {
        char * dot = imageTitle;
        char * ptr;
        while ((ptr = StrChr(dot + 1, '.')) != nullptr)
            dot = ptr;
        if (dot > imageTitle)
            *dot = '\0';
    }

    // Convert all-caps disk names to mixed case.
    bool isAllCaps = true;
    int indexFirstLowercase = 0;
    for (; imageTitle[indexFirstLowercase]; ++indexFirstLowercase) {
        if (imageTitle[indexFirstLowercase] >= 'a' && imageTitle[indexFirstLowercase] <= 'z') {
            isAllCaps = false;
            break;
        }
    }
    if (isAllCaps && indexFirstLowercase > 1) {
        for (int i = 1; imageTitle[i] != '\0'; ++i)
            imageTitle[i] = (imageTitle[i] >= 'A' && imageTitle[i] <= 'Z')
                ? imageTitle[i] - 'A' + 'a'
                : imageTitle[i];
    }

    StrCopy(imageName, imageTitle, imageNameChars);
}

//===========================================================================
static bool InsertDisk(int drive, const char * imageFilename) {
    Floppy * floppy = &s_floppyDrive[drive];
    if (floppy->image)
        RemoveDisk(drive);

    memset(floppy, 0, sizeof(Floppy));
    bool success = ImageOpen(
        imageFilename,
        &floppy->image,
        &floppy->writeProtected
    );
    if (success)
        GetImageTitle(imageFilename, floppy->imageName, ARRSIZE(floppy->imageName));

    return success;
}

//===========================================================================
static void InstallRom(int slot) {
    MemInstallPeripheralRom(slot, "DISK2_ROM", IoSwitchDisk2);

    // Remove wait call in Disk ][ controller ROM.
    if (g_optEnhancedDisk) {
        uint8_t * rom = MemGetSlotRomPtr();
        uint16_t offset = slot << 8;
        rom[offset + 0x4c] = 0xa9;
        rom[offset + 0x4d] = 0x00;
        rom[offset + 0x4e] = 0xea;
    }
}

//===========================================================================
static void ReadTrack(int drive) {
    Floppy * floppy = &s_floppyDrive[drive];
    if (floppy->track >= TRACKS) {
        floppy->trackImageData = false;
        return;
    }
    if (!floppy->trackImage)
        floppy->trackImage = new uint8_t[0x1a00];
    if (floppy->trackImage && floppy->image) {
        ImageReadTrack(
            floppy->image,
            floppy->track,
            floppy->phase,
            floppy->trackImage,
            &floppy->nibbles
        );
        floppy->trackImageData = (floppy->nibbles != 0);
    }
}

//===========================================================================
static void NotifyInvalidImage(const char * imageFilename) {
    FILE * file = fopen(imageFilename, "rb");
    char buffer[260 + 128];
    if (!file)
        StrPrintf(buffer, ARRSIZE(buffer), "Unable to open the file %s.", imageFilename);
    else {
        fclose(file);
        StrPrintf(
            buffer,
            ARRSIZE(buffer),
            "%s\nUnable to use the file because the disk "
            "image format is not recognized.",
            imageFilename
        );
    }
    MessageBox(frameWindow, buffer, EmulatorGetTitle(), MB_ICONEXCLAMATION);
}

//===========================================================================
static void RemoveDisk(int drive) {
    Floppy * floppy = &s_floppyDrive[drive];
    if (floppy->image) {
        if (floppy->trackImage && floppy->trackImageDirty)
            WriteTrack(drive);
        ImageClose(floppy->image);
        floppy->image = nullptr;
    }
    if (floppy->trackImage) {
        delete[] floppy->trackImage;
        floppy->trackImage = nullptr;
        floppy->trackImageData = false;
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
static void UpdateFloppyMotorOn(bool on) {
    if (on == s_floppyMotorOn)
        return;
    s_floppyMotorOn = on;
    TimerUpdateFullSpeed(FULLSPEED_DISK_MOTOR_ON, s_floppyMotorOn && g_optEnhancedDisk);
}

//===========================================================================
static void WriteTrack(int drive) {
    Floppy * floppy = &s_floppyDrive[drive];
    if (floppy->track >= TRACKS)
        return;
    if (floppy->trackImage && floppy->image) {
        ImageWriteTrack(
            floppy->image,
            floppy->track,
            floppy->phase,
            floppy->trackImage,
            floppy->nibbles
        );
    }
    floppy->trackImageDirty = false;
}


/****************************************************************************
*
*   I/O switch handlers
*
***/

//===========================================================================
static uint8_t EnableMotor(bool on) {
    UpdateFloppyMotorOn(on);
    return MemReturnRandomData(true);
}

//===========================================================================
static uint8_t EnableDisk(int drive) {
    s_currDrive = drive;
    CheckSpinning();
    return 0;
}

//===========================================================================
static uint8_t GetSetLatchValue(bool write, uint8_t value) {
    if (write)
        s_floppyLatch = value;
    return s_floppyLatch;
}

//===========================================================================
static uint8_t ReadWriteDisk() {
    s_diskAccessed = true;

    Floppy * floppy = &s_floppyDrive[s_currDrive];
    if ((!floppy->trackImageData) && floppy->image)
        ReadTrack(s_currDrive);
    if (!floppy->trackImageData)
        return 0xff;

    uint8_t result = 0;
    if (!s_floppyWriteMode || !floppy->writeProtected) {
        if (s_floppyWriteMode) {
            if (s_floppyLatch & 0x80) {
                floppy->trackImage[floppy->byte] = s_floppyLatch;
                floppy->trackImageDirty = true;
            }
            else
                return 0;
        }
        else
            result = floppy->trackImage[floppy->byte];
    }
    if (++floppy->byte >= floppy->nibbles)
        floppy->byte = 0;
    return result;
}

//===========================================================================
static uint8_t SetReadMode() {
    s_floppyWriteMode = false;
    return MemReturnRandomData(s_floppyDrive[s_currDrive].writeProtected);
}

//===========================================================================
static uint8_t SetWriteMode() {
    s_floppyWriteMode = true;

    bool modeChange = (s_floppyDrive[s_currDrive].writeCounter == 0);
    s_floppyDrive[s_currDrive].writeCounter = INDICATOR_LAG;
    if (modeChange)
        FrameRefreshStatus();

    return MemReturnRandomData(true);
}

//===========================================================================
static uint8_t UpdateStepper(int phase) {
    if (g_optEnhancedDisk)
        RemoveStepperDelay();

    CheckSpinning();

    Floppy * floppy = &s_floppyDrive[s_currDrive];
    int direction = 0;
    if (phase == ((floppy->phase + 1) & 3))
        direction = 1;
    if (phase == ((floppy->phase + 3) & 3))
        direction = -1;

    if (direction != 0) {
        floppy->phase = MAX(0, MIN(79, floppy->phase + direction));
        if ((floppy->phase & 1) == 0) {
            int newTrack = MIN(TRACKS - 1, floppy->phase >> 1);
            if (newTrack != floppy->track) {
                if (floppy->trackImage && floppy->trackImageDirty)
                    WriteTrack(s_currDrive);
                floppy->track = newTrack;
                floppy->trackImageData = false;
            }
        }
    }

    return MemReturnRandomData(true);
}

//===========================================================================
static uint8_t IoSwitchDisk2(uint8_t address, bool write, uint8_t value) {
    switch (address & 0x0f) {
        case 0x0:   return 0xff;
        case 0x1:   return UpdateStepper(0);
        case 0x2:   return MemReturnRandomData(true);
        case 0x3:   return UpdateStepper(1);
        case 0x4:   return MemReturnRandomData(true);
        case 0x5:   return UpdateStepper(2);
        case 0x6:   return MemReturnRandomData(true);
        case 0x7:   return UpdateStepper(3);
        case 0x8:   return EnableMotor(false);
        case 0x9:   return EnableMotor(true);
        case 0xa:   return EnableDisk(0);
        case 0xb:   return EnableDisk(1);
        case 0xc:   return ReadWriteDisk();
        case 0xd:   return GetSetLatchValue(write, value);
        case 0xe:   return SetReadMode();
        case 0xf:   return SetWriteMode();
        default:    return 0;
    }
}


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
void DiskBoot() {
    // This function reloads a program image if one is loaded in drive one.
    // if a disk image or no image is loaded in drive one, it does nothing.
    if (s_floppyDrive[0].image && ImageBoot(s_floppyDrive[0].image))
        UpdateFloppyMotorOn(false);
}

//===========================================================================
void DiskDestroy() {
    RemoveDisk(0);
    RemoveDisk(1);
}

//===========================================================================
void DiskGetStatus(EDiskStatus * statusDrive1, EDiskStatus * statusDrive2) {
    *statusDrive1 = s_floppyDrive[0].spinCounter
        ? s_floppyDrive[0].writeCounter
            ? DISKSTATUS_WRITE
            : DISKSTATUS_READ
        : DISKSTATUS_OFF;
    *statusDrive2 = s_floppyDrive[1].spinCounter
        ? s_floppyDrive[1].writeCounter
            ? DISKSTATUS_WRITE
            : DISKSTATUS_READ
        : DISKSTATUS_OFF;
}

//===========================================================================
const char * DiskGetName(int drive) {
    return s_floppyDrive[drive].imageName;
}

//===========================================================================
void DiskInitialize() {
    s_currDrive    = 0;
    s_floppyLatch  = 0;
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
        memset(&s_floppyDrive[drive], 0, sizeof(Floppy));

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
    return s_floppyDrive[0].spinCounter > 0 || s_floppyDrive[1].spinCounter > 0;
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
void DiskUpdatePosition(int cyclesSinceLastUpdate) {
    for (int i = 0; i < 2; ++i) {
        Floppy * floppy = &s_floppyDrive[i];
        if (floppy->spinCounter && !s_floppyMotorOn) {
            floppy->spinCounter = MAX(0, floppy->spinCounter - cyclesSinceLastUpdate);
            if (floppy->spinCounter == 0)
                FrameRefreshStatus();
        }
        if (s_floppyWriteMode && (s_currDrive == i) && floppy->spinCounter)
            floppy->writeCounter = INDICATOR_LAG;
        else if (floppy->writeCounter) {
            floppy->writeCounter = MAX(0, floppy->writeCounter - cyclesSinceLastUpdate);
            if (floppy->writeCounter == 0)
                FrameRefreshStatus();
        }
        if (!g_optEnhancedDisk && !s_diskAccessed && floppy->spinCounter) {
            floppy->byte += (cyclesSinceLastUpdate >> 5);
            if (floppy->byte >= floppy->nibbles)
                floppy->byte -= floppy->nibbles;
        }
    }
    s_diskAccessed = false;
}
