/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

constexpr int MAX_EVENTS = 15;

static int   s_eventCount = 0;
static Event s_eventQueue[MAX_EVENTS];

//===========================================================================
bool SchedulerDequeue (int64_t elapsedCycles, Event * event) {
    if (s_eventCount == 0 || s_eventQueue[0].cycle > elapsedCycles)
        return false;

    *event = s_eventQueue[0];

    int index = 0;
    s_eventQueue[index] = s_eventQueue[--s_eventCount];
    while (index < s_eventCount) {
        int leftIndex  = index * 2 + 1;
        int rightIndex = leftIndex + 1;
        int newIndex;
        if (leftIndex < s_eventCount && s_eventQueue[index].cycle > s_eventQueue[leftIndex].cycle) {
            if (rightIndex >= s_eventCount || s_eventQueue[leftIndex].cycle < s_eventQueue[rightIndex].cycle)
                newIndex = leftIndex;
            else
                newIndex = rightIndex;
        }
        else if (rightIndex < s_eventCount && s_eventQueue[index].cycle > s_eventQueue[rightIndex].cycle)
            newIndex = rightIndex;
        else
            break;

        std::swap(s_eventQueue[index], s_eventQueue[newIndex]);
        index = newIndex;
    }

    return true;
}

//===========================================================================
bool SchedulerEnqueue(Event event) {
    if (s_eventCount >= MAX_EVENTS)
        return false;

    int index = s_eventCount++;
    s_eventQueue[index] = event;
    while (index > 0) {
        int parentIndex = index / 2;
        if (s_eventQueue[parentIndex].cycle <= s_eventQueue[index].cycle)
            break;
        std::swap(s_eventQueue[parentIndex], s_eventQueue[index]);
        index = parentIndex;
    }
    return true;
}

//===========================================================================
void SchedulerInitialize() {
    s_eventCount = 0;
}

//===========================================================================
int64_t SchedulerPeekTime() {
    if (s_eventCount == 0)
        return LLONG_MAX;
    return s_eventQueue[0].cycle;
}
