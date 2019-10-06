/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

static const BYTE keyCodes[0x65][4] = {
    { 0x00, 0x00, 0x00, 0x00 }, // 0x00 unused
    { 0x00, 0x00, 0x00, 0x00 }, // 0x01 unused
    { 0x00, 0x00, 0x00, 0x00 }, // 0x02 unused
    { 0x00, 0x00, 0x00, 0x00 }, // 0x03 unused
    { 0x61, 0x01, 0x41, 0x01 }, // 0x04 A
    { 0x62, 0x02, 0x42, 0x02 }, // 0x05 B
    { 0x63, 0x03, 0x43, 0x03 }, // 0x06 C
    { 0x64, 0x04, 0x44, 0x04 }, // 0x07 D
    { 0x65, 0x05, 0x45, 0x05 }, // 0x08 E
    { 0x66, 0x06, 0x46, 0x06 }, // 0x09 F
    { 0x67, 0x07, 0x47, 0x07 }, // 0x0a G
    { 0x68, 0x08, 0x48, 0x08 }, // 0x0b H
    { 0x69, 0x09, 0x49, 0x09 }, // 0x0c I
    { 0x6A, 0x0A, 0x4A, 0x0A }, // 0x0d J
    { 0x6B, 0x0B, 0x4B, 0x0B }, // 0x0e K
    { 0x6C, 0x0C, 0x4C, 0x0C }, // 0x0f L
    { 0x6D, 0x0D, 0x4D, 0x0D }, // 0x10 M
    { 0x6E, 0x0E, 0x4E, 0x0E }, // 0x11 N
    { 0x6F, 0x0F, 0x4F, 0x0F }, // 0x12 O
    { 0x70, 0x10, 0x50, 0x10 }, // 0x13 P
    { 0x71, 0x11, 0x51, 0x11 }, // 0x14 Q
    { 0x72, 0x12, 0x52, 0x12 }, // 0x15 R
    { 0x73, 0x13, 0x53, 0x13 }, // 0x16 S
    { 0x74, 0x14, 0x54, 0x14 }, // 0x17 T
    { 0x75, 0x15, 0x55, 0x15 }, // 0x18 U
    { 0x76, 0x16, 0x56, 0x16 }, // 0x19 V
    { 0x77, 0x17, 0x57, 0x17 }, // 0x1a W
    { 0x78, 0x18, 0x58, 0x18 }, // 0x1b X
    { 0x79, 0x19, 0x59, 0x19 }, // 0x1c Y
    { 0x7A, 0x1A, 0x5A, 0x1A }, // 0x1d Z
    { 0x31, 0x31, 0x21, 0x21 }, // 0x1e 1
    { 0x32, 0x00, 0x40, 0x00 }, // 0x1f 2
    { 0x33, 0x33, 0x23, 0x23 }, // 0x20 3
    { 0x34, 0x34, 0x24, 0x24 }, // 0x21 4
    { 0x35, 0x35, 0x25, 0x25 }, // 0x22 5
    { 0x36, 0x1E, 0x5E, 0x1E }, // 0x23 6
    { 0x37, 0x37, 0x26, 0x26 }, // 0x24 7
    { 0x38, 0x38, 0x2A, 0x2A }, // 0x25 8
    { 0x39, 0x39, 0x28, 0x28 }, // 0x26 9
    { 0x30, 0x30, 0x29, 0x29 }, // 0x27 0
    { 0x0D, 0x0D, 0x0D, 0x0D }, // 0x28 RETURN
    { 0x1B, 0x1B, 0x1B, 0x1B }, // 0x29 ESCAPE
    { 0x08, 0x7F, 0x08, 0x7F }, // 0x2a BACKSPACE
    { 0x09, 0x09, 0x09, 0x09 }, // 0x2b TAB
    { 0x20, 0x20, 0x20, 0x20 }, // 0x2c SPACE
    { 0x2D, 0x1F, 0x5F, 0x1F }, // 0x2d MINUS - _
    { 0x3D, 0x3D, 0x2B, 0x2B }, // 0x2e EQUALS = +
    { 0x5B, 0x1B, 0x7B, 0x1B }, // 0x2f LEFTBRACKET [ {
    { 0x5D, 0x1D, 0x7D, 0x1D }, // 0x30 RIGHTBRACKET ] }
    { 0x5C, 0x1C, 0x7C, 0x1C }, // 0x31 BACKSLASH \ |
    { 0x5C, 0x1C, 0x7C, 0x1C }, // 0x32 NONUS_BACKSLASH \ |
    { 0x3B, 0x3B, 0x3A, 0x3A }, // 0x33 SEMICOLON ; :
    { 0x27, 0x27, 0x22, 0x20 }, // 0x34 APOSTROPHE ' "
    { 0x60, 0x60, 0x7E, 0x7E }, // 0x35 GRAVE ` ~
    { 0x2C, 0x2C, 0x3C, 0x3C }, // 0x36 COMMA , <
    { 0x2E, 0x2E, 0x3E, 0x3E }, // 0x37 PERIOD . >
    { 0x2F, 0x2F, 0x3F, 0x3F }, // 0x38 SLASH / ?
    { 0x00, 0x00, 0x00, 0x00 }, // 0x39 CAPSLOCK
    { 0x00, 0x00, 0x00, 0x00 }, // 0x3a F1
    { 0x00, 0x00, 0x00, 0x00 }, // 0x3b F2
    { 0x00, 0x00, 0x00, 0x00 }, // 0x3c F3
    { 0x00, 0x00, 0x00, 0x00 }, // 0x3d F4
    { 0x00, 0x00, 0x00, 0x00 }, // 0x3e F5
    { 0x00, 0x00, 0x00, 0x00 }, // 0x3f F6
    { 0x00, 0x00, 0x00, 0x00 }, // 0x40 F7
    { 0x00, 0x00, 0x00, 0x00 }, // 0x41 F8
    { 0x00, 0x00, 0x00, 0x00 }, // 0x42 F9
    { 0x00, 0x00, 0x00, 0x00 }, // 0x43 F10
    { 0x00, 0x00, 0x00, 0x00 }, // 0x44 F11
    { 0x00, 0x00, 0x00, 0x00 }, // 0x45 F12
    { 0x00, 0x00, 0x00, 0x00 }, // 0x46 PRINTSCREEN
    { 0x00, 0x00, 0x00, 0x00 }, // 0x47 SCROLLOCK
    { 0x00, 0x00, 0x00, 0x00 }, // 0x48 PAUSE
    { 0x00, 0x00, 0x00, 0x00 }, // 0x49 INSERT
    { 0x00, 0x00, 0x00, 0x00 }, // 0x4a HOME
    { 0x00, 0x00, 0x00, 0x00 }, // 0x4b PGUP
    { 0x7F, 0x7F, 0x7F, 0x7F }, // 0x4c DELETE
    { 0x00, 0x00, 0x00, 0x00 }, // 0x4d END
    { 0x00, 0x00, 0x00, 0x00 }, // 0x4e PGDN
    { 0x15, 0x15, 0x15, 0x15 }, // 0x4f RIGHT
    { 0x08, 0x08, 0x08, 0x08 }, // 0x50 LEFT
    { 0x0A, 0x0A, 0x0A, 0x0A }, // 0x51 DOWN
    { 0x0B, 0x0B, 0x0B, 0x0B }, // 0x52 UP
    { 0x00, 0x00, 0x00, 0x00 }, // 0x53 NUMLOCKCLEAR
    { 0x2F, 0x2F, 0x2F, 0x2F }, // 0x54 KP_DIVIDE
    { 0x2A, 0x2A, 0x2A, 0x2A }, // 0x55 KP_MULTIPLY
    { 0x2D, 0x2D, 0x2D, 0x2D }, // 0x56 KP_SUBTRACT
    { 0x2B, 0x2B, 0x2B, 0x2B }, // 0x57 KP_ADD
    { 0x0D, 0x0D, 0x0D, 0x0D }, // 0x58 KP_ENTER
    { 0x31, 0x31, 0x31, 0x31 }, // 0x59 KP_1
    { 0x0A, 0x0A, 0x0A, 0x0A }, // 0x5a KP_2
    { 0x33, 0x33, 0x33, 0x33 }, // 0x5b KP_3
    { 0x08, 0x08, 0x08, 0x08 }, // 0x5c KP_4
    { 0x35, 0x35, 0x35, 0x35 }, // 0x5d KP_5
    { 0x15, 0x15, 0x15, 0x15 }, // 0x5e KP_6
    { 0x37, 0x37, 0x37, 0x37 }, // 0x5f KP_7
    { 0x0B, 0x0B, 0x0B, 0x0B }, // 0x60 KP_8
    { 0x39, 0x39, 0x39, 0x39 }, // 0x61 KP_9
    { 0x30, 0x30, 0x30, 0x30 }, // 0x62 KP_0
    { 0x2E, 0x2E, 0x2E, 0x2E }, // 0x63 KP_PERIOD
    { 0x60, 0x60, 0x7E, 0x7E }, // 0x64 NONUS_BACKSLASH ` ~
};

