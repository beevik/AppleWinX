/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

//===========================================================================
bool CharIsWhitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

//===========================================================================
char CharToLower(char ch) {
    return ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch;
}

//===========================================================================
char CharToUpper(char ch) {
    return ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch;
}

//===========================================================================
int StrCat(char * dst, const char * src, size_t dstChars) {
    char * dstPtr  = dst;
    char * dstTerm = dst + dstChars;
    while (dstPtr < dstTerm && *dstPtr != '\0')
        ++dstPtr;

    const char * srcPtr  = src;
    for (; dstPtr < dstTerm; srcPtr++, dstPtr++) {
        *dstPtr = *srcPtr;
        if (*srcPtr == '\0')
            break;
    }

    if (dstPtr == dstTerm && dstPtr - 1 >= dst)
        *--dstPtr = '\0';

    return int(dstPtr - dst);
}

//===========================================================================
char * StrChr(char * str, char ch) {
    char * ptr = str;
    while (*ptr != '\0') {
        if (*ptr == ch)
            return ptr;
        ptr++;
    }
    return NULL;
}

//===========================================================================
const char * StrChrConst(const char * str, char ch) {
    const char * ptr = str;
    while (*ptr != '\0') {
        if (*ptr == ch)
            return ptr;
        ptr++;
    }
    return NULL;
}

//===========================================================================
int StrCmp(const char * str1, const char * str2) {
    const char * strPtr1 = str1;
    const char * strPtr2 = str2;
    while (*strPtr1 != '\0' && *strPtr2 != '\0' && *strPtr1 == *strPtr2) {
        ++strPtr1;
        ++strPtr2;
    }

    if (*strPtr2 == '\0')
        return *strPtr1 == '\0' ? 0 : +1;
    if (*strPtr1 == '\0')
        return -1;
    return *strPtr1 > *strPtr2 ? +1 : -1;
}

//===========================================================================
int StrCmpI(const char * str1, const char * str2) {
    const char * strPtr1 = str1;
    const char * strPtr2 = str2;
    while (*strPtr1 != '\0' && *strPtr2 != '\0' && CharToLower(*strPtr1) == CharToLower(*strPtr2)) {
        ++strPtr1;
        ++strPtr2;
    }

    if (*strPtr2 == '\0')
        return *strPtr1 == '\0' ? 0 : +1;
    if (*strPtr1 == '\0')
        return -1;
    return CharToLower(*strPtr1) > CharToLower(*strPtr2) ? +1 : -1;
}

//===========================================================================
int StrCmpLen(const char * str1, const char * str2, size_t maxchars) {
    const char * strPtr1  = str1;
    const char * strTerm1 = str1 + maxchars;
    const char * strPtr2  = str2;
    while (strPtr1 < strTerm1 && *strPtr1 != '\0' && *strPtr2 != '\0' && *strPtr1 == *strPtr2) {
        ++strPtr1;
        ++strPtr2;
    }

    if (strPtr1 == strTerm1)
        return 0;
    if (*strPtr2 == '\0')
        return *strPtr1 == '\0' ? 0 : +1;
    if (*strPtr1 == '\0')
        return -1;
    return *strPtr1 > *strPtr2 ? +1 : -1;
}

//===========================================================================
int StrCmpLenI(const char * str1, const char * str2, size_t maxchars) {
    const char * strPtr1  = str1;
    const char * strTerm1 = str1 + maxchars;
    const char * strPtr2  = str2;
    while (strPtr1 < strTerm1 && *strPtr1 != '\0' && *strPtr2 != '\0' && CharToLower(*strPtr1) == CharToLower(*strPtr2)) {
        ++strPtr1;
        ++strPtr2;
    }

    if (strPtr1 == strTerm1)
        return 0;
    if (*strPtr2 == '\0')
        return *strPtr1 == '\0' ? 0 : +1;
    if (*strPtr1 == '\0')
        return -1;
    return CharToLower(*strPtr1) > CharToLower(*strPtr2) ? +1 : -1;
}

//===========================================================================
int StrCopy(char * dst, const char * src, size_t dstChars) {
    const char * srcPtr  = src;
    char *       dstPtr  = dst;
    char *       dstTerm = dst + dstChars;
    for (; dstPtr < dstTerm; srcPtr++, dstPtr++) {
        *dstPtr = *srcPtr;
        if (*srcPtr == '\0')
            break;
    }

    if (dstPtr == dstTerm && dstPtr - 1 >= dst)
        *--dstPtr = '\0';

    return int(dstPtr - dst);
}

//===========================================================================
int StrLen(const char * str) {
    const char * ptr = str;
    while (*ptr != '\0')
        ++ptr;
    return int(ptr - str);
}

//===========================================================================
int StrPrintf(char * dest, size_t dstChars, const char * format, ...) {
    va_list args;
    va_start(args, format);
    int n = vsnprintf(dest, dstChars, format, args);
    va_end(args);
    return n;
}

//===========================================================================
char * StrStr(char * str1, const char * str2) {
    if (*str2 == '\0')
        return str1;
    for (char * ptr1 = str1; *ptr1 != '\0'; ++ptr1) {
        if (*ptr1 != str2[0])
            continue;

        bool match = true;
        char *       tmp1 = ptr1 + 1;
        const char * tmp2 = str2 + 1;
        for (; *tmp1 != '\0' && *tmp2 != '\0'; tmp1++, tmp2++) {
            if (*tmp1 != *tmp2) {
                match = false;
                break;
            }
        }

        if (match && *tmp2 == '\0')
            return ptr1;
    }
    return NULL;
}

//===========================================================================
const char * StrStrConst(const char * str1, const char * str2) {
    if (*str2 == '\0')
        return str1;
    for (const char * ptr1 = str1; *ptr1 != '\0'; ++ptr1) {
        if (*ptr1 != str2[0])
            continue;

        bool match = true;
        const char * tmp1 = ptr1 + 1;
        const char * tmp2 = str2 + 1;
        for (; *tmp1 != '\0' && *tmp2 != '\0'; tmp1++, tmp2++) {
            if (*tmp1 != *tmp2) {
                match = false;
                break;
            }
        }

        if (match && *tmp2 == '\0')
            return ptr1;
    }
    return NULL;
}

//===========================================================================
char * StrTok(char * str, const char * delimiter, char ** context) {
    return strtok_s(str, delimiter, context);
}

//===========================================================================
int StrToInt(const char * str, char ** endPtr, int base) {
    return strtol(str, endPtr, base);
}

//===========================================================================
unsigned int StrToUnsigned(const char * str, char ** endPtr, int base) {
    return strtoul(str, endPtr, base);
}
