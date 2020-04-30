/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
***/

#include "pch.h"
#pragma  hdrstop

//===========================================================================
bool Recording::Load(const char * filename) {
    m_readOffset = 0;

    FILE * file = fopen(filename, "rb");
    if (!file)
        return false;

    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    m_stream.resize(size);

    fseek(file, 0, SEEK_SET);
    size_t bytesRead = fread(&m_stream.begin(), size, 1, file);
    fclose(file);

    if (bytesRead != 1)
        return false;

    UpdateNextTime();
    return true;
}

//===========================================================================
bool Recording::Save(const char * filename) {
    FILE * file = fopen(filename, "wb");
    if (!file)
        return false;

    size_t bytesWritten = fwrite(&m_stream.begin(), m_stream.size(), 1, file);
    fclose(file);

    return bytesWritten == 1;
}

//===========================================================================
void Recording::AddEvent(const Recording::EventSyncCode & event) {
    size_t offset = m_stream.size();
    m_stream.resize(offset + 13);
    uint8_t * ptr = &m_stream[offset];
    ptr[0] = (uint8_t)EEventType::EVENT_TYPE_SYNCCODE;
    *(int64_t *)(ptr + 1) = event.counter;
    *(uint32_t *)(ptr + 9) = event.syncCode;
}

//===========================================================================
void Recording::AddEvent(const Recording::EventKeyUp & event) {
    AddKeyEvent(EEventType::EVENT_TYPE_KEYUP, event.counter, event.key);
}

//===========================================================================
void Recording::AddEvent(const Recording::EventKeyDown & event) {
    AddKeyEvent(EEventType::EVENT_TYPE_KEYDOWN, event.counter, event.key);
}

//===========================================================================
void Recording::AddKeyEvent(Recording::EEventType type, int64_t counter, const SDL_KeyboardEvent & key) {
    size_t offset = m_stream.size();
    m_stream.resize(offset + 13);
    uint8_t * ptr = &m_stream[offset];
    ptr[0] = (uint8_t)type;
    *(int64_t *)(ptr + 1) = counter;
    uint32_t packed = (uint32_t)key.keysym.scancode | ((uint32_t)key.keysym.mod << 16);
    *(uint32_t *)(ptr + 9) = packed;
}

//===========================================================================
bool Recording::NextEventType(Recording::EEventType * type) const {
    if (m_nextTime < 0)
        return false;

    *type = (EEventType)m_stream[m_readOffset];
    return true;
}

//===========================================================================
bool Recording::GetNextEvent(Recording::EventSyncCode * event) {
    if (m_readOffset + 13 >= m_stream.size())
        return false;

    event->counter = *(int64_t *)m_stream[m_readOffset + 1];
    event->syncCode = *(uint32_t *)&m_stream[m_readOffset + 9];

    m_readOffset += 13;
    UpdateNextTime();
    return true;
}

//===========================================================================
bool Recording::GetNextEvent(Recording::EventKeyUp * event) {
    return GetKeyEvent(&event->counter, &event->key);
}

//===========================================================================
bool Recording::GetNextEvent(Recording::EventKeyDown * event) {
    return GetKeyEvent(&event->counter, &event->key);
}

//===========================================================================
bool Recording::GetKeyEvent(int64_t * counter, SDL_KeyboardEvent * key) {
    if (m_readOffset + 13 >= m_stream.size())
        return false;

    *counter = *(int64_t *)m_stream[m_readOffset + 1];

    uint32_t packed = *(uint32_t *)&m_stream[m_readOffset + 9];
    key->keysym.scancode = (SDL_Scancode)(packed & 0xffff);
    key->keysym.mod      = (Uint16)(packed >> 16);

    m_readOffset += 13;
    UpdateNextTime();
    return true;
}

//===========================================================================
bool Recording::UpdateNextTime() {
    if (m_readOffset + 9 >= m_stream.size()) {
        m_nextTime = -1;
        return false;
    }

    m_nextTime = *(int64_t *)m_stream[m_readOffset + 1];
    return true;
}
