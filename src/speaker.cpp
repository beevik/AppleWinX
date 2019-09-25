/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

static SDL_AudioDeviceID audioDevice = 0;
static uint8_t           audioSilence;
static int               audioBufSize;
static BYTE *            audioBuf;
static int8_t            audioSample = 80;
static int64_t           lastToggle;

//===========================================================================
void SpkrDestroy() {
    SDL_AudioQuit();

    if (audioDevice) {
        audioDevice = 0;
        delete[] audioBuf;
    }
}

//===========================================================================
void SpkrInitialize() {
    audioDevice    = 0;
    audioSilence   = 0;
    audioBufSize   = 0;

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    SDL_AudioSpec want, have;
    ZeroMemory(&want, sizeof(want));
    want.freq     = 30994;
    want.format   = AUDIO_S8;
    want.channels = 1;
    want.samples  = 1024;
    want.callback = NULL;

    audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audioDevice == 0)
        return;

    audioSilence = have.silence;
    audioBufSize = have.size;
    audioBuf     = new BYTE[audioBufSize];
    memset(audioBuf, have.silence, audioBufSize);

    SDL_PauseAudioDevice(audioDevice, 0);
}

//===========================================================================
BYTE SpkrToggle(WORD, BYTE address, BYTE write, BYTE) {
    int cyclesSince = int(g_cyclesEmulated - lastToggle);
    int samples     = cyclesSince / 32;

    if (samples < audioBufSize && !TimerIsFullSpeed()) {
        for (int i = 0; i < samples; i++)
            audioBuf[i] = audioSample;
        SDL_QueueAudio(audioDevice, audioBuf, samples);
    }

    audioSample = -audioSample;
    lastToggle  = g_cyclesEmulated;

    return MemReturnRandomData(TRUE);
}

//===========================================================================
void SpkrUpdate(DWORD cycles) {
}
