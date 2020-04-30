/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
***/

#pragma once

class Recording {
public:
    enum EEventType {
        EVENT_TYPE_SYNCCODE,
        EVENT_TYPE_KEYDOWN,
        EVENT_TYPE_KEYUP,
    };

    struct EventBase {
        int64_t counter;
    };

    struct EventKeyUp : public EventBase {
        SDL_KeyboardEvent key;
    };

    struct EventKeyDown : public EventBase {
        SDL_KeyboardEvent key;
    };

    struct EventSyncCode : public EventBase {
        uint32_t syncCode;
    };

    bool Load(const char * filename);
    bool Save(const char * filename);

    void AddEvent(const EventSyncCode & event);
    void AddEvent(const EventKeyUp & event);
    void AddEvent(const EventKeyDown & event);

    int64_t NextEventTime() const { return m_nextTime; }
    bool NextEventType(EEventType * type) const;

    bool GetNextEvent(EventSyncCode * event);
    bool GetNextEvent(EventKeyUp * event);
    bool GetNextEvent(EventKeyDown * event);

private:
    std::vector<uint8_t> m_stream;

    size_t  m_readOffset = 0;
    int64_t m_nextTime   = 0;

    void AddKeyEvent(EEventType type, int64_t counter, const SDL_KeyboardEvent & key);
    bool GetKeyEvent(int64_t * counter, SDL_KeyboardEvent * key);
    bool UpdateNextTime();
};
