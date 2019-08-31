/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

typedef LONG(APIENTRY * regclosetype)(HKEY);
typedef LONG(APIENTRY * regcreatetype)(HKEY, LPCTSTR, DWORD, LPTSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);
typedef LONG(APIENTRY * regopentype)(HKEY, LPCTSTR, DWORD, REGSAM, PHKEY);
typedef LONG(APIENTRY * regquerytype)(HKEY, LPCTSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
typedef LONG(APIENTRY * regsettype)(HKEY, LPCTSTR, DWORD, DWORD, CONST BYTE *, DWORD);

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BOOL RegLoadString(
    LPCTSTR section,
    LPCTSTR key,
    BOOL    peruser,
    LPTSTR  buffer,
    DWORD   chars
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
            TCHAR fullkeyname[256];
            StrPrintf(
                fullkeyname,
                ARRSIZE(fullkeyname),
                "Software\\AppleWin\\CurrentVersion\\%s",
                section
            );
            HKEY keyhandle;
            if (!regopen((peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
                fullkeyname,
                0,
                KEY_READ,
                &keyhandle)) {
                DWORD type;
                DWORD size = chars;
                success = (!regquery(keyhandle, key, 0, &type, (LPBYTE)buffer, &size)) &&
                    size;
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
BOOL RegLoadValue(LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD * value) {
    if (!value)
        return 0;
    TCHAR buffer[32] = "";
    if (!RegLoadString(section, key, peruser, buffer, 32))
        return 0;
    buffer[31] = 0;
    *value = (DWORD)_ttoi(buffer);
    return 1;
}

//===========================================================================
void RegSaveString(LPCTSTR section, LPCTSTR key, BOOL peruser, LPCTSTR buffer) {
    BOOL success = 0;
    TCHAR fullkeyname[256];
    StrPrintf(
        fullkeyname,
        ARRSIZE(fullkeyname),
        "Software\\AppleWin\\CurrentVersion\\%s",
        section
    );
    HKEY  keyhandle;
    DWORD disposition;
    LSTATUS status = RegCreateKeyExA((peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
        fullkeyname,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_READ | KEY_WRITE,
        (LPSECURITY_ATTRIBUTES)NULL,
        &keyhandle,
        &disposition);
    if (status == ERROR_SUCCESS) {
        RegSetValueExA(
            keyhandle,
            key,
            0,
            REG_SZ,
            (CONST BYTE *)buffer,
            StrLen(buffer) + 1
        );
        RegCloseKey(keyhandle);
    }
}

//===========================================================================
void RegSaveValue(LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value) {
    TCHAR buffer[32] = "";
    StrPrintf(buffer, ARRSIZE(buffer), "%u", value);
    RegSaveString(section, key, peruser, buffer);
}
