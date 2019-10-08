/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

void ConfigLoad();
void ConfigSave();

bool ConfigGetString(const char * key, char * buffer, int bufferSize, const char * defaultValue);
bool ConfigGetValue(const char * key, int * value, int defaultValue);
void ConfigSetString(const char * key, const char * value);
void ConfigSetValue(const char * key, int value);
bool ConfigRemoveString(const char * key);
bool ConfigRemoveValue(const char * key);
