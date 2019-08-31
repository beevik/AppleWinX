/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

constexpr DWORD BUTTONTIME     = 5000;

constexpr int DEVICE_NONE      = 0;
constexpr int DEVICE_JOYSTICK  = 1;
constexpr int DEVICE_KEYBOARD  = 2;
constexpr int DEVICE_MOUSE     = 3;

constexpr int MODE_NONE        = 0;
constexpr int MODE_STANDARD    = 1;
constexpr int MODE_CENTERING   = 2;
constexpr int MODE_SMOOTH      = 3;

struct joyinforec {
    int device;
    int mode;
};

static const joyinforec joyinfo[5] = {
    { DEVICE_NONE,      MODE_NONE       },
    { DEVICE_JOYSTICK,  MODE_STANDARD   },
    { DEVICE_KEYBOARD,  MODE_STANDARD   },
    { DEVICE_KEYBOARD,  MODE_CENTERING  },
    { DEVICE_MOUSE,     MODE_STANDARD   },
};

static BOOL keydown[11] = {};

static const POINT keyvalue[9] = {
    {   0, 255 },
    { 127, 255 },
    { 255, 255 },
    {   0, 127 },
    { 127, 127 },
    { 255, 127 },
    {   0, 0   },
    { 127, 0   },
    { 255, 0   },
};

DWORD joytype = 1;

static DWORD buttonlatch[3] = { 0, 0, 0 };
static int   delayleft      = 0;
static BOOL  firstdelay     = FALSE;
static BOOL  joybutton[3]   = { FALSE, FALSE, FALSE };
static int   joyshrx        = 8;
static int   joyshry        = 8;
static int   joysubx        = 0;
static int   joysuby        = 0;
static BOOL  setbutton[2]   = { FALSE, FALSE };
static int   xdelay         = 0;
static int   xpos           = 127;
static int   ydelay         = 0;
static int   ypos           = 127;

