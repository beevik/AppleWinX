/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

using FIoSwitch = uint8_t (*)(uint8_t offset, bool write, uint8_t value);

void      MemDestroy();
uint8_t * MemGetAuxPtr();
uint8_t * MemGetMainPtr();
uint8_t * MemGetSlotRomPtr();
void      MemInitialize();
bool      MemIsPage2();
void      MemInstallPeripheralRom(int slot, const char * romResourceName, FIoSwitch switchFunc);
uint8_t   MemReadByte(uint16_t address);
uint16_t  MemReadWord(uint16_t address);
uint32_t  MemReadDword(uint16_t address);
void      MemReset();
uint8_t   MemReturnRandomData(bool setHiBit);
void      MemWriteByte(uint16_t address, uint8_t value);
void      MemWriteDword(uint16_t address, uint32_t value);

class Memory {
public:
    uint8_t  Read8(uint16_t address);
    uint16_t Read16(uint16_t address);
    void     Write8(uint16_t address, uint8_t value);
};
