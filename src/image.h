/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

struct Image;

void ImageClose(Image * image);
void ImageDestroy();
void ImageInitialize();
bool ImageOpen(const char * imageFilename, Image ** image, bool * writeProtected);
void ImageReadTrack(Image * image, int track, int quarterTrack, uint8_t * trackImage, int * nibbles);
void ImageWriteTrack(Image * image, int track, int quarterTrack, uint8_t * trackImage, int nibbles);
