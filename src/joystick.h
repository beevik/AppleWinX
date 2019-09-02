/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern DWORD joytype;

void JoyInitialize();
BOOL JoyProcessKey(int virtkey, BOOL extended, BOOL down, BOOL autorep);
void JoyReset();
void JoySetButton(int number, BOOL down);
BOOL JoySetEmulationType(HWND window, DWORD newtype);
void JoySetPosition(int xvalue, int xrange, int yvalue, int yrange);
void JoyUpdatePosition(DWORD cycles);
BOOL JoyUsingMouse();

BYTE JoyReadButton(WORD, BYTE, BYTE, BYTE);
BYTE JoyReadPosition(WORD, BYTE, BYTE, BYTE);
BYTE JoyResetPosition(WORD, BYTE, BYTE, BYTE);
