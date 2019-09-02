/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

void         DiskBoot();
void         DiskDestroy();
void         DiskGetLightStatus(int * drive1, int * drive2);
const char * DiskGetName(int drive);
BOOL         DiskInitialize();
BOOL         DiskIsSpinning();
void         DiskSelect(int drive);
void         DiskUpdatePosition(DWORD cycles);

BYTE DiskControlMotor(WORD, BYTE, BYTE, BYTE);
BYTE DiskControlStepper(WORD, BYTE, BYTE, BYTE);
BYTE DiskEnable(WORD, BYTE, BYTE, BYTE);
BYTE DiskReadWrite(WORD, BYTE, BYTE, BYTE);
BYTE DiskSetLatchValue(WORD, BYTE, BYTE, BYTE);
BYTE DiskSetReadMode(WORD, BYTE, BYTE, BYTE);
BYTE DiskSetWriteMode(WORD, BYTE, BYTE, BYTE);
