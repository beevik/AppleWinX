/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern HWND frameWindow;
extern BOOL autoBoot;

void FrameCreateWindow();
HDC  FrameGetDC();
void FrameRefreshStatus();
void FrameRegisterClass();
void FrameReleaseDC(HDC dc);
