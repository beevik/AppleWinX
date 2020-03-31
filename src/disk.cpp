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
constexpr int MAX_TRACKS    = 40;

struct Drive {
    Image *   image;
    uint8_t * trackBuffer;
    uint8_t   steppersEnabled;
    bool      motorOn;
    bool      writeProtected;
    bool      hasTrackData;
    bool      trackDirty;
    int       trackNibbles;
    int       quarterTrack;
    int       nibbleCounter;
    int       spinCounter;
    int       writeCounter;
};

struct Controller {
    uint8_t slot;
    bool    writeMode;
    bool    diskAccessed;
    uint8_t dataRegister;
    uint8_t selectedDrive;
    uint8_t nextControllerSlot;
    Drive   drive[DRIVES];
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

static inline Drive * GetSelectedDrive(Controller * controller);
static uint8_t        IoSwitchDisk2(uint8_t address, bool write, uint8_t value);
static void           ReadTrack(Drive * drive);
static void           RemoveDisk(Drive * drive);
static void           WriteTrack(Drive * drive);

// Stepper phase bits -> quarter-track target (mod 8)
//
// If phases 0+2 are on, they cancel out. Same for 1+3.
//
//       Phase      Q Target
//      3 2 1 0     76543210
//      =======     ========
//      0 0 0 0     --------
//      0 0 0 1     -------0
//      0 0 1 0     -----2--
//      0 0 1 1     ------1-
//      0 1 0 0     ---4----
//      0 1 0 1     --------
//      0 1 1 0     ----3---
//      0 1 1 1     -----2--
//      1 0 0 0     -6------
//      1 0 0 1     7-------
//      1 0 1 0     --------
//      1 0 1 1     -------0
//      1 1 0 0     --5-----
//      1 1 0 1     -6------
//      1 1 1 0     ---4----
//      1 1 1 1     --------

static const int8_t s_quarterTarget[]     = { -1, 0, 2, 1, 4, -1, 3, 2, 6, 7, -1, 0, 5, 6, 4, -1 };
static const int8_t s_quarterDiffToStep[] = { 0, +1, +2, +3, 0, -3, -2, -1 };


/****************************************************************************
*
*   Helper functions
*
***/

//===========================================================================
static void CheckSpinning(Controller * controller) {
    Drive * drive = GetSelectedDrive(controller);
    bool modeChange = (drive->motorOn && !drive->spinCounter);
    if (drive->motorOn)
        drive->spinCounter = INDICATOR_LAG;
    if (modeChange)
        FrameRefreshStatus();
}

//===========================================================================
static inline Drive * GetSelectedDrive(Controller * controller) {
    return &controller->drive[controller->selectedDrive];
}

//===========================================================================
static bool InsertDisk(Controller * controller, int driveNum, const char * imageFilename) {
    Drive * drive = &controller->drive[driveNum];
    if (drive->image)
        RemoveDisk(drive);

    memset(drive, 0, sizeof(Drive));
    return ImageOpen(imageFilename, &drive->image, &drive->writeProtected);
}

//===========================================================================
static void InstallController(int slot) {
    // Configure the slot's floppy disk controller.
    Controller * controller = s_controller[slot] = new Controller;
    controller->slot               = (uint8_t)slot;
    controller->writeMode          = false;
    controller->diskAccessed       = false;
    controller->dataRegister       = 0;
    controller->selectedDrive      = 0;
    controller->nextControllerSlot = 0;
    memset(controller->drive, 0, sizeof(controller->drive));

    // Install the controller rom for the slot.
    MemInstallPeripheralRom(slot, "DISK2_ROM", IoSwitchDisk2);

    // Remove wait call in Disk ][ controller ROM.
    if (g_optEnhancedDisk) {
        uint8_t * rom = MemGetSlotRomPtr();
        uint16_t offset = slot << 8;
        rom[offset + 0x4c] = 0xa9; // replace JSR $FCA8 (WAIT) with LDA #$00; NOP
        rom[offset + 0x4d] = 0x00;
        rom[offset + 0x4e] = 0xea;
    }
}

//===========================================================================
static void ReadTrack(Drive * drive) {
    if (!drive->trackBuffer)
        drive->trackBuffer = new uint8_t[0x1a00];

    if (drive->image) {
        ImageReadTrack(
            drive->image,
            drive->quarterTrack,
            drive->trackBuffer,
            &drive->trackNibbles
        );
        drive->hasTrackData = (drive->trackNibbles != 0);
    }
}

//===========================================================================
static void NotifyInvalidImage(const char * imageFilename) {
    char buffer[260 + 128];
    FILE * file = fopen(imageFilename, "rb");
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
static void RemoveDisk(Drive * drive) {
    if (drive->image) {
        if (drive->trackBuffer && drive->trackDirty)
            WriteTrack(drive);
        ImageClose(drive->image);
        drive->image = nullptr;
    }
    if (drive->trackBuffer) {
        delete[] drive->trackBuffer;
        drive->trackBuffer = nullptr;
        drive->hasTrackData = false;
    }
}

//===========================================================================
static void RemoveStepperDelay() {
    // Patch DOS to remove head stepper positioning delays.
    // NOTE: make sure this works for the latest version of ProDOS.

    if (MemReadDword(0xba00) == 0xd0ca11a2 && MemReadByte(0xba04) == 0xfd) {
        // Replace the following 87 cycle delay with 5 NOPs (10 cycles):
        //   BA00: LDX #$11
        //   BA02: DEX
        //   BA03: BNE $BA02

        MemWriteDword(0xba00, 0xeaeaeaea);
        MemWriteByte(0xba04, 0xea);
    }
    if (MemReadDword(0xbd9e) == 0xd08812a0 && MemReadByte(0xbda2) == 0xfd) {
        // Replace the following 92 cycle delay with 5 NOPs (10 cycles):
        //   BD9E: LDY #$12
        //   BDA0: DEY
        //   BDA1: BNE $BDA0

        MemWriteDword(0xbd9e, 0xeaeaeaea);
        MemWriteByte(0xbda2, 0xea);
    }
}

//===========================================================================
static void UpdateDriveMotor(Controller * controller, int driveNum, bool on) {
    Drive * drive = &controller->drive[driveNum];
    if (drive->motorOn == on)
        return;

    bool anyMotorOn = drive->motorOn = on;
    if (!on) {
        for (int slot = s_firstControllerSlot; s_controller[slot]; slot = s_controller[slot]->nextControllerSlot)
            anyMotorOn |= (s_controller[slot]->drive[0].motorOn || s_controller[slot]->drive[1].motorOn);
    }

    TimerUpdateFullSpeed(FULLSPEED_DISK_MOTOR_ON, anyMotorOn && g_optEnhancedDisk);
}

//===========================================================================
static void WriteTrack(Drive * drive) {
    if (drive->trackBuffer && drive->image) {
        ImageWriteTrack(
            drive->image,
            drive->quarterTrack,
            drive->trackBuffer,
            drive->trackNibbles
        );
    }
    drive->trackDirty = false;
}


/****************************************************************************
*
*   I/O switch handlers
*
***/

//===========================================================================
static inline uint8_t GetDataRegister(Controller * controller) {
    return controller->dataRegister;
}

//===========================================================================
static uint8_t Load_ReadWriteProtect(Controller * controller, bool write, uint8_t value) {
    // In write mode, load contents of the data bus into the controller data
    // register. In read mode, shift the write-protect bit into the MSB of the
    // data register.

    if (controller->writeMode) {
        if (write)
            controller->dataRegister = value;
        else
            controller->dataRegister = MemReturnRandomData(false); // undefined data bus contents
    }
    else {
        controller->dataRegister = GetSelectedDrive(controller)->writeProtected ? 0xff : 0x00;
    }
    return MemReturnRandomData(false);
}

//===========================================================================
static uint8_t Shift_Read(Controller * controller) {
    // In write mode, shift-left the contents of the controller data register.
    // In read mode, load data from the controller data register.

    controller->diskAccessed = true;

    Drive * drive = GetSelectedDrive(controller);
    if (!drive->hasTrackData && drive->image)
        ReadTrack(drive);
    if (!drive->hasTrackData)
        return 0xff;

    uint8_t result = 0;
    if (!controller->writeMode || !drive->writeProtected) {
        if (controller->writeMode) {
            if (controller->dataRegister & 0x80) {
                drive->trackBuffer[drive->nibbleCounter] = controller->dataRegister;
                drive->trackDirty = true;
            }
        }
        else
            result = drive->trackBuffer[drive->nibbleCounter];
    }

    ++drive->nibbleCounter;
    if (drive->nibbleCounter >= drive->trackNibbles)
        drive->nibbleCounter = 0;

    return result;
}

//===========================================================================
static uint8_t SelectDrive(Controller * controller, int driveNum) {
    UpdateDriveMotor(controller, driveNum == 0 ? 1 : 0, false); // Turn off other drive motor
    controller->selectedDrive = driveNum;
    CheckSpinning(controller);
    return driveNum == 0 ? controller->dataRegister : MemReturnRandomData(false);
}

//===========================================================================
static uint8_t SetMotorOn(Controller * controller) {
    UpdateDriveMotor(controller, controller->selectedDrive, true);
    return MemReturnRandomData(false);
}

//===========================================================================
static uint8_t SetMotorsOff(Controller * controller) {
    UpdateDriveMotor(controller, 0, false);
    UpdateDriveMotor(controller, 1, false);
    return controller->dataRegister;
}

//===========================================================================
static uint8_t SetReadMode(Controller * controller) {
    controller->writeMode = false;
    return controller->dataRegister;
}

//===========================================================================
static uint8_t SetWriteMode(Controller * controller) {
    controller->writeMode = true;

    // Update write-indicator display counter
    Drive * drive = GetSelectedDrive(controller);
    bool modeChange = (drive->writeCounter == 0);
    drive->writeCounter = INDICATOR_LAG;
    if (modeChange)
        FrameRefreshStatus();

    return MemReturnRandomData(true);
}

//===========================================================================
static uint8_t UpdateSteppingMotor(Controller * controller, int phase, bool on) {
    if (g_optEnhancedDisk)
        RemoveStepperDelay();

    // Update the enabled steppers.
    Drive * drive = GetSelectedDrive(controller);
    if (on)
        drive->steppersEnabled |= (1 << phase);
    else
        drive->steppersEnabled &= ~(1 << phase);

    // Convert the enabled steppers into a quarter-track target, then
    // determine how many quarter-track steps are needed to reach it.
    int quarterStep = 0;
    int quarterTarget = s_quarterTarget[drive->steppersEnabled];
    if (quarterTarget >= 0) {
        int quarterDiff = (quarterTarget - drive->quarterTrack + 8) & 7;
        quarterStep = s_quarterDiffToStep[quarterDiff];
    }

    // Update the current quarter-track. Flush any unwritten data on the
    // current quarter-track first.
    if (quarterStep != 0) {
        int newQuarterTrack = MAX(0, MIN(MAX_TRACKS * 4 - 1, drive->quarterTrack + quarterStep));
        if (newQuarterTrack != drive->quarterTrack) {
            if (drive->trackDirty && drive->trackBuffer)
                WriteTrack(drive);
            drive->quarterTrack = newQuarterTrack;
            drive->hasTrackData = false;
        }

        if (on)
            CheckSpinning(controller);
    }

    if (on)
        return MemReturnRandomData(true);
    else
        return controller->dataRegister;
}

//===========================================================================
static uint8_t IoSwitchDisk2(uint8_t address, bool write, uint8_t value) {
    int slot = ((address >> 4) & 0xf) - 8;
    Controller * controller = s_controller[slot];
    if (!controller)
        return MemReturnRandomData(true);

    switch (address & 0x0f) {
        case 0x0:   return UpdateSteppingMotor(controller, 0, false);
        case 0x1:   return UpdateSteppingMotor(controller, 0, true);
        case 0x2:   return UpdateSteppingMotor(controller, 1, false);
        case 0x3:   return UpdateSteppingMotor(controller, 1, true);
        case 0x4:   return UpdateSteppingMotor(controller, 2, false);
        case 0x5:   return UpdateSteppingMotor(controller, 2, true);
        case 0x6:   return UpdateSteppingMotor(controller, 3, false);
        case 0x7:   return UpdateSteppingMotor(controller, 3, true);
        case 0x8:   return SetMotorsOff(controller);
        case 0x9:   return SetMotorOn(controller);
        case 0xa:   return SelectDrive(controller, 0);
        case 0xb:   return SelectDrive(controller, 1);
        case 0xc:   return Shift_Read(controller);
        case 0xd:   return Load_ReadWriteProtect(controller, write, value);
        case 0xe:   return SetReadMode(controller);
        case 0xf:   return SetWriteMode(controller);
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
            RemoveDisk(&s_controller[slot]->drive[0]);
            delete s_controller[slot];
            s_controller[slot] = nullptr;
        }
    }
    s_firstControllerSlot = s_lastControllerSlot = 0;
}

//===========================================================================
void DiskGetStatus(EDiskStatus * statusDrive1, EDiskStatus * statusDrive2) {
    *statusDrive1 = *statusDrive2 = DISKSTATUS_OFF;

    for (int slot = s_firstControllerSlot; s_controller[slot]; slot = s_controller[slot]->nextControllerSlot) {
        Controller * controller = s_controller[slot];
        if (controller->drive[0].spinCounter > 0) {
            if (controller->drive[0].writeCounter > 0)
                *statusDrive1 = DISKSTATUS_WRITE;
            else if (*statusDrive1 == DISKSTATUS_OFF)
                *statusDrive1 = DISKSTATUS_READ;
        }
        if (controller->drive[1].spinCounter > 0) {
            if (controller->drive[1].writeCounter > 0)
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
    Image * image = controller->drive[drive].image;
    if (image == nullptr)
        return "";
    return ImageGetName(image);
}

//===========================================================================
void DiskInitialize() {
    // Check the config for slots containing Disk2 controllers.
    bool installed = false;
    int  prevSlot = 0;
    for (int slot = SLOT_MIN; slot <= SLOT_MAX; ++slot) {
        char slotConfig[16];
        StrPrintf(slotConfig, ARRSIZE(slotConfig), "Slot%d", slot);

        char peripheral[16];
        if (!ConfigGetString(slotConfig, peripheral, ARRSIZE(peripheral), ""))
            continue;

        if (!StrCmp(peripheral, "Disk][")) {
            InstallController(slot);
            if (prevSlot)
                s_controller[prevSlot]->nextControllerSlot = (uint8_t)slot;
            if (s_firstControllerSlot == 0)
                s_firstControllerSlot = slot;
            prevSlot = slot;
            installed = true;
        }
    }
    s_lastControllerSlot = prevSlot;

    // If no slots are configured for Disk2 controllers, find the first empty
    // slot starting from slot 6 and install one there.
    if (!installed) {
        for (int slot = 6; slot >= 4; --slot) {
            if (!s_controller[slot]) {
                InstallController(slot);
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
        for (int drive = 0; drive < DRIVES; ++drive) {
            char diskConfig[16];
            StrPrintf(diskConfig, ARRSIZE(diskConfig), "Disk%d,%d", slot, drive);

            char filename[260];
            ConfigGetString(diskConfig, filename, ARRSIZE(filename), "");

            if (filename[0] != '\0') {
                bool success = InsertDisk(controller, drive, filename);
                if (!success) {
                    ConfigRemoveString(diskConfig);
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
        if (controller->drive[0].spinCounter > 0 || controller->drive[1].spinCounter > 0)
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
    ofn.lpstrFilter     = "All Images\0*.dsk;*.do;*.nib;*.po\0"
                          "Disk Images (*.dsk,*.do*.nib,*.po)\0*.dsk;*.do;*.nib;*.po\0"
                          "All Files\0*.*\0";
    ofn.lpstrFile       = filename;
    ofn.nMaxFile        = ARRSIZE(filename);
    ofn.lpstrInitialDir = directory;
    ofn.Flags           = OFN_CREATEPROMPT | OFN_HIDEREADONLY;
    ofn.lpTemplateName  = "INSERT_DIALOG";

    if (GetOpenFileName(&ofn)) {
        if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
            StrCat(filename, ".dsk", ARRSIZE(filename));

        if (InsertDisk(s_controller[s_lastControllerSlot], drive, filename)) {
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
        for (int driveNum = 0; driveNum < DRIVES; ++driveNum) {
            Drive * drive = &controller->drive[driveNum];
            if (drive->spinCounter && !drive->motorOn) {
                drive->spinCounter = MAX(0, drive->spinCounter - cyclesSinceLastUpdate);
                if (drive->spinCounter == 0)
                    FrameRefreshStatus();
            }
            if (controller->writeMode && (controller->selectedDrive == driveNum) && drive->spinCounter)
                drive->writeCounter = INDICATOR_LAG;
            else if (drive->writeCounter) {
                drive->writeCounter = MAX(0, drive->writeCounter - cyclesSinceLastUpdate);
                if (drive->writeCounter == 0)
                    FrameRefreshStatus();
            }
            if (!g_optEnhancedDisk && !controller->diskAccessed && drive->spinCounter) {
                drive->nibbleCounter += (cyclesSinceLastUpdate >> 5);
                if (drive->nibbleCounter >= drive->trackNibbles)
                    drive->nibbleCounter -= drive->trackNibbles;
            }
        }
        controller->diskAccessed = false;
    }
}
