/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

void  KeybGetCapsStatus(BOOL * status);
BYTE  KeybGetKeycode();
DWORD KeybGetNumQueries();
void  KeybQueueKeypress(int virtkey, BOOL extended);

BYTE KeybReadData(WORD, BYTE, BYTE, BYTE);
BYTE KeybReadFlag(WORD, BYTE, BYTE, BYTE);
