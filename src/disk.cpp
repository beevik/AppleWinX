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
    char      imageName[64];
    Image *   image;
    int       track;
    uint8_t * trackImage;
    int       phase;
    int       byte;
    bool      motorOn;
    bool      writeProtected;
    bool      trackImageData;
    bool      trackImageDirty;
    int32_t   spinCounter;
    int32_t   writeCounter;
    int       nibbles;
};

struct Controller {
    uint8_t slot;
    bool    writeMode;
    bool    diskAccessed;
    uint8_t latchValue;
    uint8_t selectedDrive;
    uint8_t nextControllerSlot;
    Floppy  floppy[DRIVES];
};


/****************************************************************************
*
*   Variables
*
***/

bool g_optEnhancedDisk = true;

static Controller * s_controller[SLOT_MAX + 1];
static int          s_firstControllerSlot;
static int          s_lastControllerSlot;

static uint8_t IoSwitchDisk2(uint8_t address, bool write, uint8_t value);
static void    ReadTrack(Floppy * floppy);
static void    RemoveDisk(Floppy * floppy);
static void    WriteTrack(Floppy * floppy);


/****************************************************************************
*
*   Helper functions
*
***/

//===========================================================================
static void CheckSpinning(int slot) {
    Controller * controller = s_controller[slot];
    Floppy * floppy = &controller->floppy[controller->selectedDrive];
    bool modeChange = (floppy->motorOn && !floppy->spinCounter);
    if (floppy->motorOn)
        floppy->spinCounter = INDICATOR_LAG;
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
static bool InsertDisk(int slot, int drive, const char * imageFilename) {
    Controller * controller = s_controller[slot];
    Floppy * floppy = &controller->floppy[drive];
    if (floppy->image)
        RemoveDisk(floppy);

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
static void InstallDiskController(int slot) {
    // Configure the slot's floppy disk controller.
    Controller * controller = s_controller[slot] = new Controller;
    controller->slot = (uint8_t)slot;
    controller->writeMode = false;
    controller->diskAccessed = false;
    controller->latchValue = 0;
    controller->selectedDrive = 0;
    controller->nextControllerSlot = 0;
    memset(controller->floppy, 0, sizeof(controller->floppy));

    // Install the controller rom for the slot.
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
static void ReadTrack(Floppy * floppy) {
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
static void RemoveDisk(Floppy * floppy) {
    if (floppy->image) {
        if (floppy->trackImage && floppy->trackImageDirty)
            WriteTrack(floppy);
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
static void UpdateFloppyMotor(int slot, int drive, bool on) {
    Controller * controller = s_controller[slot];
    Floppy * floppy = &controller->floppy[drive];
    if (floppy->motorOn == on)
        return;

    bool anyMotorOn = floppy->motorOn = on;
    if (!on) {
        for (int slot = s_firstControllerSlot; s_controller[slot]; slot = s_controller[slot]->nextControllerSlot)
            anyMotorOn |= (s_controller[slot]->floppy[0].motorOn || s_controller[slot]->floppy[1].motorOn);
    }

    TimerUpdateFullSpeed(FULLSPEED_DISK_MOTOR_ON, anyMotorOn && g_optEnhancedDisk);
}

//===========================================================================
static void WriteTrack(Floppy * floppy) {
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
static uint8_t GetSetLatchValue(int slot, bool write, uint8_t value) {
    Controller * controller = s_controller[slot];
    if (write)
        controller->latchValue = value;
    return controller->latchValue;
}

//===========================================================================
static uint8_t ReadWriteDisk(int slot) {
    Controller * controller = s_controller[slot];
    controller->diskAccessed = true;

    Floppy * floppy = &controller->floppy[controller->selectedDrive];
    if ((!floppy->trackImageData) && floppy->image)
        ReadTrack(floppy);
    if (!floppy->trackImageData)
        return 0xff;

    uint8_t result = 0;
    if (!controller->writeMode || !floppy->writeProtected) {
        if (controller->writeMode) {
            if (controller->latchValue & 0x80) {
                floppy->trackImage[floppy->byte] = controller->latchValue;
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
static uint8_t SelectDrive(int slot, int drive) {
    UpdateFloppyMotor(slot, drive == 0 ? 1 : 0, false); // Turn off other drive motor
    s_controller[slot]->selectedDrive = drive;
    CheckSpinning(slot);
    return 0;
}

//===========================================================================
static uint8_t SetMotorOn(int slot) {
    Controller * controller = s_controller[slot];
    UpdateFloppyMotor(slot, controller->selectedDrive, true);
    return MemReturnRandomData(true);
}

//===========================================================================
static uint8_t SetMotorsOff(int slot) {
    Controller * controller = s_controller[slot];
    UpdateFloppyMotor(slot, 0, false);
    UpdateFloppyMotor(slot, 1, false);
    return MemReturnRandomData(true);
}

//===========================================================================
static uint8_t SetReadMode(int slot) {
    Controller * controller = s_controller[slot];
    controller->writeMode = false;
    return MemReturnRandomData(controller->floppy[controller->selectedDrive].writeProtected);
}

//===========================================================================
static uint8_t SetWriteMode(int slot) {
    Controller * controller = s_controller[slot];
    controller->writeMode = true;

    Floppy * floppy = &controller->floppy[controller->selectedDrive];
    bool modeChange = (floppy->writeCounter == 0);
    floppy->writeCounter = INDICATOR_LAG;
    if (modeChange)
        FrameRefreshStatus();

    return MemReturnRandomData(true);
}

//===========================================================================
static uint8_t UpdateStepper(int slot, int phase) {
    if (g_optEnhancedDisk)
        RemoveStepperDelay();

    Controller * controller = s_controller[slot];
    CheckSpinning(slot);

    Floppy * floppy = &controller->floppy[controller->selectedDrive];
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
                    WriteTrack(floppy);
                floppy->track = newTrack;
                floppy->trackImageData = false;
            }
        }
    }

    return MemReturnRandomData(true);
}

//===========================================================================
static uint8_t IoSwitchDisk2(uint8_t address, bool write, uint8_t value) {
    int slot = ((address >> 4) & 0xf) - 8;
    switch (address & 0x0f) {
        case 0x0:   return 0xff;
        case 0x1:   return UpdateStepper(slot, 0);
        case 0x2:   return MemReturnRandomData(true);
        case 0x3:   return UpdateStepper(slot, 1);
        case 0x4:   return MemReturnRandomData(true);
        case 0x5:   return UpdateStepper(slot, 2);
        case 0x6:   return MemReturnRandomData(true);
        case 0x7:   return UpdateStepper(slot, 3);
        case 0x8:   return SetMotorsOff(slot);
        case 0x9:   return SetMotorOn(slot);
        case 0xa:   return SelectDrive(slot, 0);
        case 0xb:   return SelectDrive(slot, 1);
        case 0xc:   return ReadWriteDisk(slot);
        case 0xd:   return GetSetLatchValue(slot, write, value);
        case 0xe:   return SetReadMode(slot);
        case 0xf:   return SetWriteMode(slot);
        default:    return 0;
    }
}


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
void DiskDestroy() {
    for (int slot = SLOT_MIN; slot <= SLOT_MAX; ++slot) {
        if (s_controller[slot] != nullptr) {
            RemoveDisk(&s_controller[slot]->floppy[0]);
            delete s_controller[slot];
            s_controller[slot] = nullptr;
        }
    }
    s_firstControllerSlot = 0;
    s_lastControllerSlot = 0;
}

//===========================================================================
void DiskGetStatus(EDiskStatus * statusDrive1, EDiskStatus * statusDrive2) {
    *statusDrive1 = *statusDrive2 = DISKSTATUS_OFF;

    for (int slot = s_firstControllerSlot; s_controller[slot]; slot = s_controller[slot]->nextControllerSlot) {
        Controller * controller = s_controller[slot];
        if (controller->floppy[0].spinCounter > 0) {
            if (controller->floppy[0].writeCounter > 0)
                *statusDrive1 = DISKSTATUS_WRITE;
            else if (*statusDrive1 == DISKSTATUS_OFF)
                *statusDrive1 = DISKSTATUS_READ;
        }
        if (controller->floppy[1].spinCounter > 0) {
            if (controller->floppy[1].writeCounter > 0)
                *statusDrive2 = DISKSTATUS_WRITE;
            else if (*statusDrive2 == DISKSTATUS_OFF)
                *statusDrive2 = DISKSTATUS_READ;
        }
    }
}

//===========================================================================
const char * DiskGetName(int drive) {
    Controller * controller = s_controller[s_lastControllerSlot];
    if (!controller)
        return "";
    return controller->floppy[drive].imageName;
}

//===========================================================================
void DiskInitialize() {
    // Check the config for slots containing Disk2 controllers.
    bool slotUsed[SLOT_MAX + 1] = {};
    bool installed = false;
    int  prevSlot = 0;
    for (int slot = SLOT_MIN; slot <= SLOT_MAX; ++slot) {
        char slotConfig[16];
        StrPrintf(slotConfig, ARRSIZE(slotConfig), "Slot%d", slot);

        char peripheral[16];
        if (!ConfigGetString(slotConfig, peripheral, ARRSIZE(peripheral), "")) {
            slotUsed[slot] = false;
            continue;
        }

        if (!StrCmp(peripheral, "Disk][")) {
            InstallDiskController(slot);
            if (prevSlot)
                s_controller[prevSlot]->nextControllerSlot = (uint8_t)slot;
            installed = true;
            if (s_firstControllerSlot == 0)
                s_firstControllerSlot = slot;
            prevSlot = slot;
        }
    }
    s_lastControllerSlot = prevSlot;

    // If no slots are configured for Disk2 controllers, find the first empty
    // slot starting from slot 6 and install one there.
    if (!installed) {
        for (int slot = 6; slot >= 4; --slot) {
            if (!slotUsed[slot]) {
                InstallDiskController(slot);
                s_firstControllerSlot = s_lastControllerSlot = slot;
                char slotConfig[16];
                StrPrintf(slotConfig, ARRSIZE(slotConfig), "Slot%d", slot);
                ConfigSetString(slotConfig, "Disk][");
                installed = true;
                break;
            }
        }
    }

    // Use config to load the most recently used disk images.
    for (int slot = s_firstControllerSlot; s_controller[slot]; slot = s_controller[slot]->nextControllerSlot) {
        Controller * controller = s_controller[slot];
        for (int drive = 0; drive < 2; ++drive) {
            char startDiskConfig[16];
            StrPrintf(startDiskConfig, ARRSIZE(startDiskConfig), "Disk%d,%d", slot, drive);

            char filename[260];
            ConfigGetString(startDiskConfig, filename, ARRSIZE(filename), "");

            if (filename[0] != '\0') {
                bool success = InsertDisk(slot, drive, filename);
                if (!success) {
                    ConfigRemoveString(startDiskConfig);
                    NotifyInvalidImage(filename);
                }
            }
        }
    }
}

//===========================================================================
bool DiskIsSpinning() {
    for (int slot = s_firstControllerSlot; s_controller[slot]; slot = s_controller[slot]->nextControllerSlot) {
        Controller * controller = s_controller[slot];
        if (controller->floppy[0].spinCounter > 0 || controller->floppy[1].spinCounter > 0)
            return true;
    }
    return false;
}

//===========================================================================
void DiskSelect(int drive) {
    if (s_lastControllerSlot == 0)
        return;

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

        if (InsertDisk(s_lastControllerSlot, drive, filename)) {
            char diskConfig[16];
            StrPrintf(diskConfig, ARRSIZE(diskConfig), "Disk%d,%d", s_lastControllerSlot, drive);
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
    for (int slot = s_firstControllerSlot; s_controller[slot]; slot = s_controller[slot]->nextControllerSlot) {
        Controller * controller = s_controller[slot];
        for (int drive = 0; drive < 2; ++drive) {
            Floppy * floppy = &controller->floppy[drive];
            if (floppy->spinCounter && !floppy->motorOn) {
                floppy->spinCounter = MAX(0, floppy->spinCounter - cyclesSinceLastUpdate);
                if (floppy->spinCounter == 0)
                    FrameRefreshStatus();
            }
            if (controller->writeMode && (controller->selectedDrive == drive) && floppy->spinCounter)
                floppy->writeCounter = INDICATOR_LAG;
            else if (floppy->writeCounter) {
                floppy->writeCounter = MAX(0, floppy->writeCounter - cyclesSinceLastUpdate);
                if (floppy->writeCounter == 0)
                    FrameRefreshStatus();
            }
            if (!g_optEnhancedDisk && !controller->diskAccessed && floppy->spinCounter) {
                floppy->byte += (cyclesSinceLastUpdate >> 5);
                if (floppy->byte >= floppy->nibbles)
                    floppy->byte -= floppy->nibbles;
            }
        }
        controller->diskAccessed = false;
    }
}
