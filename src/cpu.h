/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

struct registers {
    uint8_t  a;   // accumulator
    uint8_t  x;   // index X
    uint8_t  y;   // index Y
    uint8_t  ps;  // processor status
    uint16_t pc;  // program counter
    uint16_t sp;  // stack pointer
};

enum ECpuType {
    CPU_TYPE_6502,
    CPU_TYPE_65C02,
};

extern bool      cpuKill;
extern registers regs;

int   CpuExecute(int32_t cycles, int64_t * cyclecounter);
void  CpuInitialize();
void  CpuSetupBenchmark();
void  CpuSetType(ECpuType type);
int   CpuStep6502();
int   CpuStepTest();
