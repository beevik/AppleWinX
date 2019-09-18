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
    BOOL   writeProtected;
    BOOL   trackImageData;
    BOOL   trackImageDirty;
    DWORD  spinning;
    DWORD  writeLight;
    int    nibbles;
};

BOOL optEnhancedDisk = TRUE;

static int    currDrive       = 0;
static BOOL   diskAccessed    = FALSE;
static floppy floppyDrive[DRIVES];
static BYTE   floppyLatch     = 0;
static BOOL   floppyMotorOn   = FALSE;
static BOOL   floppyWriteMode = FALSE;

static void ReadTrack(int drive);
static void RemoveDisk(int drive);
static void WriteTrack(int drive);

//===========================================================================
static void CheckSpinning() {
    DWORD modechange = (floppyMotorOn && !floppyDrive[currDrive].spinning);
    if (floppyMotorOn)
        floppyDrive[currDrive].spinning = INDICATOR_LAG;
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
static BOOL InsertDisk(int drive, const char * imagefilename, BOOL createifnecessary) {
    floppy * fptr = &floppyDrive[drive];
    if (fptr->imageHandle)
        RemoveDisk(drive);
    ZeroMemory(fptr, sizeof(floppy));
    BOOL result = ImageOpen(
        imagefilename,
        &fptr->imageHandle,
        &fptr->writeProtected,
        createifnecessary
    );
    if (result)
        GetImageTitle(imagefilename, fptr->imageName, ARRSIZE(fptr->imageName));
    return result;
}

//===========================================================================
static void ReadTrack(int drive) {
    floppy * fptr = &floppyDrive[drive];
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
    char buffer[MAX_PATH + 128];
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
    MessageBox(framewindow, buffer, title, MB_ICONEXCLAMATION);
}

//===========================================================================
static void RemoveDisk(int drive) {
    floppy * fptr = &floppyDrive[drive];
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
    if ((*(LPDWORD)(mem + 0xBA00) == 0xD0CA11A2) && (mem[0xBA04] == 0xFD)) {
        *(LPDWORD)(mem + 0xBA00) = 0xEAEAEAEA;
        mem[0xBA04] = 0xEA;
    }
    if ((*(LPDWORD)(mem + 0xBD9E) == 0xD08812A0) && (mem[0xBDA2] == 0xFD)) {
        *(LPDWORD)(mem + 0xBD9E) = 0xEAEAEAEA;
        mem[0xBDA2] = 0xEA;
    }
}

//===========================================================================
static void WriteTrack(int drive) {
    floppy *fptr = &floppyDrive[drive];
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

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void DiskBoot() {
    // THIS FUNCTION RELOADS A PROGRAM IMAGE IF ONE IS LOADED IN DRIVE ONE.
    // IF A DISK IMAGE OR NO IMAGE IS LOADED IN DRIVE ONE, IT DOES NOTHING.
    if (floppyDrive[0].imageHandle && ImageBoot(floppyDrive[0].imageHandle))
        floppyMotorOn = FALSE;
}

//===========================================================================
BYTE DiskControlMotor(WORD, BYTE address, BYTE, BYTE) {
    floppyMotorOn = (address & 1) != 0;
    return MemReturnRandomData(TRUE);
}

//===========================================================================
BYTE DiskControlStepper(WORD, BYTE address, BYTE, BYTE) {
    CheckSpinning();
    if (optEnhancedDisk)
        RemoveStepperDelay();
    floppy * fptr = &floppyDrive[currDrive];
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
                        WriteTrack(currDrive);
                    fptr->track = newtrack;
                    fptr->trackImageData = 0;
                }
            }
        }
    }
    return (address == 0xE0) ? 0xFF : MemReturnRandomData(TRUE);
}

//===========================================================================
void DiskDestroy() {
    RemoveDisk(0);
    RemoveDisk(1);
}

//===========================================================================
BYTE DiskEnable(WORD, BYTE address, BYTE, BYTE) {
    currDrive = address & 1;
    CheckSpinning();
    return 0;
}

//===========================================================================
void DiskGetLightStatus(int * drive1, int * drive2) {
    *drive1 = floppyDrive[0].spinning
        ? floppyDrive[0].writeLight
            ? 2
            : 1
        : 0;
    *drive2 = floppyDrive[1].spinning
        ? floppyDrive[1].writeLight
            ? 2
            : 1
        : 0;
}

//===========================================================================
const char * DiskGetName(int drive) {
    return floppyDrive[drive].imageName;
}

//===========================================================================
BOOL DiskInitialize() {
    for (int loop = 0; loop < DRIVES; loop++)
        ZeroMemory(&floppyDrive[loop], sizeof(floppy));

    // PARSE THE COMMAND LINE LOOKING FOR THE NAME OF A DISK IMAGE.
    // (THIS IS MADE MORE COMPLICATED BY THE FACT THAT LONG FILE NAMES MAY
    // BE EMBEDDED IN QUOTES, INCLUDING THE NAME OF THE PROGRAM ITSELF)
    char         imagefilename[MAX_PATH] = "";
    const char * cmdlinenameIn           = GetCommandLine();
    int          cmdlinenameLen          = StrLen(cmdlinenameIn);
    char *       cmdlinenameCopy         = new char[cmdlinenameLen + 1];
    char *       cmdlinename             = cmdlinenameCopy;
    StrCopy(cmdlinenameCopy, cmdlinenameIn, cmdlinenameLen + 1);
    BOOL inquotes = FALSE;
    while (*cmdlinename && (*cmdlinename != ' ' || inquotes)) {
        if (*cmdlinename == '\"')
            inquotes = !inquotes;
        ++cmdlinename;
    }
    while (*cmdlinename && (*cmdlinename == ' ' || *cmdlinename == '\"'))
        ++cmdlinename;

    if (*cmdlinename) {
        StrCopy(imagefilename, cmdlinename, ARRSIZE(imagefilename));
        char * ptr = StrChr(imagefilename, '"');
        if (ptr != NULL)
            *ptr = '\0';
    }
    else {
        StrCopy(imagefilename, programDir, ARRSIZE(imagefilename));
        StrCat(imagefilename, "master.dsk", ARRSIZE(imagefilename));
    }

    if (InsertDisk(0, imagefilename, 0)) {
        if (*cmdlinename)
            autoBoot = TRUE;
        delete[] cmdlinenameCopy;
        return TRUE;
    }
    else {
        NotifyInvalidImage(imagefilename);
        delete[] cmdlinenameCopy;
        return FALSE;
    }
}

