/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

const char * title = "Apple //e Emulator";

BOOL      apple2e           = TRUE;
BOOL      autoboot          = FALSE;
BOOL      fullspeed         = FALSE;
HINSTANCE instance          = (HINSTANCE)0;
int       mode              = MODE_LOGO;
BOOL      optenhancedisk    = TRUE;
BOOL      optmonochrome     = FALSE;
char      progdir[MAX_PATH] = "";
BOOL      restart           = FALSE;
DWORD     speed             = 10;
int64_t   totalcycles       = 0;

static int64_t lastexecute = 0;

//===========================================================================
static void ContinueExecution() {
    static int cyclesurplus    = 0;
    static int cyclesthisframe = 0;

    // Check if the emulator should run at full-speed.
    bool forcefullspeed = (speed == SPEED_MAX) || KeybIsKeyDown(SDL_SCANCODE_SCROLLLOCK);
    bool tmpfullspeed   = DiskIsFullSpeedEligible();
    fullspeed = forcefullspeed || tmpfullspeed;

    // Check the actual elapsed time.
    int64_t elapsed = TimerGetMsElapsed();
    while (elapsed == lastexecute && !fullspeed) {
        TimerWait();
        elapsed = TimerGetMsElapsed();
    }

    // Calculate the speed-up (or slow-down) multiplier.
    double multiplier;
    if (speed < 10)
        multiplier = 0.5 + speed * 0.05;
    else
        multiplier = speed * 0.1;

    // See how many milliseconds we need to simulate in order to catch up to the
    // wall clock.  Allow up to 32ms of simulation time while catching up.
    int msnormal = MIN(32, int((elapsed - lastexecute) * multiplier));
    int ms = fullspeed ? 32 : msnormal;

    // Advance the emulator 1 millisecond at a time.
    for (int t = 0; t < ms; ++t) {
        int cyclesattempted = CPU_CYCLES_PER_MS - cyclesurplus;
        int cyclesexecuted  = CpuExecute(cyclesattempted, &totalcycles);
        cyclesurplus = cyclesexecuted - cyclesattempted;

        DiskUpdatePosition(cyclesexecuted);
        JoyUpdatePosition(cyclesexecuted);
        VideoUpdateVbl(cyclesexecuted, cyclesthisframe >= 15000);
        SpkrUpdate(cyclesexecuted);

        cyclesthisframe += cyclesexecuted;
        if (cyclesthisframe >= 17025) {
            cyclesthisframe -= 17025;
            VideoCheckPage(TRUE);
        }

        // Check if fullspeed status has changed.
        if (fullspeed) {
            tmpfullspeed = DiskIsFullSpeedEligible();
            fullspeed    = forcefullspeed || tmpfullspeed;
            if (!fullspeed)
                ms = msnormal;
        }
    }

    lastexecute = elapsed;

    if (!fullspeed)
        TimerWait();
}

//===========================================================================
static LRESULT CALLBACK DlgProc(
    HWND    window,
    UINT    message,
    WPARAM  wparam,
    LPARAM  lparam
) {
    if (message == WM_CREATE) {
        RECT rect;
        GetWindowRect(window, &rect);
        SIZE size;
        size.cx     = rect.right - rect.left;
        size.cy     = rect.bottom - rect.top;
        rect.left   = (GetSystemMetrics(SM_CXSCREEN) - size.cx) >> 1;
        rect.top    = (GetSystemMetrics(SM_CYSCREEN) - size.cy) >> 1;
        rect.right  = rect.left + size.cx;
        rect.bottom = rect.top + size.cy;
        MoveWindow(window,
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            0);
        ShowWindow(window, SW_SHOW);
    }
    return DefWindowProc(window, message, wparam, lparam);
}

//===========================================================================
static void GetProgramDirectory() {
    GetModuleFileName((HINSTANCE)0, progdir, MAX_PATH);
    progdir[MAX_PATH - 1] = '\0';
    int loop = StrLen(progdir);
    while (loop--) {
        if (progdir[loop] == '\\' || progdir[loop] == ':') {
            progdir[loop + 1] = 0;
            break;
        }
    }
}

//===========================================================================
static void LoadConfiguration() {
#define LOAD(a,b) RegLoadValue("Configuration",a,b);
    LOAD("Computer Emulation", (DWORD *)&apple2e);
    LOAD("Joystick Emulation", &joytype);
    LOAD("Serial Port", &serialport);
    LOAD("Emulation Speed", &speed);
    LOAD("Enhance Disk Speed", (DWORD *)&optenhancedisk);
    LOAD("Monochrome Video", (DWORD *)&optmonochrome);
#undef LOAD
}

//===========================================================================
static void MessageLoop() {
    for (;;) {
        WindowUpdate();

        if (mode == MODE_RUNNING)
            ContinueExecution();
        else if (mode == MODE_STEPPING)
            DebugContinueStepping();
        else if (mode == MODE_SHUTDOWN)
            break;
        else
            Sleep(1);
    }
}

//===========================================================================
static void RegisterExtensions() {
    char command[MAX_PATH];
    GetModuleFileName((HMODULE)0, command, MAX_PATH);
    command[MAX_PATH - 1] = 0;
    char icon[MAX_PATH];
    StrPrintf(icon, ARRSIZE(icon), "%s,1", command);
    StrCat(command, " %1", ARRSIZE(command));
    RegSetValue(HKEY_CLASSES_ROOT, ".bin", REG_SZ, "DiskImage", 10);
    RegSetValue(HKEY_CLASSES_ROOT, ".do", REG_SZ, "DiskImage", 10);
    RegSetValue(HKEY_CLASSES_ROOT, ".dsk", REG_SZ, "DiskImage", 10);
    RegSetValue(HKEY_CLASSES_ROOT, ".nib", REG_SZ, "DiskImage", 10);
    RegSetValue(HKEY_CLASSES_ROOT, ".po", REG_SZ, "DiskImage", 10);
    RegSetValue(HKEY_CLASSES_ROOT, "DiskImage", REG_SZ, "Disk Image", 11);
    RegSetValue(HKEY_CLASSES_ROOT, "DiskImage\\DefaultIcon", REG_SZ, icon, StrLen(icon) + 1);
    RegSetValue(HKEY_CLASSES_ROOT, "DiskImage\\shell\\open\\command", REG_SZ, command, StrLen(command) + 1);
}

//===========================================================================
void SetMode(int newmode) {
    if (mode == newmode)
        return;
    if (newmode == MODE_RUNNING)
        lastexecute = TimerGetMsElapsed();
    mode = newmode;
}

//===========================================================================
int APIENTRY WinMain(HINSTANCE passinstance, HINSTANCE, LPSTR, int) {
    instance = passinstance;
    GdiSetBatchLimit(512);
    GetProgramDirectory();
    RegisterExtensions();
    FrameRegisterClass();
    ImageInitialize();
    if (!WindowInitialize())
        return 1;
    if (!DiskInitialize())
        return 1;

    do {
        restart = FALSE;
        SetMode(MODE_LOGO);
        LoadConfiguration();
        DebugInitialize();
        JoyInitialize();
        MemInitialize();
        VideoInitialize();
        FrameCreateWindow();
        TimerInitialize(1);

        MessageLoop();

        TimerDestroy();
    } while (restart);

    WindowDestroy();
    return 0;
}
