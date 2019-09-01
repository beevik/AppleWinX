/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop


constexpr int DRIVES   = 2;
constexpr int NIBBLES  = 6384;
constexpr int TRACKS   = 35;

struct floppyrec {
    char   imagename[16];
    HIMAGE imagehandle;
    int    track;
    LPBYTE trackimage;
    int    phase;
    int    byte;
    BOOL   writeprotected;
    BOOL   trackimagedata;
    BOOL   trackimagedirty;
    DWORD  spinning;
    DWORD  writelight;
    int    nibbles;
};

static int       currdrive       = 0;
static BOOL      diskaccessed    = FALSE;
static floppyrec floppy[DRIVES];
static BYTE      floppylatch     = 0;
static BOOL      floppymotoron   = FALSE;
static BOOL      floppywritemode = FALSE;

static void ReadTrack(int drive);
static void RemoveDisk(int drive);
static void WriteTrack(int drive);

//===========================================================================
static void CheckSpinning() {
    DWORD modechange = (floppymotoron && !floppy[currdrive].spinning);
    if (floppymotoron)
        floppy[currdrive].spinning = 20000;
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
    while (imagetitle[loop] && !found)
        if (IsCharLower(imagetitle[loop]))
            found = 1;
        else
            loop++;
    if ((!found) && (loop > 2))
        CharLowerBuff(imagetitle + 1, StrLen(imagetitle + 1));
    StrCopy(imagename, imagetitle, imagenamechars);
    imagename[15] = 0;
}

//===========================================================================
static BOOL InsertDisk(int drive, const char * imagefilename, BOOL createifnecessary) {
    floppyrec * fptr = &floppy[drive];
    if (fptr->imagehandle)
        RemoveDisk(drive);
    ZeroMemory(fptr, sizeof(floppyrec));
    BOOL result = ImageOpen(imagefilename,
        &fptr->imagehandle,
        &fptr->writeprotected,
        createifnecessary);
    if (result)
        GetImageTitle(imagefilename, fptr->imagename, ARRSIZE(fptr->imagename));
    return result;
}

//===========================================================================
static void ReadTrack(int drive) {
    floppyrec * fptr = &floppy[drive];
    if (fptr->track >= TRACKS) {
        fptr->trackimagedata = 0;
        return;
    }
    if (!fptr->trackimage)
        fptr->trackimage = (LPBYTE)VirtualAlloc(NULL, 0x1A00, MEM_COMMIT, PAGE_READWRITE);
    if (fptr->trackimage && fptr->imagehandle) {
        ImageReadTrack(fptr->imagehandle,
            fptr->track,
            fptr->phase,
            fptr->trackimage,
            &fptr->nibbles);
        fptr->trackimagedata = (fptr->nibbles != 0);
    }
}

