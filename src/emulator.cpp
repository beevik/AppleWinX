/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

char         programDir[MAX_PATH];
const char * title = "Apple //e Emulator";

BOOL      apple2e     = TRUE;
BOOL      autoBoot    = FALSE;
BOOL      fullSpeed   = FALSE;
HINSTANCE instance    = (HINSTANCE)0;
int       mode        = MODE_LOGO;
BOOL      restart     = FALSE;
int64_t   totalCycles = 0;

static int     speed           = SPEED_NORMAL;
static double  speedMultiplier = 1.0;
static int64_t lastExecute     = 0;

//===========================================================================
static void ContinueExecution() {
    static int cycleSurplus    = 0;
    static int cyclesThisFrame = 0;

    // Check if the emulator should run at full-speed.
    bool forceFullspeed = (speed == SPEED_MAX) || KeybIsKeyDown(SDL_SCANCODE_SCROLLLOCK);
    bool tmpFullspeed   = DiskIsFullSpeedEligible();
    fullSpeed = forceFullspeed || tmpFullspeed;

    // Check the actual elapsed time.
    int64_t elapsed = TimerGetMsElapsed();
    while (elapsed == lastExecute && !fullSpeed) {
        TimerWait();
        elapsed = TimerGetMsElapsed();
    }

    // See how many milliseconds we need to simulate in order to catch up to the
    // wall clock.  Allow up to 32ms of simulation time while catching up.
    int msnormal = MIN(32, int((elapsed - lastExecute) * speedMultiplier));
    int ms = fullSpeed ? 32 : msnormal;

    // Advance the emulator 1 millisecond at a time.
    for (int t = 0; t < ms; ++t) {
        int cyclesattempted = CPU_CYCLES_PER_MS - cycleSurplus;
        int cyclesexecuted  = CpuExecute(cyclesattempted, &totalCycles);
        cycleSurplus = cyclesexecuted - cyclesattempted;

        DiskUpdatePosition(cyclesexecuted);
        JoyUpdatePosition(cyclesexecuted);
        VideoUpdateVbl(cyclesexecuted, cyclesThisFrame >= 15000);
        SpkrUpdate(cyclesexecuted);

        cyclesThisFrame += cyclesexecuted;
        if (cyclesThisFrame >= 17025) {
            cyclesThisFrame -= 17025;
            VideoCheckPage(TRUE);
        }

        // Check if fullspeed status has changed.
        if (fullSpeed) {
            tmpFullspeed = DiskIsFullSpeedEligible();
            fullSpeed    = forceFullspeed || tmpFullspeed;
            if (!fullSpeed)
                ms = msnormal;
        }
    }

    lastExecute = elapsed;

    if (!fullSpeed)
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
    GetModuleFileName((HINSTANCE)0, programDir, MAX_PATH);
    programDir[MAX_PATH - 1] = '\0';
    int loop = StrLen(programDir);
    while (loop--) {
        if (programDir[loop] == '\\' || programDir[loop] == ':') {
            programDir[loop + 1] = 0;
            break;
        }
    }
}

//===========================================================================
static void LoadConfiguration() {
    RegLoadValue("Configuration", "Computer Emulation", (DWORD *)&apple2e);
    RegLoadValue("Configuration", "Joystick Emulation", &joytype);
    RegLoadValue("Configuration", "Serial Port", &serialport);
    RegLoadValue("Configuration", "Emulation Speed", (DWORD *)&speed);
    RegLoadValue("Configuration", "Enhance Disk Speed", (DWORD *)&optEnhancedDisk);
    RegLoadValue("Configuration", "Monochrome Video", (DWORD *)&optMonochrome);
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
int GetSpeed() {
    return speed;
}

//===========================================================================
void SetMode(int newMode) {
    if (mode == newMode)
        return;
    if (newMode == MODE_RUNNING)
        lastExecute = TimerGetMsElapsed();
    mode = newMode;
}

//===========================================================================
void SetSpeed(int newSpeed) {
    speed = newSpeed;
    if (speed < SPEED_NORMAL)
        speedMultiplier = 0.5 + speed * 0.05;
    else
        speedMultiplier = speed * 0.1;
}

//===========================================================================
int APIENTRY WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int) {
    instance = inst;
    GdiSetBatchLimit(512);
    GetProgramDirectory();
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
