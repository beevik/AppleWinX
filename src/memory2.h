/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

using FIoSwitch = uint8_t (*)(uint8_t offset, bool write, uint8_t value);

extern uint8_t * g_pageRead[0x100];
extern uint8_t * g_pageWrite[0x100];

void     MemDestroy2();
void     MemInitialize2();
void     MemInstallPeripheralRom(int slot, const char * romResourceName, FIoSwitch switchFunc);
uint8_t  MemIoRead(uint16_t address);
void     MemIoWrite(uint16_t address, uint8_t value);
void     MemReset2();
uint8_t  MemReadByte(uint16_t address);
uint16_t MemReadWord(uint16_t address);
uint32_t MemReadDword(uint16_t address);
void     MemWriteByte(uint16_t address, uint8_t value);
void     MemWriteWord(uint16_t address, uint16_t value);
void     MemWriteDword(uint16_t address, uint32_t value);