//===========================================================================
static void NotifyInvalidImage(const char * imagefilename) {
    HANDLE file = CreateFile(
        imagefilename,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        (LPSECURITY_ATTRIBUTES)NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    char buffer[MAX_PATH + 128];
    if (file == INVALID_HANDLE_VALUE)
        StrPrintf(buffer, ARRSIZE(buffer), "Unable to open the file %s.", imagefilename);
    else {
        CloseHandle(file);
        StrPrintf(
            buffer,
            ARRSIZE(buffer),
            "%s\nUnable to use the file because the disk "
            "image format is not recognized.",
            imagefilename
        );
    }
    MessageBox(framewindow, buffer, TITLE, MB_ICONEXCLAMATION);
}

//===========================================================================
static void RemoveDisk(int drive) {
    floppyrec * fptr = &floppy[drive];
    if (fptr->imagehandle) {
        if (fptr->trackimage && fptr->trackimagedirty)
            WriteTrack(drive);
        ImageClose(fptr->imagehandle);
        fptr->imagehandle = (HIMAGE)0;
    }
    if (fptr->trackimage) {
        VirtualFree(fptr->trackimage, 0, MEM_RELEASE);
        fptr->trackimage = NULL;
        fptr->trackimagedata = 0;
    }
}

//===========================================================================
static void RemoveStepperDelay() {
  // note: make sure this works for the latest version of prodos
    if ((*(LPDWORD)(mem + 0xBA00) == 0xD0CA11A2) && (*(mem + 0xBA04) == 0xFD)) {
        *(LPDWORD)(mem + 0xBA00) = 0xEAEAEAEA;
        *(mem + 0xBA04) = 0xEA;
    }
    if ((*(LPDWORD)(mem + 0xBD9E) == 0xD08812A0) && (*(mem + 0xBDA2) == 0xFD)) {
        *(LPDWORD)(mem + 0xBD9E) = 0xEAEAEAEA;
        *(mem + 0xBDA2) = 0xEA;
    }
}

//===========================================================================
static void WriteTrack(int drive) {
    floppyrec *fptr = &floppy[drive];
    if (fptr->track >= TRACKS)
        return;
    if (fptr->trackimage && fptr->imagehandle)
        ImageWriteTrack(fptr->imagehandle,
            fptr->track,
            fptr->phase,
            fptr->trackimage,
            fptr->nibbles);
    fptr->trackimagedirty = 0;
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void DiskBoot() {

  // THIS FUNCTION RELOADS A PROGRAM IMAGE IF ONE IS LOADED IN DRIVE ONE.
  // IF A DISK IMAGE OR NO IMAGE IS LOADED IN DRIVE ONE, IT DOES NOTHING.
    if (floppy[0].imagehandle && ImageBoot(floppy[0].imagehandle))
        floppymotoron = 0;

}

//===========================================================================
BYTE __stdcall DiskControlMotor(WORD, BYTE address, BYTE, BYTE) {
    floppymotoron = address & 1;
    return MemReturnRandomData(1);
}

//===========================================================================
BYTE __stdcall DiskControlStepper(WORD, BYTE address, BYTE, BYTE) {
    CheckSpinning();
    if (optenhancedisk)
        RemoveStepperDelay();
    floppyrec * fptr = &floppy[currdrive];
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
                    if (fptr->trackimage && fptr->trackimagedirty)
                        WriteTrack(currdrive);
                    fptr->track = newtrack;
                    fptr->trackimagedata = 0;
                }
            }
        }
    }
    return (address == 0xE0) ? 0xFF : MemReturnRandomData(1);
}

//===========================================================================
void DiskDestroy() {
    RemoveDisk(0);
    RemoveDisk(1);
}

//===========================================================================
BYTE __stdcall DiskEnable(WORD, BYTE address, BYTE, BYTE) {
    currdrive = address & 1;
    CheckSpinning();
    return 0;
}

//===========================================================================
void DiskGetLightStatus(int * drive1, int * drive2) {
    *drive1 = floppy[0].spinning ? floppy[0].writelight ? 2
        : 1
        : 0;
    *drive2 = floppy[1].spinning ? floppy[1].writelight ? 2
        : 1
        : 0;
}

//===========================================================================
const char * DiskGetName(int drive) {
    return floppy[drive].imagename;
}

//===========================================================================
BOOL DiskInitialize() {
    int loop = DRIVES;
    while (loop--)
        ZeroMemory(&floppy[loop], sizeof(floppyrec));

    // PARSE THE COMMAND LINE LOOKING FOR THE NAME OF A DISK IMAGE.
    // (THIS IS MADE MORE COMPLICATED BY THE FACT THAT LONG FILE NAMES MAY
    //  BE EMBEDDED IN QUOTES, INCLUDING THE NAME OF THE PROGRAM ITSELF)
    char         imagefilename[MAX_PATH] = "";
    const char * cmdlinenameIn           = GetCommandLine();
    int          cmdlinenameLen          = StrLen(cmdlinenameIn);
    char *       cmdlinenameCopy         = new char[cmdlinenameLen + 1];
    char *       cmdlinename             = cmdlinenameCopy;
    StrCopy(cmdlinenameCopy, cmdlinenameIn, cmdlinenameLen + 1);
    BOOL    inquotes = 0;
    while (cmdlinename && ((*cmdlinename != ' ') || inquotes)) {
        if (*cmdlinename == '\"')
            inquotes = !inquotes;
        ++cmdlinename;
    }
    while (cmdlinename && ((*cmdlinename == ' ') || (*cmdlinename == '\"')))
        ++cmdlinename;
    if (cmdlinename && *cmdlinename) {
        StrCopy(imagefilename, cmdlinename, ARRSIZE(imagefilename));
        char * ptr = StrChr(cmdlinename, '"');
        if (ptr != NULL)
            *ptr = '\0';
    }

    // IF WE DIDN'T FIND AN IMAGE FILE NAME, USE MASTER.DSK
    else {
        StrCopy(imagefilename, progdir, ARRSIZE(imagefilename));
        StrCat(imagefilename, "master.dsk", ARRSIZE(imagefilename));
    }

    // OPEN THE IMAGE FILE
    if (InsertDisk(0, imagefilename, 0)) {
        if (cmdlinename && *cmdlinename)
            autoboot = 1;
        delete[] cmdlinenameCopy;
        return 1;
    }
    else {
        NotifyInvalidImage(imagefilename);
        delete[] cmdlinenameCopy;
        return 0;
    }
}

