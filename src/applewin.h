/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

constexpr int BUILDNUMBER   = 4;

constexpr int VERSIONMAJOR  = 1;
constexpr int VERSIONMINOR  = 11;

constexpr int MODE_LOGO     = 0;
constexpr int MODE_PAUSED   = 1;
constexpr int MODE_RUNNING  = 2;
constexpr int MODE_DEBUG    = 3;
constexpr int MODE_STEPPING = 4;
constexpr int MODE_SHUTDOWN = 5;

constexpr int SPEED_NORMAL  = 10;
constexpr int SPEED_MAX     = 40;

constexpr double CPU_HZ            = 1020484.32;
constexpr DWORD  CPU_CYCLES_PER_MS = DWORD(CPU_HZ / 1000.0);

#define  MAX(a,b)   (((a) > (b)) ? (a) : (b))
#define  MIN(a,b)   (((a) < (b)) ? (a) : (b))
#define  ARRSIZE(x) (sizeof(x) / sizeof(x[0]))

extern BOOL         apple2e;
extern BOOL         autoboot;
extern BOOL         behind;
extern DWORD        cyclenum;
extern BOOL         fullspeed;
extern HINSTANCE    instance;
extern int          mode;
extern DWORD        needsprecision;
extern BOOL         optenhancedisk;
extern BOOL         optmonochrome;
extern char         progdir[MAX_PATH];
extern BOOL         resettiming;
extern BOOL         restart;
extern DWORD        speed;
extern const char * title;
extern DWORD        totalcycles;

void SetMode(int newmode);
