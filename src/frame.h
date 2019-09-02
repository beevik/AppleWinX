/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

constexpr int VIEWPORTX = 5;
constexpr int VIEWPORTY = 5;

extern HWND framewindow;

void FrameCreateWindow();
HDC  FrameGetDC();
void FrameRefreshStatus();
void FrameRegisterClass();
void FrameReleaseDC(HDC dc);
