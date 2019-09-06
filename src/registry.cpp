/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BOOL RegLoadString(const char * section, const char * key, char * buffer, DWORD chars) {
    char fullkeyname[256];
    StrPrintf(
        fullkeyname,
        ARRSIZE(fullkeyname),
        "Software\\AppleWinX\\CurrentVersion\\%s",
        section
    );

    BOOL success = FALSE;
    HKEY keyhandle;
    LSTATUS status = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        fullkeyname,
        0,
        KEY_READ,
        &keyhandle
    );
    if (status == 0) {
        DWORD type;
        DWORD size = chars;
        status = RegQueryValueExA(keyhandle, key, 0, &type, (LPBYTE)buffer, &size);
        success = (status == 0) && (size > 0);
        RegCloseKey(keyhandle);
    }

    return success;
}

//===========================================================================
BOOL RegLoadValue(const char * section, const char * key, DWORD * value) {
    char buffer[32] = "";
    if (!RegLoadString(section, key, buffer, ARRSIZE(buffer)))
        return 0;
    buffer[31] = 0;

    char * endptr;
    *value = (DWORD)StrToUnsigned(buffer, &endptr, 10);
    return TRUE;
}

//===========================================================================
void RegSaveString(const char * section, const char * key, const char * value) {
    char fullkeyname[256];
    StrPrintf(
        fullkeyname,
        ARRSIZE(fullkeyname),
        "Software\\AppleWinX\\CurrentVersion\\%s",
        section
    );

    HKEY  keyhandle;
    DWORD disposition;
    LSTATUS status = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        fullkeyname,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_READ | KEY_WRITE,
        NULL,
        &keyhandle,
        &disposition
    );

    if (status == ERROR_SUCCESS) {
        RegSetValueExA(
            keyhandle,
            key,
            0,
            REG_SZ,
            (CONST BYTE *)value,
            StrLen(value) + 1
        );
        RegCloseKey(keyhandle);
    }
}

//===========================================================================
void RegSaveValue(const char * section, const char * key, DWORD value) {
    char buffer[32] = "";
    StrPrintf(buffer, ARRSIZE(buffer), "%u", value);
    RegSaveString(section, key, buffer);
}
