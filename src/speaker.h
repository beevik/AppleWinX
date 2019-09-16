/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

void  SpkrDestroy();
void  SpkrInitialize();
void  SpkrUpdate(DWORD totalcycles);

BYTE SpkrToggle(WORD pc, BYTE address, BYTE write, BYTE value);
