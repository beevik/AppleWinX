/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

static SDL_AudioDeviceID audiodevice = 0;
static uint8_t           audiosilence;
static int               audiobufsize;
static BYTE *            audiobuf;
static int8_t            audiosample = 80;
static DWORD             lasttoggle;

//===========================================================================
void SpkrDestroy() {
    SDL_AudioQuit();

    if (audiodevice) {
        audiodevice = 0;
        delete[] audiobuf;
    }
}

//===========================================================================
void SpkrInitialize() {
    audiodevice    = 0;
    audiosilence   = 0;
    audiobufsize   = 0;

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    SDL_AudioSpec want, have;
    ZeroMemory(&want, sizeof(want));
    want.freq     = 30994;
    want.format   = AUDIO_S8;
    want.channels = 1;
    want.samples  = 1024;
    want.callback = NULL;

    audiodevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audiodevice == 0)
        return;

    audiosilence = have.silence;
    audiobufsize = have.size;
    audiobuf     = new BYTE[audiobufsize];
    memset(audiobuf, have.silence, audiobufsize);

    SDL_PauseAudioDevice(audiodevice, 0);
}

//===========================================================================
BYTE SpkrToggle(WORD, BYTE address, BYTE write, BYTE) {
    int cyclessince = int(totalcycles - lasttoggle);
    int samples     = cyclessince / 32;

    if (samples < audiobufsize && !fullspeed) {
        for (int i = 0; i < samples; i++)
            audiobuf[i] = audiosample;
        SDL_QueueAudio(audiodevice, audiobuf, samples);
    }

    audiosample = -audiosample;
    lasttoggle  = totalcycles;

    return MemReturnRandomData(TRUE);
}

//===========================================================================
void SpkrUpdate(DWORD cycles) {
}
