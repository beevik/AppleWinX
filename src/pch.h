/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <string>
#include <unordered_map>
#include <vector>

#include "platform.h"

#ifdef COMPILER_MSVC
#include <windows.h>
#include <commctrl.h>
#endif

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "applewin.h"
#include "comm.h"
#include "cpu.h"
#include "debug.h"
#include "disk.h"
#include "frame.h"
#include "image.h"
#include "joystick.h"
#include "keyboard.h"
#include "memory.h"
#include "registry.h"
#include "speaker.h"
#include "strings.h"
#include "timer.h"
#include "video.h"
#include "window.h"
