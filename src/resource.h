/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

void         ResourceFree(const void * resource);
const void * ResourceLoad(const char * name, const char * type, int * size);
