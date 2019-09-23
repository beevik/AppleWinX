/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

enum ETimerMode {
    TIMER_MODE_NORMAL,
    TIMER_MODE_PAUSED,
    TIMER_MODE_FULLSPEED,
};

extern ETimerMode g_timerMode;

void    TimerDestroy();
void    TimerInitialize(int periodMs);
int64_t TimerGetCyclesElapsed();
int64_t TimerGetMsElapsed();
void    TimerReset(int64_t cyclesElapsed);
void    TimerSetMode(ETimerMode mode);
void    TimerSetSpeedMultiplier(float multiplier);
void    TimerSleepUs(int us);
void    TimerWait();
