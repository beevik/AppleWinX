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
BOOL ImageOpen(const char * imageFilename, HIMAGE * image, BOOL * writeProtected, BOOL createIfNecessary);
void ImageReadTrack(HIMAGE image, int track, int quarterTrack, LPBYTE trackImage, int * nibbles);
void ImageWriteTrack(HIMAGE image, int track, int quarterTrack, LPBYTE trackImage, int nibbles);
