/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

struct registers {
    BYTE a;   // accumulator
    BYTE x;   // index X
    BYTE y;   // index Y
    BYTE ps;  // processor status
    WORD pc;  // program counter
    WORD sp;  // stack pointer
};

enum CpuType {
    CPU_TYPE_6502,
    CPU_TYPE_65C02,
};

extern bool      cpuKill;
extern registers regs;

int   CpuExecute(DWORD cycles, int64_t * cyclecounter);
void  CpuInitialize();
void  CpuSetupBenchmark();
void  CpuSetType(CpuType type);
int   CpuStep6502();
int   CpuStepTest();
