/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

using FEvent = void (*)(int64_t scheduledTime);

struct Event {
    int64_t cycle = 0;
    FEvent  func  = nullptr;

    Event() { }
    Event(int64_t cycle, FEvent func) : cycle(cycle), func(func) { }
};

bool    SchedulerDequeue (int64_t elapsedCycles, Event * event);
bool    SchedulerEnqueue(Event event);
void    SchedulerInitialize();
int64_t SchedulerPeekTime();
