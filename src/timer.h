/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

void    TimerDestroy();
void    TimerInitialize(int periodMs);
int64_t TimerGetMsElapsed();
void    TimerWait();
