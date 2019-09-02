/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

typedef void * HIMAGE;

BOOL ImageBoot(HIMAGE image);
void ImageClose(HIMAGE image);
void ImageDestroy();
void ImageInitialize();
BOOL ImageOpen(const char * imagefilename, HIMAGE * image, BOOL * writeprotected, BOOL createifnecessary);
void ImageReadTrack(HIMAGE image, int track, int quartertrack, LPBYTE trackimage, int * nibbles);
void ImageWriteTrack(HIMAGE image, int track, int quartertrack, LPBYTE trackimage, int nibbles);