//===========================================================================
static void CheckJoystick() {
    static DWORD lastcheck = 0;
    DWORD currtime = GetTickCount();
    if ((currtime - lastcheck >= 10) || joybutton[0] || joybutton[1]) {
        lastcheck = currtime;
        JOYINFO info;
        if (joyGetPos(JOYSTICKID1, &info) == JOYERR_NOERROR) {
            if ((info.wButtons & JOY_BUTTON1) && !joybutton[0])
                buttonlatch[0] = BUTTONTIME;
            if ((info.wButtons & JOY_BUTTON2) && !joybutton[1])
                buttonlatch[1] = BUTTONTIME;
            if ((info.wButtons & JOY_BUTTON3) && !joybutton[2])
                buttonlatch[2] = BUTTONTIME;
            joybutton[0] = ((info.wButtons & JOY_BUTTON1) != 0);
            joybutton[1] = ((info.wButtons & JOY_BUTTON2) != 0);
            joybutton[2] = ((info.wButtons & JOY_BUTTON3) != 0);
            xpos = (info.wXpos - joysubx) >> joyshrx;
            ypos = (info.wYpos - joysuby) >> joyshry;
        }
    }
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void JoyInitialize() {
    if (joyinfo[joytype].device == DEVICE_JOYSTICK) {
        JOYCAPS caps;
        if (joyGetDevCaps(JOYSTICKID1, &caps, sizeof(JOYCAPS)) == JOYERR_NOERROR) {
            joyshrx = 0;
            joyshry = 0;
            joysubx = (int)caps.wXmin;
            joysuby = (int)caps.wYmin;
            UINT xrange = caps.wXmax - caps.wXmin;
            UINT yrange = caps.wYmax - caps.wYmin;
            while (xrange > 256) {
                xrange >>= 1;
                ++joyshrx;
            }
            while (yrange > 256) {
                yrange >>= 1;
                ++joyshry;
            }
        }
        else
            joytype = 3;
    }
}

//===========================================================================
BOOL JoyProcessKey(int virtkey, BOOL extended, BOOL down, BOOL autorep) {
    if ((joyinfo[joytype].device != DEVICE_KEYBOARD) && (virtkey != VK_MENU))
        return 0;
    BOOL keychange = !extended;
    if (virtkey == VK_MENU) {
        keychange = 1;
        keydown[9 + (extended != 0)] = down;
    }
    else if (!extended)
        if ((virtkey >= VK_NUMPAD1) && (virtkey <= VK_NUMPAD9))
            keydown[virtkey - VK_NUMPAD1] = down;
        else
            switch (virtkey) {
                case VK_END:     keydown[0] = down;  break;
                case VK_DOWN:    keydown[1] = down;  break;
                case VK_NEXT:    keydown[2] = down;  break;
                case VK_LEFT:    keydown[3] = down;  break;
                case VK_CLEAR:   keydown[4] = down;  break;
                case VK_RIGHT:   keydown[5] = down;  break;
                case VK_HOME:    keydown[6] = down;  break;
                case VK_UP:      keydown[7] = down;  break;
                case VK_PRIOR:   keydown[8] = down;  break;
                case VK_NUMPAD0: keydown[9] = down;  break;
                case VK_INSERT:  keydown[9] = down;  break;
                case VK_DECIMAL: keydown[10] = down;  break;
                case VK_DELETE:  keydown[10] = down;  break;
                default:         keychange = 0;       break;
            }
    if (keychange)
        if ((virtkey == VK_NUMPAD0) || (virtkey == VK_INSERT)) {
            if (down)
                buttonlatch[0] = BUTTONTIME;
        }
        else if ((virtkey == VK_DECIMAL) || (virtkey == VK_DELETE)) {
            if (down)
                buttonlatch[1] = BUTTONTIME;
        }
        else if ((down && !autorep) || (joyinfo[joytype].mode == MODE_CENTERING)) {
            int keys = 0;
            int xtotal = 0;
            int ytotal = 0;
            int keynum = 0;
            while (keynum < 9) {
                if (keydown[keynum]) {
                    keys++;
                    xtotal += keyvalue[keynum].x;
                    ytotal += keyvalue[keynum].y;
                }
                keynum++;
            }
            if (keys) {
                xpos = xtotal / keys;
                ypos = ytotal / keys;
            }
            else if (joyinfo[joytype].mode == MODE_CENTERING) {
                xpos = 127;
                ypos = 127;
            }
        }
    return keychange;
}

//===========================================================================
BYTE __stdcall JoyReadButton(WORD, BYTE address, BYTE, BYTE) {
    if (joyinfo[joytype].device == DEVICE_JOYSTICK)
        CheckJoystick();
    BOOL pressed = 0;
    switch (address) {

        case 0x61:
            pressed = (buttonlatch[0] || joybutton[0] || setbutton[0] || keydown[9]);
            buttonlatch[0] = 0;
            break;

        case 0x62:
            pressed = (buttonlatch[1] || joybutton[1] || setbutton[1] || keydown[10]);
            buttonlatch[1] = 0;
            break;

        case 0x63:
            pressed = (buttonlatch[2] || joybutton[2] || !(GetKeyState(VK_SHIFT) < 0));
            buttonlatch[2] = 0;
            break;

    }
    return MemReturnRandomData(pressed);
}

//===========================================================================
BYTE __stdcall JoyReadPosition(WORD programcounter, BYTE address, BYTE, BYTE) {
    needsprecision = cumulativecycles;
    if ((*(LPDWORD)(mem + programcounter) == 0xD0C80410) &&
        (*(LPWORD)(mem + programcounter + 4) == 0x88F8))
        delayleft = 1;
    if (delayleft) {
        if (xdelay)
            --xdelay;
        if (ydelay)
            --ydelay;
        --delayleft;
    }
    return MemReturnRandomData((address & 1) ? (ydelay != 0) : (xdelay != 0));
}

//===========================================================================
void JoyReset() {
    int loop = 0;
    while (loop < 11)
        keydown[loop++] = 0;
}

//===========================================================================
BYTE __stdcall JoyResetPosition(WORD, BYTE, BYTE, BYTE) {
    needsprecision = cumulativecycles;
    if (joyinfo[joytype].device == DEVICE_JOYSTICK)
        CheckJoystick();
    xdelay = xpos;
    ydelay = ypos;
    delayleft = 8;
    firstdelay = 1;
    return MemReturnRandomData(1);
}

//===========================================================================
void JoySetButton(int number, BOOL down) {
    if (number > 1)
        return;
    setbutton[number] = down;
    if (down)
        buttonlatch[number] = BUTTONTIME;
}

//===========================================================================
BOOL JoySetEmulationType(HWND window, DWORD newtype) {
    if (joyinfo[newtype].device == DEVICE_JOYSTICK) {
        JOYCAPS caps;
        if (joyGetDevCaps(JOYSTICKID1, &caps, sizeof(JOYCAPS)) != JOYERR_NOERROR) {
            MessageBox(window,
                "The emulator is unable to read your PC joystick.  "
                "Ensure that your game port is configured properly, "
                "that the joystick is firmly plugged in, and that "
                "you have a joystick driver installed.",
                "Configuration",
                MB_ICONEXCLAMATION);
            return 0;
        }
    }
    else if ((joyinfo[newtype].device == DEVICE_MOUSE) &&
        (joyinfo[joytype].device != DEVICE_MOUSE))
        MessageBox(window,
            "To begin emulating a joystick with your mouse, move "
            "the mouse cursor over the emulated screen of a running "
            "program and click the left mouse button.  During the "
            "time the mouse is emulating a joystick, you will not "
            "be able to use it to perform mouse functions, and the "
            "mouse cursor will not be visible.  To end joystick "
            "emulation and regain the mouse cursor, press escape.",
            "Configuration",
            MB_ICONINFORMATION);
    joytype = newtype;
    JoyInitialize();
    JoyReset();
    return 1;
}

//===========================================================================
void JoySetPosition(int xvalue, int xrange, int yvalue, int yrange) {
    xpos = (xvalue * 255) / xrange;
    ypos = (yvalue * 255) / yrange;
}

//===========================================================================
void JoyUpdatePosition(DWORD cycles) {
    if (firstdelay)
        firstdelay = 0;
    else {
        xdelay = MAX(0, xdelay - delayleft);
        ydelay = MAX(0, ydelay - delayleft);
    }
    delayleft = 8;
    if (buttonlatch[0]) --buttonlatch[0];
    if (buttonlatch[1]) --buttonlatch[1];
    if (buttonlatch[2]) --buttonlatch[2];
}

//===========================================================================
BOOL JoyUsingMouse() {
    return (joyinfo[joytype].device == DEVICE_MOUSE);
}