//===========================================================================
BOOL DiskIsFullSpeedEligible() {
    return optEnhancedDisk && floppyMotorOn;
}

//===========================================================================
BOOL DiskIsSpinning() {
    return (floppyDrive[0].spinning || floppyDrive[1].spinning);
}

//===========================================================================
BYTE DiskReadWrite(WORD programcounter, BYTE, BYTE, BYTE) {
    floppy * fptr = &floppyDrive[currDrive];
    diskAccessed = TRUE;
    if ((!fptr->trackImageData) && fptr->imageHandle)
        ReadTrack(currDrive);
    if (!fptr->trackImageData)
        return 0xFF;
    BYTE result = 0;
    if (!floppyWriteMode || !fptr->writeProtected) {
        if (floppyWriteMode) {
            if (floppyLatch & 0x80) {
                fptr->trackImage[fptr->byte] = floppyLatch;
                fptr->trackImageDirty = 1;
            }
            else
                return 0;
        }
        else {
            if (optEnhancedDisk
                && (*(LPDWORD)(mem + programcounter) == 0xD5C9FB10 || *(LPDWORD)(mem + programcounter) == 0xD549FB10)
                && (*(LPDWORD)(mem + programcounter + 4) & 0xFFFF00FF) != 0xAAC900F0
                && (mem[programcounter + 4] != 0xD0 || mem[programcounter + 5] == 0xF7 || mem[programcounter + 5] == 0xF0))
            {
                int loop = fptr->nibbles;
                while (fptr->trackImage[fptr->byte] != 0xD5 && loop--) {
                    if (++fptr->byte >= fptr->nibbles)
                        fptr->byte = 0;
                }
            }
            result = fptr->trackImage[fptr->byte];
        }
    }
    if (++fptr->byte >= fptr->nibbles)
        fptr->byte = 0;
    return result;
}

//===========================================================================
void DiskSelect(int drive) {
    char directory[MAX_PATH] = "";
    char filename[MAX_PATH]  = "";
    RegLoadString("Preferences", "Starting Directory", directory, MAX_PATH);
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner       = framewindow;
    ofn.hInstance       = instance;
    ofn.lpstrFilter     = "All Images\0*.apl;*.bin;*.do;*.dsk;*.iie;*.nib;*.po\0"
                          "Disk Images (*.bin,*.do,*.dsk,*.iie,*.nib,*.po)\0*.bin;*.do;*.dsk;*.iie;*.nib;*.po\0"
                          "All Files\0*.*\0";
    ofn.lpstrFile       = filename;
    ofn.nMaxFile        = MAX_PATH;
    ofn.lpstrInitialDir = directory;
    ofn.Flags           = OFN_CREATEPROMPT | OFN_HIDEREADONLY;
    ofn.lpTemplateName  = "INSERT_DIALOG";
    if (GetOpenFileName(&ofn)) {
        if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
            StrCat(filename, ".dsk", ARRSIZE(filename));
        if (InsertDisk(drive, filename, 1)) {
            filename[ofn.nFileOffset] = 0;
            if (StrCmpI(directory, filename) != 0)
                RegSaveString("Preferences", "Starting Directory", filename);
        }
        else
            NotifyInvalidImage(filename);
    }
}

//===========================================================================
BYTE DiskSetLatchValue(WORD, BYTE, BYTE write, BYTE value) {
    if (write)
        floppyLatch = value;
    return floppyLatch;
}

//===========================================================================
BYTE DiskSetReadMode(WORD pc, BYTE address, BYTE write, BYTE value) {
    floppyWriteMode = FALSE;
    return MemReturnRandomData(floppyDrive[currDrive].writeProtected);
}

//===========================================================================
BYTE DiskSetWriteMode(WORD pc, BYTE address, BYTE write, BYTE value) {
    floppyWriteMode = TRUE;
    BOOL modechange = !floppyDrive[currDrive].writeLight;
    floppyDrive[currDrive].writeLight = INDICATOR_LAG;
    if (modechange)
        FrameRefreshStatus();
    return MemReturnRandomData(TRUE);
}

//===========================================================================
void DiskUpdatePosition(DWORD cycles) {
    for (int i = 0; i < 2; ++i) {
        floppy * fptr = &floppyDrive[i];
        if (fptr->spinning && !floppyMotorOn) {
            if (!(fptr->spinning -= MIN(fptr->spinning, cycles)))
                FrameRefreshStatus();
        }
        if (floppyWriteMode && (currDrive == i) && fptr->spinning)
            fptr->writeLight = INDICATOR_LAG;
        else if (fptr->writeLight) {
            if (!(fptr->writeLight -= MIN(fptr->writeLight, cycles)))
                FrameRefreshStatus();
        }
        if (!optEnhancedDisk && !diskAccessed && fptr->spinning) {
            fptr->byte += (cycles >> 5);
            if (fptr->byte >= fptr->nibbles)
                fptr->byte -= fptr->nibbles;
        }
    }
    diskAccessed = FALSE;
}
