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

enum EFullSpeedSetting {
    FULL_SPEED_SETTING_KEYDOWN,
    FULL_SPEED_SETTING_DISK_MOTOR_ON,
};

extern ETimerMode g_timerMode;

void    TimerDestroy();
void    TimerInitialize(int periodMs);
int64_t TimerGetMsElapsed();
bool    TimerIsFullSpeed();
void    TimerReset();
void    TimerSetMode(ETimerMode mode);
void    TimerSetSpeedMultiplier(float multiplier);
void    TimerSleepUs(int us);
void    TimerUpdateFullSpeedSetting(EFullSpeedSetting reason, bool on);
int64_t TimerUpdateElapsedCycles();
void    TimerWait();
