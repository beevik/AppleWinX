/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

/****************************************************************************
*
*   Windows implementation
*
***/

#ifdef OS_WINDOWS

//===========================================================================
void ResourceFree(const void * resource) {
    REF(resource);
}

//===========================================================================
const void * ResourceLoad(const char * name, const char * type, int * size) {
    *size = 0;

    HRSRC handle = FindResourceA(g_instance, name, type);
    if (!handle)
        return nullptr;

    HANDLE resource = LoadResource(NULL, handle);
    if (!resource)
        return nullptr;

    *size = (int)SizeofResource(NULL, handle);
    return LockResource(resource);
}

#endif // OS_WINDOWS
