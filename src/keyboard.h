/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

void  KeybGetCapsStatus(BOOL * status);
BYTE  KeybGetKeycode();
DWORD KeybGetNumQueries();
void  KeybQueueKeypress(int virtkey, BOOL extended);
void  KeybQueueKeypressSdl(const SDL_Keysym & sym);

BYTE KeybReadData(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE KeybReadFlag(WORD pc, BYTE address, BYTE write, BYTE value);