static const BYTE asciiCode[0x70][4] = {
    { 0x00, 0x00, 0x00, 0x00 }, // 0x00
    { 0x00, 0x00, 0x00, 0x00 }, // 0x01
    { 0x00, 0x00, 0x00, 0x00 }, // 0x02
    { 0x00, 0x00, 0x00, 0x00 }, // 0x03
    { 0x00, 0x00, 0x00, 0x00 }, // 0x04
    { 0x00, 0x00, 0x00, 0x00 }, // 0x05
    { 0x00, 0x00, 0x00, 0x00 }, // 0x06
    { 0x00, 0x00, 0x00, 0x00 }, // 0x07
    { 0x08, 0x7F, 0x08, 0x7F }, // 0x08 VK_BACK
    { 0x09, 0x09, 0x09, 0x09 }, // 0x09 VK_TAB
    { 0x00, 0x00, 0x00, 0x00 }, // 0x0A
    { 0x00, 0x00, 0x00, 0x00 }, // 0x0B
    { 0x00, 0x00, 0x00, 0x00 }, // 0x0C
    { 0x0D, 0x0D, 0x0D, 0x0D }, // 0x0D VK_RETURN
    { 0x00, 0x00, 0x00, 0x00 }, // 0x0E
    { 0x00, 0x00, 0x00, 0x00 }, // 0x0F
    { 0x00, 0x00, 0x00, 0x00 }, // 0x10
    { 0x00, 0x00, 0x00, 0x00 }, // 0x11
    { 0x00, 0x00, 0x00, 0x00 }, // 0x12
    { 0x00, 0x00, 0x00, 0x00 }, // 0x13
    { 0x00, 0x00, 0x00, 0x00 }, // 0x14
    { 0x00, 0x00, 0x00, 0x00 }, // 0x15
    { 0x00, 0x00, 0x00, 0x00 }, // 0x16
    { 0x00, 0x00, 0x00, 0x00 }, // 0x17
    { 0x00, 0x00, 0x00, 0x00 }, // 0x18
    { 0x00, 0x00, 0x00, 0x00 }, // 0x19
    { 0x00, 0x00, 0x00, 0x00 }, // 0x1A
    { 0x1B, 0x1B, 0x1B, 0x1B }, // 0x1B VK_ESCAPE
    { 0x00, 0x00, 0x00, 0x00 }, // 0x1C
    { 0x00, 0x00, 0x00, 0x00 }, // 0x1D
    { 0x00, 0x00, 0x00, 0x00 }, // 0x1E
    { 0x00, 0x00, 0x00, 0x00 }, // 0x1F
    { 0x20, 0x20, 0x20, 0x20 }, // 0x20 VK_SPACE
    { 0x00, 0x00, 0x00, 0x00 }, // 0x21
    { 0x00, 0x00, 0x00, 0x00 }, // 0x22
    { 0x00, 0x00, 0x00, 0x00 }, // 0x23
    { 0x00, 0x00, 0x00, 0x00 }, // 0x24
    { 0x08, 0x08, 0x08, 0x08 }, // 0x25 VK_LEFT
    { 0x0B, 0x0B, 0x0B, 0x0B }, // 0x26 VK_UP
    { 0x15, 0x15, 0x15, 0x15 }, // 0x27 VK_RIGHT
    { 0x0A, 0x0A, 0x0A, 0x0A }, // 0x28 VK_DOWN
    { 0x00, 0x00, 0x00, 0x00 }, // 0x29
    { 0x00, 0x00, 0x00, 0x00 }, // 0x2A
    { 0x00, 0x00, 0x00, 0x00 }, // 0x2B
    { 0x00, 0x00, 0x00, 0x00 }, // 0x2C
    { 0x00, 0x00, 0x00, 0x00 }, // 0x2D
    { 0x7F, 0x7F, 0x7F, 0x7F }, // 0x2E VK_DELETE
    { 0x00, 0x00, 0x00, 0x00 }, // 0x2F
    { 0x30, 0x30, 0x29, 0x29 }, // 0x30 VK_0
    { 0x31, 0x31, 0x21, 0x21 }, // 0x31 VK_1
    { 0x32, 0x00, 0x40, 0x00 }, // 0x32 VK_2
    { 0x33, 0x33, 0x23, 0x23 }, // 0x33 VK_3
    { 0x34, 0x34, 0x24, 0x24 }, // 0x34 VK_4
    { 0x35, 0x35, 0x25, 0x25 }, // 0x35 VK_5
    { 0x36, 0x1E, 0x5E, 0x1E }, // 0x36 VK_6
    { 0x37, 0x37, 0x26, 0x26 }, // 0x37 VK_7
    { 0x38, 0x38, 0x2A, 0x2A }, // 0x38 VK_8
    { 0x39, 0x39, 0x28, 0x28 }, // 0x39 VK_9
    { 0x3B, 0x3B, 0x3A, 0x3A }, // 0x3A ; :
    { 0x3D, 0x3D, 0x2B, 0x2B }, // 0x3B = +
    { 0x2C, 0x2C, 0x3C, 0x3C }, // 0x3C , <
    { 0x2D, 0x1F, 0x5F, 0x1F }, // 0x3D - _
    { 0x2E, 0x2E, 0x3E, 0x3E }, // 0x3E . >
    { 0x2F, 0x2F, 0x3F, 0x3F }, // 0x3F / ?
    { 0x60, 0x60, 0x7E, 0x7E }, // 0x40 ` ~
    { 0x61, 0x01, 0x41, 0x01 }, // 0x41 VK_A
    { 0x62, 0x02, 0x42, 0x02 }, // 0x42 VK_B
    { 0x63, 0x03, 0x43, 0x03 }, // 0x43 VK_C
    { 0x64, 0x04, 0x44, 0x04 }, // 0x44 VK_D
    { 0x65, 0x05, 0x45, 0x05 }, // 0x45 VK_E
    { 0x66, 0x06, 0x46, 0x06 }, // 0x46 VK_F
    { 0x67, 0x07, 0x47, 0x07 }, // 0x47 VK_G
    { 0x68, 0x08, 0x48, 0x08 }, // 0x48 VK_H
    { 0x69, 0x09, 0x49, 0x09 }, // 0x49 VK_I
    { 0x6A, 0x0A, 0x4A, 0x0A }, // 0x4A VK_J
    { 0x6B, 0x0B, 0x4B, 0x0B }, // 0x4B VK_K
    { 0x6C, 0x0C, 0x4C, 0x0C }, // 0x4C VK_L
    { 0x6D, 0x0D, 0x4D, 0x0D }, // 0x4D VK_M
    { 0x6E, 0x0E, 0x4E, 0x0E }, // 0x4E VK_N
    { 0x6F, 0x0F, 0x4F, 0x0F }, // 0x4F VK_O
    { 0x70, 0x10, 0x50, 0x10 }, // 0x50 VK_P
    { 0x71, 0x11, 0x51, 0x11 }, // 0x51 VK_Q
    { 0x72, 0x12, 0x52, 0x12 }, // 0x52 VK_R
    { 0x73, 0x13, 0x53, 0x13 }, // 0x53 VK_S
    { 0x74, 0x14, 0x54, 0x14 }, // 0x54 VK_T
    { 0x75, 0x15, 0x55, 0x15 }, // 0x55 VK_U
    { 0x76, 0x16, 0x56, 0x16 }, // 0x56 VK_V
    { 0x77, 0x17, 0x57, 0x17 }, // 0x57 VK_W
    { 0x78, 0x18, 0x58, 0x18 }, // 0x58 VK_X
    { 0x79, 0x19, 0x59, 0x19 }, // 0x59 VK_Y
    { 0x7A, 0x1A, 0x5A, 0x1A }, // 0x5A VK_Z
    { 0x5B, 0x1B, 0x7B, 0x1B }, // 0x5B [ {
    { 0x5C, 0x1C, 0x7C, 0x1C }, // 0x5C \ |
    { 0x5D, 0x1D, 0x7D, 0x1D }, // 0x5D ] }
    { 0x27, 0x27, 0x22, 0x20 }, // 0x5E ' "
    { 0x00, 0x00, 0x00, 0x00 }, // 0x5F
    { 0x30, 0x30, 0x30, 0x30 }, // 0x60 VK_NUMPAD0
    { 0x31, 0x31, 0x31, 0x31 }, // 0x61 VK_NUMPAD1
    { 0x32, 0x32, 0x32, 0x32 }, // 0x62 VK_NUMPAD2
    { 0x33, 0x33, 0x33, 0x33 }, // 0x63 VK_NUMPAD3
    { 0x34, 0x34, 0x34, 0x34 }, // 0x64 VK_NUMPAD4
    { 0x35, 0x35, 0x35, 0x35 }, // 0x65 VK_NUMPAD5
    { 0x36, 0x36, 0x36, 0x36 }, // 0x66 VK_NUMPAD6
    { 0x37, 0x37, 0x37, 0x37 }, // 0x67 VK_NUMPAD7
    { 0x38, 0x38, 0x38, 0x38 }, // 0x68 VK_NUMPAD8
    { 0x39, 0x39, 0x39, 0x39 }, // 0x69 VK_NUMPAD9
    { 0x2A, 0x2A, 0x2A, 0x2A }, // 0x6A VK_MULTIPLY
    { 0x2B, 0x2B, 0x2B, 0x2B }, // 0x6B VK_ADD
    { 0x00, 0x00, 0x00, 0x00 }, // 0x6C
    { 0x2D, 0x2D, 0x2D, 0x2D }, // 0x6D VK_SUBTRACT
    { 0x2E, 0x2E, 0x2E, 0x2E }, // 0x6E VK_DECIMAL
    { 0x2F, 0x2F, 0x2F, 0x2F }, // 0x6F VK_DIVIDE
};

