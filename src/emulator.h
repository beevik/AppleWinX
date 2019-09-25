/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

enum EEmulatorMode {
    EMULATOR_MODE_LOGO,
    EMULATOR_MODE_PAUSED,
    EMULATOR_MODE_RUNNING,
    EMULATOR_MODE_DEBUG,
    EMULATOR_MODE_STEPPING,
    EMULATOR_MODE_SHUTDOWN,
};

constexpr int BUILDNUMBER   = 4;

constexpr int VERSIONMAJOR  = 1;
constexpr int VERSIONMINOR  = 11;

constexpr int SPEED_NORMAL  = 10;
constexpr int SPEED_MAX     = 80;

constexpr double  CPU_HZ            = 1020484.32;
constexpr DWORD   CPU_CYCLES_PER_MS = DWORD(CPU_HZ * 0.001);
constexpr double  CPU_CYCLES_PER_US = CPU_HZ * 0.000001;

#define  MAX(a,b)   (((a) > (b)) ? (a) : (b))
#define  MIN(a,b)   (((a) < (b)) ? (a) : (b))
#define  ARRSIZE(x) (sizeof(x) / sizeof(x[0]))

extern BOOL         apple2e;
extern BOOL         autoBoot;
extern HINSTANCE    instance;
extern char         programDir[MAX_PATH];
extern int64_t      g_cyclesEmulated;

EEmulatorMode EmulatorGetMode();
int           EmulatorGetSpeed();
const char *  EmulatorGetTitle();
bool          EmulatorIsBehind();
void          EmulatorRequestRestart();
void          EmulatorSetMode(EEmulatorMode mode);
void          EmulatorSetSpeed(int speed);
