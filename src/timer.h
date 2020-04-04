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

enum EFullSpeed {
    FULLSPEED_KEYDOWN,
    FULLSPEED_DISK_MOTOR_ON,
};

extern ETimerMode g_timerMode;

void    TimerBenchmarkRecord(int timeUs);
int64_t TimerBenchmarkStart();
int     TimerBenchmarkStop(int64_t handle); // returns elapsed microseconds
void    TimerDestroy();
void    TimerInitialize(int periodMs);
int64_t TimerGetMsElapsed();
float   TimerGetSpeedMultiplier();
bool    TimerIsFullSpeed();
void    TimerReset(int64_t cyclesEmulated);
void    TimerSetMode(ETimerMode mode);
void    TimerSetSpeedMultiplier(float multiplier);
void    TimerSleepUs(int us);
void    TimerUpdateFullSpeed(EFullSpeed reason, bool on);
int64_t TimerUpdateElapsedCycles();
void    TimerWait();
