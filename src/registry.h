/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

BOOL RegLoadString(const char * section, const char * key, char * buffer, DWORD chars);
BOOL RegLoadValue(const char * section, const char * key, DWORD * value);
void RegSaveString(const char * section, const char * key, const char * value);
void RegSaveValue(const char * section, const char * key, DWORD value);
