/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern bool g_optEnhancedDisk;

void         DiskBoot();
void         DiskDestroy();
void         DiskGetLightStatus(int * drive1, int * drive2);
const char * DiskGetName(int drive);
void         DiskInitialize();
bool         DiskIsSpinning();
void         DiskSelect(int drive);
void         DiskUpdatePosition(DWORD cycles);
