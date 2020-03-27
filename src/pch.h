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
#include <climits>

#include <string>
#include <map>
#include <vector>

#include "platform.h"

#ifdef COMPILER_MSVC
#include <windows.h>
#include <commctrl.h>
#endif

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "emulator.h"
#include "comm.h"
#include "config.h"
#include "cpu.h"
#include "disk.h"
#include "frame.h"
#include "image.h"
#include "joystick.h"
#include "keyboard.h"
#include "memory.h"
#include "resource.h"
#include "scheduler.h"
#include "speaker.h"
#include "strings.h"
#include "timer.h"
#include "video.h"
#include "window.h"
