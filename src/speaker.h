/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern DWORD soundtype;

DWORD SpkrCyclesSinceSound();
void  SpkrDestroy();
void  SpkrInitialize();
BOOL  SpkrNeedsAccurateCycleCount();
BOOL  SpkrNeedsFineGrainTiming();
BOOL  SpkrSetEmulationType(HWND window, DWORD newtype);
void  SpkrUpdate(DWORD totalcycles);

BYTE SpkrToggle(WORD, BYTE, BYTE, BYTE);