//===========================================================================
BOOL DiskIsSpinning() {
    return (floppy[0].spinning || floppy[1].spinning);
}

//===========================================================================
BYTE __stdcall DiskReadWrite(WORD programcounter, BYTE, BYTE, BYTE) {
    floppyrec * fptr = &floppy[currdrive];
    diskaccessed = 1;
    if ((!fptr->trackimagedata) && fptr->imagehandle)
        ReadTrack(currdrive);
    if (!fptr->trackimagedata)
        return 0xFF;
    BYTE result = 0;
    if ((!floppywritemode) || (!fptr->writeprotected))
        if (floppywritemode)
            if (floppylatch & 0x80) {
                *(fptr->trackimage + fptr->byte) = floppylatch;
                fptr->trackimagedirty = 1;
            }
            else
                return 0;
        else {
            if (optenhancedisk &&
                ((*(LPDWORD)(mem + programcounter) == 0xD5C9FB10) ||
                (*(LPDWORD)(mem + programcounter) == 0xD549FB10)) &&
                    ((*(LPDWORD)(mem + programcounter + 4) & 0xFFFF00FF) != 0xAAC900F0) &&
                ((*(mem + programcounter + 4) != 0xD0) ||
                (*(mem + programcounter + 5) == 0xF7) ||
                    (*(mem + programcounter + 5) == 0xF0))) {
                int loop = fptr->nibbles;
                while ((*(fptr->trackimage + fptr->byte) != 0xD5) && loop--)
                    if (++fptr->byte >= fptr->nibbles)
                        fptr->byte = 0;
            }
            result = *(fptr->trackimage + fptr->byte);
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
    ofn.hwndOwner = framewindow;
    ofn.hInstance = instance;
    ofn.lpstrFilter = "All Images\0*.apl;*.bin;*.do;*.dsk;*.iie;*.nib;*.po\0"
        "Disk Images (*.bin,*.do,*.dsk,*.iie,*.nib,*.po)\0*.bin;*.do;*.dsk;*.iie;*.nib;*.po\0"
        "All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = directory;
    ofn.Flags = OFN_CREATEPROMPT | OFN_HIDEREADONLY;
    ofn.lpTemplateName = "INSERT_DIALOG";
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
BYTE __stdcall DiskSetLatchValue(WORD, BYTE, BYTE write, BYTE value) {
    if (write)
        floppylatch = value;
    return floppylatch;
}

//===========================================================================
BYTE __stdcall DiskSetReadMode(WORD, BYTE, BYTE, BYTE) {
    floppywritemode = 0;
    return MemReturnRandomData(floppy[currdrive].writeprotected);
}

//===========================================================================
BYTE __stdcall DiskSetWriteMode(WORD, BYTE, BYTE, BYTE) {
    floppywritemode = 1;
    BOOL modechange = !floppy[currdrive].writelight;
    floppy[currdrive].writelight = 20000;
    if (modechange)
        FrameRefreshStatus();
    return MemReturnRandomData(1);
}

//===========================================================================
void DiskUpdatePosition(DWORD cycles) {
    int loop = 2;
    while (loop--) {
        floppyrec * fptr = &floppy[loop];
        if (fptr->spinning && !floppymotoron) {
            if (!(fptr->spinning -= MIN(fptr->spinning, (cycles >> 6))))
                FrameRefreshStatus();
        }
        if (floppywritemode && (currdrive == loop) && fptr->spinning)
            fptr->writelight = 20000;
        else if (fptr->writelight) {
            if (!(fptr->writelight -= MIN(fptr->writelight, (cycles >> 6))))
                FrameRefreshStatus();
        }
        if ((!optenhancedisk) && (!diskaccessed) && fptr->spinning) {
            needsprecision = cumulativecycles;
            fptr->byte += (cycles >> 5);
            if (fptr->byte >= fptr->nibbles)
                fptr->byte -= fptr->nibbles;
        }
    }
    diskaccessed = 0;
}
