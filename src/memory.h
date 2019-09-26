/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

typedef BYTE (* FIo)(WORD pc, BYTE address, BYTE write, BYTE value);

extern FIo        ioRead[0x100];
extern FIo        ioWrite[0x100];
extern LPBYTE     g_mem;
extern LPBYTE     g_memDirty;
extern LPBYTE     g_memWrite[0x100];

void   MemDestroy();
LPBYTE MemGetAuxPtr(WORD offset);
LPBYTE MemGetMainPtr(WORD offset);
void   MemInitialize();
void   MemReset();
void   MemResetPaging();
BYTE   MemReturnRandomData(BOOL highbit);

BYTE MemGetPaging(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE MemSetPaging(WORD pc, BYTE address, BYTE write, BYTE value);
