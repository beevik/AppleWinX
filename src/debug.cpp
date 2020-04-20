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

int64_t g_debugCounter;

static FILE * s_fp[10];

//===========================================================================
void DebugDestroy() {
    for (int i = 0; i < ARRSIZE(s_fp); i++) {
        if (s_fp[i]) {
            fflush(s_fp[i]);
            fclose(s_fp[i]);
            s_fp[i] = nullptr;
        }
    }
}

//===========================================================================
void DebugFprintf(int fileno, const char format[], ...) {
    if (!s_fp[fileno]) {
        char filename[16];
        StrPrintf(filename, ARRSIZE(filename), "%d.log", fileno);
        s_fp[fileno] = fopen(filename, "w");
    }

    va_list argList;
    va_start(argList, format);

    char buffer[256];
    vsnprintf(buffer, ARRSIZE(buffer), format, argList);
    fputs(buffer, s_fp[fileno]);
    va_end(argList);
}

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
