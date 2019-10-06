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
*   Variables
*
***/

int64_t   g_cyclesEmulated;
HINSTANCE g_instance;

static EAppleType    s_appleType;
static bool          s_behind;
static bool          s_lastFullSpeed;
static EEmulatorMode s_mode;
static char          s_programDir[260];
static bool          s_restartRequested;
static int           s_speed;

static const char * s_appleTypeNames[] = {
    "Apple ][",
    "Apple ][+",
    "Apple //e"
};
static_assert(ARRSIZE(s_appleTypeNames) == APPLE_TYPES, "apple type name mismatch");


/****************************************************************************
*
*   Local functions
*
***/

//===========================================================================
static void AdvanceNormal() {
    // Advance the emulator until we catch up to the expected number of elapsed
    // cycles.  Run at most 16ms worth of cycles.
    int64_t elapsedCycles = TimerUpdateElapsedCycles();
    int64_t stopCycle = MIN(elapsedCycles, g_cyclesEmulated + 16 * CPU_CYCLES_PER_MS);
    s_behind = (elapsedCycles > stopCycle);
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

    // Delay until the next emulator tick (unless the emulator has fallen
    // significantly behind the wall clock).
    if (!s_behind)
        TimerWait();
}

//===========================================================================
static void AdvanceFullSpeed() {
    // In full speed mode, advance 32ms worth of cycles at a time without
    // delays.
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
        TimerReset();
    s_lastFullSpeed = fullSpeed;

    if (fullSpeed)
        AdvanceFullSpeed();
    else
        AdvanceNormal();
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
static void UpdateEmulator(int64_t cycle) {
    // Interrupt the emulator once every millisecond.
    SchedulerEnqueue(cycle + CPU_CYCLES_PER_MS, UpdateEmulator);
}

//===========================================================================
static void GetProgramDirectory() {
    GetModuleFileName((HINSTANCE)0, s_programDir, ARRSIZE(s_programDir));
    s_programDir[ARRSIZE(s_programDir) - 1] = '\0';
    int loop = StrLen(s_programDir);
    while (loop--) {
        if (s_programDir[loop] == '\\' || s_programDir[loop] == ':') {
            s_programDir[loop + 1] = '\0';
            break;
        }
    }
}

//===========================================================================
static void LoadConfiguration() {
    int speed, appleType, enhancedDisk, monochrome;
    ConfigGetValue("Computer Emulation", &appleType, APPLE_TYPE_IIE);
    ConfigGetValue("Joystick Emulation", &joyType, 0);
    ConfigGetValue("Serial Port", &serialPort, 0);
    ConfigGetValue("Emulation Speed", &speed, SPEED_NORMAL);
    ConfigGetValue("Enhance Disk Speed", &enhancedDisk, 1);
    ConfigGetValue("Monochrome Video", &monochrome, 0);
    EmulatorSetAppleType(EAppleType(MIN(appleType, APPLE_TYPES - 1)));
    EmulatorSetSpeed((int)speed);
    optEnhancedDisk = enhancedDisk != 0;
    g_optMonochrome = monochrome != 0;
}


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
EAppleType EmulatorGetAppleType() {
    return s_appleType;
}

//===========================================================================
EEmulatorMode EmulatorGetMode() {
    return s_mode;
}

//===========================================================================
const char * EmulatorGetProgramDirectory() {
    return s_programDir;
}

//===========================================================================
int EmulatorGetSpeed() {
    return s_speed;
}

//===========================================================================
const char * EmulatorGetTitle() {
    return s_appleTypeNames[s_appleType];
}

//===========================================================================
bool EmulatorIsBehind() {
    return s_behind;
}

//===========================================================================
void EmulatorRequestRestart() {
    s_restartRequested = true;
}

//===========================================================================
void EmulatorReset() {
    MemReset();
    DiskBoot();
    VideoResetState();
    CommReset();
    JoyReset();
}

//===========================================================================
void EmulatorSetAppleType(EAppleType type) {
    s_appleType = type;

    switch (s_appleType) {
        case APPLE_TYPE_II:
        case APPLE_TYPE_IIPLUS:
            CpuSetType(CPU_TYPE_6502);
            break;
        case APPLE_TYPE_IIE:
        default:
            CpuSetType(CPU_TYPE_65C02);
            break;
    }
}

//===========================================================================
void EmulatorSetMode(EEmulatorMode mode) {
    if (s_mode == mode)
        return;

    s_mode = mode;
    if (s_mode == EMULATOR_MODE_RUNNING)
        TimerReset();
}

//===========================================================================
void EmulatorSetSpeed(int newSpeed) {
    s_speed = MIN(MAX(1, newSpeed), SPEED_MAX);
    TimerSetSpeedMultiplier(s_speed * (1.0f / SPEED_NORMAL));
}

//===========================================================================
int APIENTRY WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int) {
    g_instance = inst;
    GdiSetBatchLimit(512);
    GetProgramDirectory();
    FrameRegisterClass();
    ImageInitialize();
    if (!WindowInitialize())
        return 1;
    if (!DiskInitialize())
        return 1;
    SpkrInitialize();

    do {
        s_behind = false;
        s_lastFullSpeed = false;
        s_restartRequested = false;
        g_cyclesEmulated = 0;
        SchedulerInitialize();
        TimerInitialize(1);
        SchedulerEnqueue(CPU_CYCLES_PER_MS, UpdateEmulator);

        EmulatorSetMode(EMULATOR_MODE_LOGO);
        EmulatorSetSpeed(SPEED_NORMAL);
        ConfigLoad();
        LoadConfiguration();
        DebugInitialize();
        JoyInitialize();
        MemInitialize();
        MemInitialize2();
        VideoInitialize();
        FrameCreateWindow();

        while (s_mode != EMULATOR_MODE_SHUTDOWN) {
            switch (s_mode) {
                case EMULATOR_MODE_RUNNING:  Advance();      break;
                case EMULATOR_MODE_STEPPING: DebugAdvance(); break;
                default:                     Sleep(1);       break;
            }
            WindowUpdate();
        }

        VideoDestroy();
        CommDestroy();
        MemDestroy2();
        //MemDestroy();
        DebugDestroy();
        ConfigSave();
        TimerDestroy();
    } while (s_restartRequested);

    SpkrDestroy();
    DiskDestroy();
    ImageDestroy();
    WindowDestroy();
    return 0;
}
