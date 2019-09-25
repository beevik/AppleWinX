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

BOOL          apple2e   = TRUE;
BOOL          autoBoot  = FALSE;
HINSTANCE     instance  = (HINSTANCE)0;
BOOL          restart   = FALSE;

int64_t g_cyclesEmulated = 0;

static bool          s_isBehind;
static bool          s_lastFullSpeed;
static EEmulatorMode s_mode;
static int           s_speed;

//===========================================================================
static void AdvanceNormal() {
    // Advance the emulator until we catch up to the expected number of elapsed
    // cycles.  Run at most 16ms worth of cycles.
    int64_t elapsedCycles = TimerUpdateElapsedCycles();
    int64_t stopCycle = MIN(elapsedCycles, g_cyclesEmulated + 16 * CPU_CYCLES_PER_MS);
    s_isBehind = (elapsedCycles != stopCycle);
    while (g_cyclesEmulated < stopCycle && !TimerIsFullSpeed()) {

        // Step the CPU until the next scheduled event.
        int64_t nextEventCycle = MIN(SchedulerPeekTime(), stopCycle);
        while (g_cyclesEmulated < nextEventCycle)
            g_cyclesEmulated += CpuStep6502();

        // Process all scheduled events happening before or on the current
        // cycle.
        Event event;
        while (SchedulerDequeue(g_cyclesEmulated, &event))
            event.func(event.cycle);
    }

    // Wait for the emulator tick (unless the emulator has fallen behind the
    // wall clock).
    if (!s_isBehind)
        TimerWait();
}

//===========================================================================
static void AdvanceFullSpeed() {
    // In full speed mode, advance 32ms worth of cycles at a time.
    int64_t stopCycle = g_cyclesEmulated + 32 * CPU_CYCLES_PER_MS;
    while (g_cyclesEmulated < stopCycle && TimerIsFullSpeed()) {

        // Step the CPU until the next scheduled event.
        int64_t nextEventCycle = MIN(SchedulerPeekTime(), stopCycle);
        while (g_cyclesEmulated < nextEventCycle)
            g_cyclesEmulated += CpuStep6502();

        // Process all scheduled events happening before or on the current
        // cycle.
        Event event;
        while (SchedulerDequeue(g_cyclesEmulated, &event))
            event.func(event.cycle);
    }
}

//===========================================================================
static void Advance() {
    // When switching back to normal speed, reset the timer.
    bool fullSpeed = TimerIsFullSpeed();
    if (s_lastFullSpeed && !fullSpeed)
        TimerReset(g_cyclesEmulated);
    s_lastFullSpeed = fullSpeed;

    if (fullSpeed)
        AdvanceFullSpeed();
    else
        AdvanceNormal();
}

//===========================================================================
#if 0
static void Advance() {
    static int cycleSurplus = 0;

    // Check if the emulator should run at full-speed.
    bool forceFullspeed = (s_speed == SPEED_MAX) || KeybIsKeyDown(SDL_SCANCODE_SCROLLLOCK);
    bool tmpFullspeed   = DiskIsFullSpeedEligible();
    fullSpeed = forceFullspeed || tmpFullspeed;

    // Check the actual elapsed time.
    int64_t elapsed = TimerGetMsElapsed();
    while (elapsed == s_lastExecuteMs && !fullSpeed) {
        TimerWait();
        elapsed = TimerGetMsElapsed();
    }

    // See how many milliseconds we need to simulate in order to catch up to the
    // wall clock.  Allow up to 32ms of simulation time while catching up.
    int msnormal = MIN(32, int((elapsed - s_lastExecuteMs) * s_speedMultiplier));
    int ms = fullSpeed ? 32 : msnormal;

    // Advance the emulator 1 millisecond at a time.
    for (int t = 0; t < ms; ++t) {
        int cyclesAttempted = CPU_CYCLES_PER_MS - cycleSurplus;
        int cyclesExecuted  = CpuExecute(cyclesAttempted, &g_cyclesEmulated);
        cycleSurplus = cyclesExecuted - cyclesAttempted;

        // Process all scheduled events.
        Event event;
        while (SchedulerDequeue(g_cyclesEmulated, &event))
            event.func(event.cycle);

        DiskUpdatePosition(cyclesExecuted);
        JoyUpdatePosition(cyclesExecuted);
        SpkrUpdate(cyclesExecuted);

        // Check if fullspeed status has changed.
        if (fullSpeed) {
            tmpFullspeed = DiskIsFullSpeedEligible();
            fullSpeed    = forceFullspeed || tmpFullspeed;
            if (!fullSpeed)
                ms = msnormal;
        }
    }

    s_lastExecuteMs = elapsed;

    if (!fullSpeed)
        TimerWait();
}
#endif

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
static void UpdateEmulator(int64_t cycle) {
    // Interrupt the emulator every millisecond.
    SchedulerEnqueue(Event(cycle + CPU_CYCLES_PER_MS, UpdateEmulator));
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
    DWORD speed;
    RegLoadValue("Configuration", "Computer Emulation", (DWORD *)&apple2e);
    RegLoadValue("Configuration", "Joystick Emulation", &joytype);
    RegLoadValue("Configuration", "Serial Port", &serialport);
    RegLoadValue("Configuration", "Emulation Speed", &speed);
    RegLoadValue("Configuration", "Enhance Disk Speed", (DWORD *)&optEnhancedDisk);
    RegLoadValue("Configuration", "Monochrome Video", (DWORD *)&g_optMonochrome);
    SetSpeed((int)speed);
}

//===========================================================================
EEmulatorMode GetMode() {
    return s_mode;
}

//===========================================================================
int GetSpeed() {
    return s_speed;
}

//===========================================================================
bool IsBehind() {
    return s_isBehind;
}

//===========================================================================
void SetMode(EEmulatorMode mode) {
    if (s_mode == mode)
        return;

    if (mode == EMULATOR_MODE_RUNNING) {
        TimerReset(g_cyclesEmulated);
    }

    s_mode = mode;
}

//===========================================================================
void SetSpeed(int newSpeed) {
    s_speed = MAX(1, newSpeed);
    TimerSetSpeedMultiplier(s_speed * (1.0f / SPEED_NORMAL));
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
        s_isBehind = false;
        s_lastFullSpeed = false;
        g_cyclesEmulated = 0;
        SchedulerInitialize();
        TimerInitialize(1);
        SchedulerEnqueue(Event(CPU_CYCLES_PER_MS, UpdateEmulator));

        SetMode(EMULATOR_MODE_LOGO);
        SetSpeed(SPEED_NORMAL);
        LoadConfiguration();
        DebugInitialize();
        JoyInitialize();
        MemInitialize();
        VideoInitialize();
        FrameCreateWindow();

        while (s_mode != EMULATOR_MODE_SHUTDOWN) {
            switch (s_mode) {
                case EMULATOR_MODE_RUNNING:
                    Advance();
                    break;
                case EMULATOR_MODE_STEPPING:
                    DebugContinueStepping();
                    break;
                default:
                    Sleep(1);
                    break;
            }
            WindowUpdate();
        }

        DebugDestroy();
        CommDestroy();
        MemDestroy();
        SpkrDestroy();
        VideoDestroy();
        TimerDestroy();
    } while (restart);

    DiskDestroy();
    ImageDestroy();
    WindowDestroy();
    return 0;
}
