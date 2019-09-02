/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern DWORD extbench;

void DebugBegin();
void DebugContinueStepping();
void DebugDestroy();
void DebugDisplay(BOOL drawbackground);
void DebugEnd();
void DebugInitialize();
void DebugProcessChar(char ch);
void DebugProcessCommand(int keycode);
