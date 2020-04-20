/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

bool    KeybGetCapsStatus();
uint8_t KeybGetKeycode();
bool    KeybIsKeyDown(SDL_Scancode scanCode);
void    KeybQueueKeypress(int virtkey, bool extended);
void    KeybQueueKeypressSdl(SDL_Scancode scanCode, uint16_t mod);

uint8_t KeybReadData();
uint8_t KeybReadFlag();
