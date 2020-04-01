/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

class Image;

void ImageDestroy();
void ImageInitialize();

Image * ImageOpen(const char * imageFilename, bool * writeProtected);
void ImageClose(Image ** image);

class Image {
public:
    virtual const char * Name() const = 0;
    virtual void ReadTrack(int quarterTrack, uint8_t * buffer, int * nibblesRead) = 0;
    virtual void WriteTrack(int quarterTrack, const uint8_t * buffer, int nibbles) = 0;
};
