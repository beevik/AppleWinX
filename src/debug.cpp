/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#include <cstdarg>
#pragma  hdrstop

//===========================================================================
void DebugPrintf(const char format[], ...) {
    va_list argList;
    va_start(argList, format);

    char buffer[256];
    vsnprintf(buffer, ARRSIZE(buffer), format, argList);
    OutputDebugStringA(buffer);
    va_end(argList);
}

//===========================================================================
void DebugDumpMemory(const char filename[], uint16_t memStart, uint16_t len) {
    char * buf = new char[len];
    char * ptr = buf;
    for (uint16_t addr = memStart; addr < memStart + len; ++addr) {
        *ptr++ = MemReadByte(addr);
    }

    FILE * fp = fopen(filename, "wb");
    fwrite(buf, len, 1, fp);
    fclose(fp);
    DebugPrintf("Memory from $%04X to $%04X saved to %s.\n", memStart, memStart + len, filename);
}
