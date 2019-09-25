/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

enum ECpuType {
    CPU_TYPE_6502,
    CPU_TYPE_65C02,
};

struct CpuRegisters {
    uint8_t  a;   // accumulator
    uint8_t  x;   // index X
    uint8_t  y;   // index Y
    uint8_t  ps;  // processor status
    uint16_t pc;  // program counter
    uint16_t sp;  // stack pointer
};

extern bool         cpuKill;
extern CpuRegisters regs;

void  CpuInitialize();
void  CpuSetType(ECpuType type);
int   CpuStep6502();
int   CpuStepTest();
