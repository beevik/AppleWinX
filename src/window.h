/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

void WindowDestroy();
BOOL WindowInitialize();
uint32_t * WindowLockPixels();
void WindowUnlockPixels();
void WindowUpdate();
