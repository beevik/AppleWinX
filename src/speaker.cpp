/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

static SDL_AudioDeviceID s_audioDevice = 0;
static uint8_t           s_audioSilence;
static int               s_audioBufSize;
static uint8_t *         s_audioBuf;
static int8_t            s_audioSample = 80;
static int64_t           s_lastToggle;

//===========================================================================
void SpkrDestroy() {
    SDL_AudioQuit();

    if (s_audioDevice) {
        s_audioDevice = 0;
        delete[] s_audioBuf;
    }
}

//===========================================================================
void SpkrInitialize() {
    s_audioDevice    = 0;
    s_audioSilence   = 0;
    s_audioBufSize   = 0;
    s_lastToggle     = 0;

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    SDL_AudioSpec want, have;
    memset(&want, 0, sizeof(want));
    want.freq     = (int)CPU_HZ / 32;
    want.format   = AUDIO_S8;
    want.channels = 1;
    want.samples  = 1024;
    want.callback = NULL;

    s_audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_audioDevice == 0)
        return;

    s_audioSilence = have.silence;
    s_audioBufSize = have.size;
    s_audioBuf     = new uint8_t[s_audioBufSize];
    memset(s_audioBuf, have.silence, s_audioBufSize);

    SDL_PauseAudioDevice(s_audioDevice, 0);
}

//===========================================================================
void SpkrReset() {
    s_lastToggle = 0;
}

//===========================================================================
uint8_t SpkrToggle() {
    int cyclesSince = int(g_cpu->Cycle() - s_lastToggle);
    int samples     = int(cyclesSince / (32 * TimerGetSpeedMultiplier()));

    if (samples < s_audioBufSize && !TimerIsFullSpeed()) {
        for (int i = 0; i < samples; i++)
            s_audioBuf[i] = s_audioSample;
        SDL_QueueAudio(s_audioDevice, s_audioBuf, samples);
    }

    s_audioSample = -s_audioSample;
    s_lastToggle  = g_cpu->Cycle();

    return MemReturnRandomData(TRUE);
}
