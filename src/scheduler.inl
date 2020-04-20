/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

//===========================================================================
template<typename T>
inline void Scheduler<T>::Clear() {
    m_queue.clear();
}

//===========================================================================
template<typename T>
inline bool Scheduler<T>::Dequeue(int64_t elapsedCycles, int64_t * eventCycle, T * value) {
    if (m_queue.size() == 0 || m_queue[0].cycle > elapsedCycles)
        return false;

    *eventCycle = m_queue[0].cycle;
    *value = m_queue[0].value;

    int index = 0;
    m_queue[index] = m_queue.back();
    m_queue.pop_back();
    int eventCount = (int)m_queue.size();
    while (index < eventCount) {
        int leftIndex = index * 2 + 1;
        int rightIndex = leftIndex + 1;
        int newIndex;
        if (leftIndex < eventCount && m_queue[index].cycle > m_queue[leftIndex].cycle) {
            if (rightIndex >= eventCount || m_queue[leftIndex].cycle < m_queue[rightIndex].cycle)
                newIndex = leftIndex;
            else
                newIndex = rightIndex;
        }
        else if (rightIndex < eventCount && m_queue[index].cycle > m_queue[rightIndex].cycle)
            newIndex = rightIndex;
        else
            break;

        std::swap(m_queue[index], m_queue[newIndex]);
        index = newIndex;
    }

    return true;
}

//===========================================================================
template<typename T>
inline bool Scheduler<T>::Enqueue(int64_t cycle, T value) {
    int index = (int)m_queue.size();
    m_queue.push_back(Event(cycle, value));

    while (index > 0) {
        int parentIndex = index / 2;
        if (m_queue[parentIndex].cycle <= m_queue[index].cycle)
            break;
        std::swap(m_queue[parentIndex], m_queue[index]);
        index = parentIndex;
    }
    return true;
}

//===========================================================================
template<typename T>
inline int64_t Scheduler<T>::PeekTime() const {
    return m_queue.size() > 0 ? m_queue[0].cycle : LLONG_MAX;
}
