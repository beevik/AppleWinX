/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern bool g_optEnhancedDisk;

enum EDiskStatus {
    DISKSTATUS_OFF   = 0,
    DISKSTATUS_READ  = 1,
    DISKSTATUS_WRITE = 2,
};

void         DiskBoot();
void         DiskDestroy();
void         DiskGetStatus(EDiskStatus * statusDrive1, EDiskStatus * statusDrive2);
const char * DiskGetName(int drive);
void         DiskInitialize();
bool         DiskIsSpinning();
void         DiskSelect(int drive);
void         DiskUpdatePosition(int cyclesSinceLastUpdate);
