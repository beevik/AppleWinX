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

static int s_benchmarks[256];
static int s_benchmarkIndex;


/****************************************************************************
*
*   Windows implementation
*
***/

#ifdef OS_WINDOWS

static double   s_cyclesElapsed;
static int64_t  s_lastUpdateCount;
static int64_t  s_perfFreqMs;
static double   s_cyclesPerTick;
static uint32_t s_fullSpeedBits;
static float    s_speedMultiplier;
static int64_t  s_startCount;
static int64_t  s_ticksPerSecond;
static HANDLE   s_timer;
static HANDLE   s_timerSemaphore;
static HANDLE   s_timerThread;

struct TimerThreadInit {
    int periodMs;
};


/****************************************************************************
*
*   Local functions
*
***/

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
    int64_t tickDelta = counter.QuadPart - s_lastUpdateCount;
    s_cyclesElapsed += tickDelta * s_cyclesPerTick;
    s_lastUpdateCount = counter.QuadPart;
}


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
void TimerBenchmarkRecord(int timeUs) {
    s_benchmarks[s_benchmarkIndex] = timeUs;
    s_benchmarkIndex = (s_benchmarkIndex + 1) % ARRSIZE(s_benchmarks);
}

//===========================================================================
int64_t TimerBenchmarkStart() {
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return count.QuadPart;
}

//===========================================================================
int TimerBenchmarkStop(int64_t handle) {
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    int64_t countElapsed = count.QuadPart - handle;
    return int((countElapsed * 1000) / s_perfFreqMs);
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
    s_benchmarkIndex = 0;
    memset(s_benchmarks, 0, sizeof(s_benchmarks));

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    s_ticksPerSecond = freq.QuadPart;
    s_perfFreqMs     = freq.QuadPart / 1000LL;
    s_cyclesPerTick  = CPU_HZ / s_ticksPerSecond;

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
int64_t TimerGetMsElapsed() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return int64_t(counter.QuadPart - s_startCount) / s_perfFreqMs;
}

//===========================================================================
float TimerGetSpeedMultiplier() {
    return s_speedMultiplier;
}

//===========================================================================
bool TimerIsFullSpeed() {
    return s_fullSpeedBits != 0;
}

//===========================================================================
void TimerReset(int64_t cyclesEmulated) {
    s_cyclesElapsed = double(cyclesEmulated);

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    s_lastUpdateCount = counter.QuadPart;
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
    s_speedMultiplier = multiplier;
    s_cyclesPerTick = s_speedMultiplier * CPU_HZ / s_ticksPerSecond;
}

//===========================================================================
void TimerSleepUs(int us) {
    Sleep(us / 1000);
}

//===========================================================================
void TimerUpdateFullSpeed(EFullSpeed reason, bool on) {
    if (on)
        s_fullSpeedBits |= (1 << reason);
    else
        s_fullSpeedBits &= ~(1 << reason);
}

//===========================================================================
int64_t TimerUpdateElapsedCycles() {
    UpdateElapsedCycles();
    return int64_t(s_cyclesElapsed);
}

//===========================================================================
void TimerWait() {
    WaitForSingleObject(s_timerSemaphore, INFINITE);
}

#endif // OS_WINDOWS
