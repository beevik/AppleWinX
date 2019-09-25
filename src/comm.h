/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern int serialPort;

void CommDestroy();
void CommReset();
void CommSetSerialPort(HWND window, DWORD newserialport);
void CommUpdate(int64_t cycles);

BYTE CommCommand(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE CommControl(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE CommDipSw(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE CommReceive(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE CommStatus(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE CommTransmit(WORD pc, BYTE address, BYTE write, BYTE value);
