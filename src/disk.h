/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern BOOL optEnhancedDisk;

void         DiskBoot();
void         DiskDestroy();
void         DiskGetLightStatus(int * drive1, int * drive2);
const char * DiskGetName(int drive);
BOOL         DiskInitialize();
BOOL         DiskIsFullSpeedEligible();
BOOL         DiskIsSpinning();
void         DiskSelect(int drive);
void         DiskUpdatePosition(DWORD cycles);

BYTE DiskControlMotor(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE DiskControlStepper(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE DiskEnable(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE DiskReadWrite(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE DiskSetLatchValue(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE DiskSetReadMode(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE DiskSetWriteMode(WORD pc, BYTE address, BYTE write, BYTE value);
