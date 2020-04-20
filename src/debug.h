/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern int64_t g_debugCounter;

void DebugDestroy();

void DebugDumpMemory(const char filename[], uint16_t memStart, uint16_t len);
void DebugFprintf(int fileno, const char format[], ...);
void DebugPrintf(const char format[], ...);
