/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

template<typename T>
class Scheduler {
public:
    void Clear();
    bool Dequeue(int64_t elapsedCycles, int64_t * eventCycle, T * value);
    bool Enqueue(int64_t cycle, T value);
    int64_t PeekTime() const;

private:
    struct Event {
        int64_t cycle;
        T       value;
        Event(int64_t cycle, T value) : cycle(cycle), value(value) {}
    };

    std::vector<Event> m_queue;
};

using FEvent = void (*)(int64_t scheduledTime);

extern Scheduler<FEvent> g_scheduler;

#include "scheduler.inl"
