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

HINSTANCE g_instance;
Memory *  g_memory;
Cpu *     g_cpu;

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
static void AdvanceUntil(int64_t stopCycle) {
    static int64_t s_counter = 0;

    // Step the CPU until the next scheduled event.
    int64_t nextEventCycle = MIN(g_scheduler.PeekTime(), stopCycle);
    while (g_cpu->Cycle() < nextEventCycle)
        g_cpu->Step();

    // Process all scheduled events happening before or on the current
    // cycle.
    int64_t cycle;
    FEvent eventFunc;
    while (g_scheduler.Dequeue(g_cpu->Cycle(), &cycle, &eventFunc))
        eventFunc(cycle);
}

//===========================================================================
static void Advance() {
    // When switching back to normal speed, reset the timer.
    bool fullSpeed = TimerIsFullSpeed();
    if (s_lastFullSpeed && !fullSpeed)
        TimerReset(g_cpu->Cycle());
    s_lastFullSpeed = fullSpeed;

    if (fullSpeed) {
        // In full speed mode, advance 32ms worth of cycles at a time without
        // delays.
        int64_t stopCycle = g_cpu->Cycle() + 32 * CPU_CYCLES_PER_MS;
        while (g_cpu->Cycle() < stopCycle && TimerIsFullSpeed())
            AdvanceUntil(stopCycle);
    }
    else {
        // In normal mode, advance the emulator until we catch up to the
        // expected number of elapsed cycles.  Run at most 16ms worth of cycles.
        int64_t elapsedCycles = TimerUpdateElapsedCycles();
        int64_t stopCycle = MIN(elapsedCycles, g_cpu->Cycle() + 16 * CPU_CYCLES_PER_MS);
        s_behind = (elapsedCycles > stopCycle);
        while (g_cpu->Cycle() < stopCycle && !TimerIsFullSpeed())
            AdvanceUntil(stopCycle);

        // Delay until the next emulator tick (unless the emulator has fallen
        // significantly behind the wall clock).
        if (!s_behind)
            TimerWait();
    }
}

//===========================================================================
static void UpdateEmulator(int64_t cycle) {
    // Interrupt the emulator once every millisecond.
    DiskUpdatePosition(CPU_CYCLES_PER_MS);
    g_scheduler.Enqueue(cycle + CPU_CYCLES_PER_MS, UpdateEmulator);
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
    ConfigLoad();

    int speed, appleType, enhancedDisk, monochrome;
    ConfigGetValue("Computer Emulation", &appleType, APPLE_TYPE_IIE);
    ConfigGetValue("Joystick Emulation", &joyType, 0);
    ConfigGetValue("Serial Port", &serialPort, 0);
    ConfigGetValue("Emulation Speed", &speed, SPEED_NORMAL);
    ConfigGetValue("Enhance Disk Speed", &enhancedDisk, 1);
    ConfigGetValue("Monochrome Video", &monochrome, 0);

    EmulatorSetAppleType(EAppleType(MIN(appleType, APPLE_TYPES - 1)));
    EmulatorSetSpeed(speed);
    g_optEnhancedDisk = enhancedDisk != 0;
    g_optMonochrome   = monochrome != 0;
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
    g_cpu->Initialize();
    VideoResetState();
    SpkrReset();
    CommReset();
    JoyReset();
}

//===========================================================================
void EmulatorSetAppleType(EAppleType type) {
    s_appleType = type;

    switch (s_appleType) {
        case APPLE_TYPE_II:
        case APPLE_TYPE_IIPLUS:
            g_cpu->SetType(CPU_TYPE_6502);
            break;
        case APPLE_TYPE_IIE:
        default:
            g_cpu->SetType(CPU_TYPE_65C02);
            break;
    }
}

//===========================================================================
void EmulatorSetMode(EEmulatorMode mode) {
    if (s_mode == mode)
        return;

    s_mode = mode;
    if (s_mode == EMULATOR_MODE_RUNNING)
        TimerReset(g_cpu->Cycle());
}

//===========================================================================
void EmulatorSetSpeed(int newSpeed) {
    s_speed = MIN(MAX(1, newSpeed), SPEED_MAX);
    TimerSetSpeedMultiplier(s_speed * (1.0f / SPEED_NORMAL));
}

//===========================================================================
int APIENTRY WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int) {
    g_instance = inst;

    g_memory = new Memory();
    g_cpu = new Cpu(g_memory);

    GdiSetBatchLimit(512);
    GetProgramDirectory();
    LoadConfiguration();
    FrameRegisterClass();
    ImageInitialize();
    if (!WindowInitialize())
        return 1;
    SpkrInitialize();


    do {
        s_behind = false;
        s_lastFullSpeed = false;
        s_restartRequested = false;
        TimerInitialize(1);
        g_scheduler.Enqueue(CPU_CYCLES_PER_MS, UpdateEmulator);

        EmulatorSetMode(EMULATOR_MODE_LOGO);
        EmulatorSetSpeed(SPEED_NORMAL);
        JoyInitialize();
        MemInitialize();
        g_cpu->Initialize();
        DiskInitialize();
        VideoInitialize();
        FrameCreateWindow();

        while (s_mode != EMULATOR_MODE_SHUTDOWN) {
            switch (s_mode) {
                case EMULATOR_MODE_RUNNING:
                    Advance();
                    break;
                default:
                    Sleep(1);
                    break;
            }
            WindowUpdate();
        }

        VideoDestroy();
        CommDestroy();
        DiskDestroy();
        MemDestroy();
        g_scheduler.Clear();
        TimerDestroy();
    } while (s_restartRequested);

    SpkrDestroy();
    ImageDestroy();
    WindowDestroy();
    ConfigSave();
    DebugDestroy();

    delete g_cpu;
    delete g_memory;
    return 0;
}
