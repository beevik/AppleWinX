/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

bool CharIsWhitespace(char ch);
char CharToLower(char ch);
char CharToUpper(char ch);

int          StrCat(char * dst, const char * src, size_t dstChars);
char *       StrChr(char * str, char ch);
const char * StrChrConst(const char * str, char ch);
int          StrCmp(const char * str1, const char * str2);
int          StrCmpI(const char * str1, const char * str2);
int          StrCmpLen(const char * str1, const char * str2, size_t maxchars);
int          StrCmpLenI(const char * str1, const char * str2, size_t maxchars);
int          StrCopy(char * dst, const char * src, size_t dstChars);
int          StrLen(const char * str);
int          StrPrintf(char * dest, size_t dstChars, const char * format, ...);
char *       StrStr(char * str1, const char * str2);
const char * StrStrConst(const char * str1, const char * str2);
char *       StrTok(char * str, const char * delimiter, char ** context);
int          StrToInt(const char * str, char ** endPtr, int base);
unsigned int StrToUnsigned(const char * str, char ** endPtr, int base);