static BOOL  capsLock        = TRUE;
static DWORD keyboardQueries = 0;
static BYTE  keyCode         = 0;
static BOOL  keyWaiting      = FALSE;
static int   lastScanCode    = 0;


//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void KeybGetCapsStatus(BOOL * status) {
    *status = capsLock;
}

//===========================================================================
BYTE KeybGetKeycode() {
    return keyCode;
}

//===========================================================================
DWORD KeybGetNumQueries() {
    DWORD result = keyboardQueries;
    keyboardQueries = 0;
    return result;
}

//===========================================================================
BYTE KeybReadData(WORD pc, BYTE address, BYTE write, BYTE value) {
    keyboardQueries++;
    return keyCode | (keyWaiting ? 0x80 : 0);
}

//===========================================================================
BYTE KeybReadFlag(WORD pc, BYTE address, BYTE write, BYTE value) {
    keyboardQueries++;
    keyWaiting = 0;
    int keys;
    const uint8_t * state = SDL_GetKeyboardState(&keys);
    if (lastScanCode < keys && state[lastScanCode])
        return keyCode | 0x80;
    else
        return keyCode;
}

//===========================================================================
bool KeybIsKeyDown(SDL_Scancode scancode) {
    int keys;
    const uint8_t * state = SDL_GetKeyboardState(&keys);
    return state[scancode] != 0;
}

