/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

enum EAppleType {
    APPLE_TYPE_II,
    APPLE_TYPE_IIPLUS,
    APPLE_TYPE_IIE,
    APPLE_TYPES
};

enum EEmulatorMode {
    EMULATOR_MODE_LOGO,
    EMULATOR_MODE_PAUSED,
    EMULATOR_MODE_RUNNING,
    EMULATOR_MODE_DEBUG,
    EMULATOR_MODE_STEPPING,
    EMULATOR_MODE_SHUTDOWN,
};

constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 11;
constexpr int BUILD_NUMBER  = 4;

constexpr int SPEED_NORMAL  = 10;
constexpr int SPEED_MAX     = 80;

constexpr int SLOT_MIN = 1;
constexpr int SLOT_MAX = 7;

constexpr double  CPU_HZ            = 1020484.32;
constexpr DWORD   CPU_CYCLES_PER_MS = DWORD(CPU_HZ * 0.001);
constexpr double  CPU_CYCLES_PER_US = CPU_HZ * 0.000001;

#define  MAX(a,b)   (((a) > (b)) ? (a) : (b))
#define  MIN(a,b)   (((a) < (b)) ? (a) : (b))
#define  ARRSIZE(x) (sizeof(x) / sizeof(x[0]))
#define  REF(x)     ((void)x)

class Memory2;
class Cpu;

extern HINSTANCE g_instance;
extern Memory2 * g_memory;
extern Cpu *     g_cpu;

EAppleType    EmulatorGetAppleType();
EEmulatorMode EmulatorGetMode();
const char *  EmulatorGetProgramDirectory();
int           EmulatorGetSpeed();
const char *  EmulatorGetTitle();
bool          EmulatorIsBehind();
void          EmulatorRequestRestart();
void          EmulatorReset();
void          EmulatorSetAppleType(EAppleType type);
void          EmulatorSetMode(EEmulatorMode mode);
void          EmulatorSetSpeed(int speed);
