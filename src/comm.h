/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern DWORD serialport;

void CommDestroy();
void CommReset();
void CommSetSerialPort(HWND window, DWORD newserialport);
void CommUpdate(DWORD totalcycles);

BYTE CommCommand(WORD, BYTE, BYTE, BYTE);
BYTE CommControl(WORD, BYTE, BYTE, BYTE);
BYTE CommDipSw(WORD, BYTE, BYTE, BYTE);
BYTE CommReceive(WORD, BYTE, BYTE, BYTE);
BYTE CommStatus(WORD, BYTE, BYTE, BYTE);
BYTE CommTransmit(WORD, BYTE, BYTE, BYTE);
