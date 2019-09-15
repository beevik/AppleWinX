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
*   Windows implementation
*
***/

#ifdef OS_WINDOWS

static int64_t perfFreq;
static HANDLE  timer;
static HANDLE  timerSemaphore;
static HANDLE  timerThread;
static int64_t startCount;

struct TimerThreadData {
    int periodMs;
};

//===========================================================================
static DWORD WINAPI TimerThread(LPVOID param) {
    TimerThreadData * data = (TimerThreadData *)param;
    LARGE_INTEGER dueTime;
    dueTime.QuadPart = int64_t(data->periodMs) * -10000LL;
    delete data;

    SetThreadAffinityMask(GetCurrentThread(), 1);
    SwitchToThread();

    while (timer != NULL) {
        if (!SetWaitableTimer(timer, &dueTime, 0, NULL, NULL, 0))
            break;

        if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0)
            break;

        ReleaseSemaphore(timerSemaphore, 1, NULL);
    }

    return 0;
}

//===========================================================================
void TimerDestroy() {
    CloseHandle(timer);
    WaitForSingleObject(timerThread, INFINITE);
    CloseHandle(timerSemaphore);
    CloseHandle(timerThread);
    timer          = NULL;
    timerThread    = NULL;
    timerSemaphore = NULL;
}

//===========================================================================
void TimerInitialize(int periodMs) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    perfFreq = freq.QuadPart / 1000LL;

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    startCount = counter.QuadPart;

    timerSemaphore = CreateSemaphore(NULL, 0, 1, NULL);
    timer = CreateWaitableTimer(NULL, FALSE, NULL);

    TimerThreadData * data = new TimerThreadData;
    data->periodMs = periodMs;
    DWORD timerthreadid;
    timerThread = CreateThread(NULL, 0, TimerThread, data, 0, &timerthreadid);
}

//===========================================================================
int64_t TimerGetMsElapsed() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return int64_t(counter.QuadPart - startCount) / perfFreq;
}

//===========================================================================
void TimerWait() {
    WaitForSingleObject(timerSemaphore, INFINITE);
}

#endif // OS_WINDOWS
