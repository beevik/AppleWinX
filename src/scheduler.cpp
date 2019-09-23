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

static int   eventCount = 0;
static Event eventQueue[MAX_EVENTS];

//===========================================================================
bool SchedulerDequeue (int64_t elapsedCycles, Event * event) {
    if (eventCount == 0 || eventQueue[0].cycle >= elapsedCycles)
        return false;

    *event = eventQueue[0];

    int index = 0;
    eventQueue[index] = eventQueue[--eventCount];
    while (index < eventCount) {
        int leftIndex  = index * 2 + 1;
        int rightIndex = leftIndex + 1;
        int newIndex;
        if (eventQueue[index].cycle > eventQueue[leftIndex].cycle) {
            if (eventQueue[leftIndex].cycle < eventQueue[rightIndex].cycle)
                newIndex = leftIndex;
            else
                newIndex = rightIndex;
        }
        else if (eventQueue[index].cycle > eventQueue[rightIndex].cycle)
            newIndex = rightIndex;
        else
            break;

        std::swap(eventQueue[index], eventQueue[newIndex]);
        index = newIndex;
    }

    return true;
}

//===========================================================================
bool SchedulerEnqueue(Event event) {
    if (eventCount >= MAX_EVENTS)
        return false;

    int index = eventCount++;
    eventQueue[index] = event;
    while (index > 0) {
        int parentIndex = index / 2;
        if (eventQueue[parentIndex].cycle <= eventQueue[index].cycle)
            break;
        std::swap(eventQueue[parentIndex], eventQueue[index]);
        index = parentIndex;
    }
    return true;
}