//===========================================================================
void KeybQueueKeypress(int virtkey, BOOL extended) {
    if (virtkey == VK_CAPITAL && EmulatorGetAppleType() == APPLE_TYPE_IIE) {
        capsLock = (GetKeyState(VK_CAPITAL) & 1);
        FrameRefreshStatus();
    }
    if (((virtkey == VK_CANCEL) || (virtkey == VK_F12)) &&
        ((EmulatorGetAppleType() != APPLE_TYPE_IIE) || (GetKeyState(VK_CONTROL) < 0))) {
        if (EmulatorGetAppleType() == APPLE_TYPE_IIE)
            MemReset();
        else
            regs.pc = *(uint16_t *)(g_pageRead[0xff] + 0xfc);
    }
    if ((virtkey & 0x7F) > 0x6F)
        return;
    int ctrl = (GetKeyState(VK_CONTROL) < 0);
    int shift = (GetKeyState(VK_SHIFT) < 0);
    if ((virtkey >= 'A') && (virtkey <= 'Z') && (capsLock || EmulatorGetAppleType() != APPLE_TYPE_IIE))
        shift = 1;
    keyCode = asciiCode[virtkey & 0x7F][ctrl | (shift << 1)];
    keyWaiting = (keyCode != 0);
}

//===========================================================================
void KeybQueueKeypressSdl(const SDL_Keysym & sym) {
    // Use system's actual capsLock state to update emulator capsLock state.
    if (sym.scancode == SDL_SCANCODE_CAPSLOCK && EmulatorGetAppleType() == APPLE_TYPE_IIE) {
        capsLock =  (SDL_GetModState() & KMOD_CAPS) ? TRUE : FALSE;
        FrameRefreshStatus();
    }

    // ctrl-shift-backspace is the same as ctrl-openapple-reset.
    if (sym.scancode == SDL_SCANCODE_BACKSPACE && (sym.mod & KMOD_CTRL) && (sym.mod & KMOD_SHIFT)) {
        if (EmulatorGetAppleType() == APPLE_TYPE_IIE)
            MemReset();
        else
            regs.pc = *(uint16_t *)(g_pageRead[0xff] + 0xfc);
    }

    if (sym.scancode > 0x64)
        return;

    int scancode = sym.scancode;
    int ctrl  = (sym.mod & KMOD_CTRL) ? 1 : 0;
    int shift = (sym.mod & KMOD_SHIFT) ? 1 : 0;
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z && (capsLock || EmulatorGetAppleType() != APPLE_TYPE_IIE))
        shift = 1;
    if (scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_0) {
        if (SDL_GetModState() & KMOD_NUM)
            scancode = scancode - SDL_SCANCODE_KP_1 + SDL_SCANCODE_1;
    }
    lastScanCode = scancode;

    keyCode = keyCodes[scancode][ctrl | shift << 1];
    keyWaiting = (keyCode != 0);
}
