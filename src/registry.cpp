/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

typedef LONG (APIENTRY * regclosetype)(HKEY);
typedef LONG (APIENTRY * regcreatetype)(HKEY, const char *, DWORD, char *, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);
typedef LONG (APIENTRY * regopentype)(HKEY, const char *, DWORD, REGSAM, PHKEY);
typedef LONG (APIENTRY * regquerytype)(HKEY, const char *, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LONG (APIENTRY * regsettype)(HKEY, const char *, DWORD, DWORD, CONST BYTE *, DWORD);

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BOOL RegLoadString(
    const char *    section,
    const char *    key,
    char *          buffer,
    DWORD           chars
) {
    BOOL      usingregistry = 0;
    HINSTANCE advapiinst = (HINSTANCE)0;
    advapiinst = LoadLibrary("ADVAPI32");
    if (advapiinst) {
        regclosetype regclose = (regclosetype)GetProcAddress(advapiinst, "RegCloseKey");
        regopentype  regopen = (regopentype)GetProcAddress(advapiinst, "RegOpenKeyExA");
        regquerytype regquery = (regquerytype)GetProcAddress(advapiinst, "RegQueryValueExA");
        BOOL success = 0;
        if (regclose && regopen && regquery) {
            usingregistry = 1;
            char fullkeyname[256];
            StrPrintf(
                fullkeyname,
                ARRSIZE(fullkeyname),
                "Software\\AppleWin\\CurrentVersion\\%s",
                section
            );
            HKEY keyhandle;
            LSTATUS status = regopen(
                HKEY_CURRENT_USER,
                fullkeyname,
                0,
                KEY_READ,
                &keyhandle
            );
            if (status == 0) {
                DWORD type;
                DWORD size = chars;
                success = !regquery(keyhandle, key, 0, &type, (LPBYTE)buffer, &size) && size;
                regclose(keyhandle);
            }
        }
        FreeLibrary(advapiinst);
        if (usingregistry)
            return success;
    }
    DWORD result = GetPrivateProfileString(section,
        key,
        "",
        buffer,
        chars,
        "AppleWin.ini"
    );
    return result != 0;
}

//===========================================================================
BOOL RegLoadValue(const char * section, const char * key, DWORD * value) {
    if (!value)
        return 0;
    char buffer[32] = "";
    if (!RegLoadString(section, key, buffer, 32))
        return 0;
    buffer[31] = 0;
    *value = (DWORD)_ttoi(buffer);
    return 1;
}

//===========================================================================
void RegSaveString(const char * section, const char * key, const char * value) {
    BOOL success = 0;
    char fullkeyname[256];
    StrPrintf(
        fullkeyname,
        ARRSIZE(fullkeyname),
        "Software\\AppleWin\\CurrentVersion\\%s",
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
        (LPSECURITY_ATTRIBUTES)NULL,
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
