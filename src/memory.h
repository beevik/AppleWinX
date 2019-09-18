/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

constexpr int MAXIMAGES = 16;

typedef BYTE (* fio)(WORD pc, BYTE address, BYTE write, BYTE value);

extern fio        ioRead[0x100];
extern fio        ioWrite[0x100];
extern LPBYTE     mem;
extern LPBYTE     memDirty;
extern LPBYTE     memWrite[MAXIMAGES][0x100];
extern DWORD      pages;

void   MemDestroy();
LPBYTE MemGetAuxPtr(WORD offset);
LPBYTE MemGetMainPtr(WORD offset);
void   MemInitialize();
void   MemReset();
void   MemResetPaging();
BYTE   MemReturnRandomData(BOOL highbit);

BYTE MemCheckPaging(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE MemSetPaging(WORD pc, BYTE address, BYTE write, BYTE value);
