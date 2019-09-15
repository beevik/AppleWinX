/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

static int64_t perffreq;
static HANDLE  semaphore;
static HANDLE  timer;
static DWORD   timerthreadid;
static int64_t startcount;

struct timerthreaddata {
    int periodms;
};

//===========================================================================
static DWORD WINAPI TimerThread(LPVOID param) {
    timerthreaddata * data = (timerthreaddata *)param;
    LARGE_INTEGER duetime;
    duetime.QuadPart = int64_t(data->periodms) * -10000LL;
    delete data;

    while (timer != NULL) {
        if (!SetWaitableTimer(timer, &duetime, 0, NULL, NULL, 0))
            break;

        if (WaitForSingleObject(timer, INFINITE) != WAIT_OBJECT_0)
            break;

        ReleaseSemaphore(semaphore, 1, NULL);
    }

    return 0;
}

//===========================================================================
void TimerDestroy() {
    CancelWaitableTimer(timer);
    CloseHandle(timer);
    CloseHandle(semaphore);
    timer = NULL;
    semaphore = NULL;
}

//===========================================================================
void TimerInitialize(int periodms) {
    timerthreaddata * data = new timerthreaddata;
    data->periodms = periodms;

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    perffreq = freq.QuadPart / 1000LL;

    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    startcount = count.QuadPart;

    semaphore = CreateSemaphore(NULL, 0, 1, NULL);
    timer = CreateWaitableTimer(NULL, FALSE, NULL);
    CreateThread(NULL, 0, TimerThread, data, 0, &timerthreadid);
}

//===========================================================================
int64_t TimerGetMsElapsed() {
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);

    return int64_t(count.QuadPart - startcount) / perffreq;
}

//===========================================================================
void TimerWait() {
    WaitForSingleObject(semaphore, INFINITE);
}
