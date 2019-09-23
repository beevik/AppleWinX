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

ETimerMode g_timerMode = TIMER_MODE_NORMAL;

static double s_cycleMultiplier = 1.0;


/****************************************************************************
*
*   Windows implementation
*
***/

#ifdef OS_WINDOWS

static int64_t s_cyclesElapsed;
static int64_t s_lastUpdateTimeUs;
static int64_t s_perfFreqMs;
static int64_t s_perfFreqUs;
static int64_t s_startCount;
static HANDLE  s_timer;
static HANDLE  s_timerSemaphore;
static HANDLE  s_timerThread;

struct TimerThreadInit {
    int periodMs;
};

//===========================================================================
static DWORD WINAPI TimerThread(LPVOID param) {
    TimerThreadInit * init = (TimerThreadInit *)param;
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = int64_t(init->periodMs) * -10000LL;
    delete init;

    SetThreadAffinityMask(GetCurrentThread(), 1);
    SwitchToThread();

    while (s_timer != NULL) {
        if (!SetWaitableTimer(s_timer, &dueTime, 0, NULL, NULL, 0))
            break;

        if (WaitForSingleObject(s_timer, INFINITE) != WAIT_OBJECT_0)
            break;

        ReleaseSemaphore(s_timerSemaphore, 1, NULL);
    }

    return 0;
}


//===========================================================================
static void UpdateElapsedCycles() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    int64_t timeUs      = counter.QuadPart / s_perfFreqUs;
    double  cyclesSince = double(timeUs - s_lastUpdateTimeUs) * s_cycleMultiplier;
    s_lastUpdateTimeUs = timeUs;
    s_cyclesElapsed += int64_t(cyclesSince);
}

//===========================================================================
void TimerDestroy() {
    CloseHandle(s_timer);
    WaitForSingleObject(s_timerThread, INFINITE);
    CloseHandle(s_timerSemaphore);
    CloseHandle(s_timerThread);
    s_timer          = NULL;
    s_timerThread    = NULL;
    s_timerSemaphore = NULL;
}

//===========================================================================
void TimerInitialize(int periodMs) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    s_perfFreqMs = freq.QuadPart / 1000LL;
    s_perfFreqUs = freq.QuadPart / 1000000LL;

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    s_startCount = counter.QuadPart;

    s_timerSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
    s_timer = CreateWaitableTimer(NULL, FALSE, NULL);

    TimerThreadInit * init = new TimerThreadInit;
    init->periodMs = periodMs;
    DWORD timerthreadid;
    s_timerThread = CreateThread(NULL, 0, TimerThread, init, 0, &timerthreadid);
}

//===========================================================================
int64_t TimerGetCyclesElapsed() {
    UpdateElapsedCycles();
    return s_cyclesElapsed;
}

//===========================================================================
int64_t TimerGetMsElapsed() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return int64_t(counter.QuadPart - s_startCount) / s_perfFreqMs;
}

//===========================================================================
void TimerReset(int64_t cyclesElapsed) {
    s_cyclesElapsed = cyclesElapsed;

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    s_lastUpdateTimeUs = counter.QuadPart / s_perfFreqUs;
}

//===========================================================================
void TimerSetMode(ETimerMode mode) {
    if (mode == g_timerMode)
        return;

    UpdateElapsedCycles();
    g_timerMode = mode;
}

//===========================================================================
void TimerSetSpeedMultiplier(float multiplier) {
    UpdateElapsedCycles();
    s_cycleMultiplier = double(multiplier) * CPU_CYCLES_PER_US;
}

//===========================================================================
void TimerSleepUs(int us) {
    Sleep(us / 1000);
}

//===========================================================================
void TimerWait() {
    WaitForSingleObject(s_timerSemaphore, INFINITE);
}

#endif // OS_WINDOWS
