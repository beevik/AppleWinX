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
BOOL      behind            = FALSE;
int       cyclesurplus      = 0;
DWORD     cyclenum          = 0;
BOOL      fullspeed         = FALSE;
HINSTANCE instance          = (HINSTANCE)0;
int64_t   lastexecute       = 0;
int       mode              = MODE_LOGO;
DWORD     needsprecision    = 0;
BOOL      optenhancedisk    = TRUE;
BOOL      optmonochrome     = FALSE;
char      progdir[MAX_PATH] = "";
BOOL      resettiming       = FALSE;
BOOL      restart           = FALSE;
DWORD     speed             = 10;
DWORD     totalcycles       = 0;

static DWORD clockgran      = 0;
static DWORD cyclegran      = 0;
static DWORD finegraindelay = 1;

//===========================================================================
static void ContinueExecutionNew() {
    static int cyclesthisframe = 0;

    // Check if the emulator should run at full-speed.
    bool forcefullspeed = (speed == SPEED_MAX) || KeybIsKeyDown(SDL_SCANCODE_SCROLLLOCK);
    bool tmpfullspeed   = DiskIsFullSpeedEligible() && !SpkrNeedsAccurateCycleCount();
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
            tmpfullspeed = DiskIsFullSpeedEligible() && !SpkrNeedsAccurateCycleCount();
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
static void ContinueExecution() {
    static BOOL finegrainlast   = TRUE;
    static BOOL finegraintiming = TRUE;
    static BOOL normaldelays    = FALSE;
    static BOOL pageflipping    = FALSE;

    // RUN THE CPU, DISK, AND JOYSTICK TIMERS FOR ONE CLOCK TICK'S
    // WORTH OF CYCLES
    BOOL skippedfinegrain = FALSE;
    BOOL ranfinegrain     = FALSE;
    {
        int   loop = 1 + (cyclegran >= 20000);
        DWORD cyclestorun = cyclegran >> (cyclegran >= 20000 ? 1 : 0);
        while (loop--) {
            cyclenum = 0;
            if (((totalcycles - needsprecision) > 1500000) && !finegraintiming) {
                do {
                    DWORD executedcycles = CpuExecute(2000, &totalcycles);
                    cyclenum += executedcycles;
                    DiskUpdatePosition(executedcycles);
                    JoyUpdatePosition(executedcycles);
                    VideoUpdateVbl(executedcycles, (cyclestorun - cyclenum) <= 2000);
                } while (cyclenum < cyclestorun);
            }
            else {
                DWORD cyclesneeded = 0;
                do {
                    DWORD startcycles = cyclenum;
                    if (SpkrNeedsAccurateCycleCount()) {
                        int loop2 = 3;
                        while (loop2--) {
                            cyclesneeded += 33;
                            if (cyclenum < cyclesneeded)
                                cyclenum += CpuExecute(cyclesneeded - cyclenum, &totalcycles);
                        }
                    }
                    else {
                        cyclesneeded += 100;
                        if (cyclenum < cyclesneeded)
                            cyclenum += CpuExecute(cyclesneeded - cyclenum, &totalcycles);
                    }
                    DiskUpdatePosition(cyclenum - startcycles);
                    JoyUpdatePosition(cyclenum - startcycles);
                    VideoUpdateVbl(cyclenum - startcycles,
                        (cyclestorun - cyclesneeded) < 1000);
                    if (finegraintiming && finegrainlast) {
                        if (SpkrCyclesSinceSound() > 50000)
                            skippedfinegrain = 1;
                        else {
                            ranfinegrain = 1;
                            DWORD loop = finegraindelay;
                            while (loop--);
                        }
                    }
                } while (cyclesneeded < cyclestorun);
            }
            SpkrUpdate(cyclenum);
            CommUpdate(cyclenum);
        }
    }
    pages = 0;

    // DETERMINE WHETHER THE SCREEN WAS UPDATED, THE DISK WAS SPINNING,
    // OR THE KEYBOARD I/O PORTS WERE BEING EXCESSIVELY QUERIED THIS
    // CLOCKTICK
    VideoCheckPage(FALSE);
    BOOL diskspinning  = DiskIsSpinning();
    BOOL screenupdated = VideoHasRefreshed();
    BOOL systemidle    = (KeybGetNumQueries() > (clockgran << 2)) && !ranfinegrain;
    fullspeed = speed == SPEED_MAX || KeybIsKeyDown(SDL_SCANCODE_SCROLLLOCK);
    if (screenupdated)
        pageflipping = 3;

    // IF A TWENTIETH OF A SECOND HAS ELAPSED AND THE SCREEN HAS NOT BEEN
    // UPDATED BUT IT APPEARS TO NEED UPDATING, THEN REFRESH IT
    if (mode != MODE_LOGO) {
        static BOOL  anyupdates     = 0;
        static DWORD lastcycles     = 0;
        static BOOL  lastupdates[2] = { 0, 0 };
        anyupdates |= screenupdated;
        DWORD cyclelimit = 50000;
        if (behind || fullspeed || ranfinegrain)
            cyclelimit <<= 1;
        if ((totalcycles - lastcycles) >= cyclelimit) {
            lastcycles = totalcycles;
            if (!anyupdates && !lastupdates[0] && !lastupdates[1] && VideoApparentlyDirty()) {
                VideoCheckPage(TRUE);
                static DWORD lasttime = 0;
                DWORD currtime = GetTickCount();
                if (!fullspeed || (currtime - lasttime >= (DWORD)((graphicsmode || !systemidle) ? 100 : 25))) {
                    VideoRefreshScreen();
                    lasttime = currtime;
                }
                screenupdated = 1;
            }
            lastupdates[1] = lastupdates[0];
            lastupdates[0] = anyupdates;
            anyupdates = 0;
            if (pageflipping)
                pageflipping--;
        }
    }

    // IF WE ARE NOT CALIBRATING THE SYSTEM, TURN FINE GRAIN TIMING
    // ON OR OFF BASED ON WHETHER THE DISK IS SPINNING, THE SCREEN IS
    // BEING UPDATED, OR THE SYSTEM IS IDLE
    finegrainlast = finegraintiming;
    finegraintiming =
        (!systemidle && !diskspinning && !fullspeed &&
            !pageflipping && !screenupdated && SpkrNeedsFineGrainTiming() && !VideoApparentlyDirty());

    // COMPARE THE EMULATOR'S CLOCK TO THE REAL TIME CLOCK
    {
        static DWORD milliseconds = 0;
        static DWORD microseconds = 0;
        DWORD currtime = GetTickCount();
        if (!fullspeed && (!optenhancedisk || !diskspinning) && !resettiming) {
            if (speed == SPEED_NORMAL)
                milliseconds += clockgran;
            else {
                DWORD delta = (DWORD)((clockgran * 2000.0) / pow(2.0, speed / 10.0));
                milliseconds += (delta / 1000);
                microseconds += (delta % 1000);
                if (microseconds >= 1000) {
                    microseconds -= 1000;
                    ++milliseconds;
                }
            }

            // DETERMINE WHETHER WE ARE AHEAD OF OR BEHIND REAL TIME
            if (currtime > milliseconds + 1000) {
                behind = 0;
                milliseconds = currtime;
            }
            else if (currtime >= milliseconds + 100)
                behind = 1;
            else if (milliseconds > currtime) {
                static DWORD ahead = 0;
                static DWORD lastreset = 0;
                ahead += milliseconds - currtime;
                if (currtime - lastreset > 500) {
                    if (ahead >= 200)
                        behind = 0;
                    ahead = 0;
                    lastreset = currtime;
                }
            }

            // IF WE ARE AHEAD OF REAL TIME, WAIT FOR REAL TIME TO CATCH UP
            if ((milliseconds - currtime > 0) &&
                (milliseconds - currtime < 200)) {
                ++normaldelays;
                if ((skippedfinegrain || !finegraintiming) && !behind)
                    Sleep(1);
                do {
                    if (systemidle && !behind)
                        Sleep(1);
                    currtime = GetTickCount();
                } while ((milliseconds - currtime > 0) &&
                    (milliseconds - currtime < 200));
            }
            else if (currtime - milliseconds > 250)
                milliseconds += 100;
        }
        else {
            behind = fullspeed;
            milliseconds = currtime;
            resettiming = 0;
        }
    }
}

//===========================================================================
static void DetermineClockGranularity() {
    clockgran = 50;
    DWORD oldticks = GetTickCount();
    int   loop = 40;
    while (loop-- && (clockgran >= 20)) {
        DWORD newticks;
        do {
            newticks = GetTickCount();
        } while (oldticks >= newticks);
        if (newticks - oldticks >= 10)
            clockgran = MIN(clockgran, newticks - oldticks);
        oldticks = newticks;
    }
    cyclegran = clockgran * 1000;
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
static BOOL LoadCalibrationData() {
    clockgran      = 16;
    cyclegran      = 16000;
    finegraindelay = 98305;
    return TRUE;
}

//===========================================================================
static void LoadConfiguration() {
#define LOAD(a,b) RegLoadValue("Configuration",a,b);
    LOAD("Computer Emulation", (DWORD *)&apple2e);
    LOAD("Joystick Emulation", &joytype);
    LOAD("Sound Emulation", &soundtype);
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
            ContinueExecutionNew();
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
static void SaveCalibrationData() {
    RegSaveValue("", "CurrentBuildNumber",  BUILDNUMBER);
    RegSaveValue("Calibration", "Clock Granularity", clockgran);
    RegSaveValue("Calibration", "Cycle Granularity", cyclegran);
    RegSaveValue("Calibration", "Precision Timing", finegraindelay);
}

//===========================================================================
void SetMode(int newmode) {
    if (mode == newmode)
        return;

    if (newmode == MODE_RUNNING) {
        lastexecute = TimerGetMsElapsed();
    }

    mode = newmode;
}

//===========================================================================
int APIENTRY WinMain(HINSTANCE passinstance, HINSTANCE, LPSTR, int) {

    // DO ONE-TIME INITIALIZATION
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
        // DO INITIALIZATION THAT MUST BE REPEATED FOR A RESTART
        restart = 0;
        SetMode(MODE_LOGO);
        LoadConfiguration();
        DebugInitialize();
        JoyInitialize();
        MemInitialize();
        VideoInitialize();
        LoadCalibrationData();
        FrameCreateWindow();
        TimerInitialize(1);

        // ENTER THE MAIN MESSAGE LOOP
        MessageLoop();

        TimerDestroy();
    } while (restart);

    WindowDestroy();
    return 0;
}
