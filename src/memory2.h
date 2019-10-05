/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

extern uint8_t * g_pageRead[0x100];
extern uint8_t * g_pageWrite[0x100];

void    MemDestroy2();
void    MemInitialize2();
uint8_t MemIoRead(uint16_t address);
void    MemIoWrite(uint16_t address, uint8_t value);
void    MemReset2();
