/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

constexpr DWORD SOUND_NONE    = 0;
constexpr DWORD SOUND_DIRECT  = 1;
constexpr DWORD SOUND_SMART   = 2;
constexpr DWORD SOUND_WAVE    = 3;

constexpr int   WAVEBUFFERS   = 8;

DWORD soundtype = SOUND_WAVE;

static BYTE *    wavedata[WAVEBUFFERS];
static WAVEHDR * wavehdr[WAVEBUFFERS];

static DWORD    bufferrate   = 0;
static DWORD    buffersize   = 0;
static DWORD    bufferuse    = 0;
static DWORD    cycleshift   = 0;
static BOOL     directio     = FALSE;
static DWORD    lastcyclenum = 0;
static DWORD    lastdelta[2] = {0,0};
static DWORD    soundeffect  = 0;
static DWORD    toggles      = 0;
static BYTE     toggleval    = 0x50;
static DWORD    totaldelta   = 0;
static DWORD    quietcycles  = 0;
static DWORD    waveoffset   = 0;
static HWAVEOUT waveout      = (HWAVEOUT)0;
static int      waveprep     = 0;

//===========================================================================
static void DisplayBenchmarkResults() {
    DWORD totaltime = GetTickCount() - extbench;
    VideoRedrawScreen();
    TCHAR buffer[64];
    wsprintf(buffer,
        TEXT("This benchmark took %u.%02u seconds."),
        (unsigned)(totaltime / 1000),
        (unsigned)((totaltime / 10) % 100));
    MessageBox(framewindow,
        buffer,
        TEXT("Benchmark Results"),
        MB_ICONINFORMATION);
}

//===========================================================================
void InternalBeep(DWORD frequency, DWORD duration) {
    Beep(frequency, duration);
}

//===========================================================================
void InternalClick() {
    Beep(37, (DWORD)-1);
    Beep(0, 0);
}

//===========================================================================
static void SubmitWaveBuffer(int size) {

    // IF THIS IS WAVE BUFFER ZERO, THE OVERFLOW BUFFER, THEN IGNORE THE
    // REQUEST TO SUBMIT IT AND SKIP ON TO DETERMINING THE NEXT BUFFER TO USE
    if (waveprep) {

        // OTHERWISE, BUILD A HEADER AND SUBMIT IT
        ZeroMemory(wavehdr[waveprep], sizeof(WAVEHDR));
        wavehdr[waveprep]->lpData = (char *)wavedata[waveprep];
        wavehdr[waveprep]->dwBufferLength = size;
        wavehdr[waveprep]->dwFlags = 0;
        waveOutPrepareHeader(waveout, wavehdr[waveprep], sizeof(WAVEHDR));
        waveOutWrite(waveout, wavehdr[waveprep], sizeof(WAVEHDR));

    }

    // UNPREPARE ANY COMPLETED BUFFERS, AND FIND A FREE BUFFER TO USE NEXT
    waveprep = 0;
    int loop;
    for (loop = 0; loop < WAVEBUFFERS; loop++) {
        if ((wavehdr[loop]->dwFlags & (WHDR_DONE | WHDR_PREPARED)) ==
            (WHDR_DONE | WHDR_PREPARED)) {
            waveOutUnprepareHeader(waveout, wavehdr[loop], sizeof(WAVEHDR));
            wavehdr[loop]->dwFlags = 0;
        }
        if (!(wavehdr[loop]->dwFlags & WHDR_PREPARED))
            waveprep = loop;
    }

}

//===========================================================================
static void TestPacketSize() {
    if (!waveprep)
        return;

    // SUBMIT TWO 2048 BYTE WAVE BUFFERS.  THESE SHOULD EACH TAKE 1/16TH
    // OF A SECOND TO COMPLETE.
    int testbuffer[2];
    int loop = 2;
    while (loop--) {
        testbuffer[loop] = waveprep;
        FillMemory(wavedata[waveprep], buffersize, 0x80);
        SubmitWaveBuffer(2048);
    }

    // DETERMINE HOW MUCH TIME ELAPSES BEFORE ALL BUFFERS ARE COMPLETE.
    DWORD starttime = timeGetTime();
    DWORD currtime;
    do
        currtime = timeGetTime();
    while ((currtime - starttime < 250) &&
        !((wavehdr[testbuffer[0]]->dwFlags & WHDR_DONE) &&
        (wavehdr[testbuffer[1]]->dwFlags & WHDR_DONE)));

    // THE BUFFERS SHOULD COMPLETE IN 1/8TH SECOND.  EVEN WITH OVERHEAD,
    // THEY SHOULD COMPLETE IN NO MORE THAN 1/5TH SECOND.  SO IF THEY HAVE
    // NOT COMPLETED IN 1/4TH SECOND, THEN THE AUDIO DRIVER MUST BE IMPOSING AN
    // ARTIFICIAL MINIMUM BUFFER SIZE.  GENERALLY THE MINIMUM SIZE USED IS
    // 1/4TH SECOND, SO WE INCREASE OUR BUFFER SIZE TO 1/4TH SECOND TO
    // COMPENSATE.
    if (currtime - starttime >= 250)
        bufferuse = min(buffersize, 8192);
}


//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
DWORD SpkrCyclesSinceSound() {
    return waveout ? 1000000 : quietcycles;
}

//===========================================================================
void SpkrDestroy() {
    if (waveout) {
        waveOutReset(waveout);
        int loop;
        for (loop = 0; loop < WAVEBUFFERS; loop++)
            if (wavehdr[loop]->dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(waveout, wavehdr[loop], sizeof(WAVEHDR));
        waveOutClose(waveout);
        waveout = (HWAVEOUT)0;
        for (loop = 0; loop < WAVEBUFFERS; loop++) {
            HGLOBAL handle;
            handle = GlobalHandle(wavedata[loop]);
            GlobalUnlock(handle);
            GlobalFree(handle);
            wavedata[loop] = NULL;
            handle = GlobalHandle(wavehdr[loop]);
            GlobalUnlock(handle);
            GlobalFree(handle);
            wavehdr[loop] = NULL;
        }
    }
    else
        InternalBeep(0, 0);
}

//===========================================================================
void SpkrInitialize() {

    // DETERMINE THE DEFAULT BUFFER SIZE
    buffersize = 8192;
    bufferrate = 30994;
    cycleshift = 5;

    // DETERMINE WHETHER A WAVEFORM OUTPUT DEVICE IS AVAILABLE
    if (soundtype == SOUND_WAVE) {
        if (waveOutGetNumDevs()) {
            _try{
              WAVEFORMATEX format;
              format.wFormatTag = WAVE_FORMAT_PCM;
              format.nChannels = 1;
              format.nSamplesPerSec = bufferrate;
              format.nAvgBytesPerSec = bufferrate;
              format.nBlockAlign = 1;
              format.wBitsPerSample = 8;
              format.cbSize = sizeof(WAVEFORMATEX);
              if (waveOutOpen(&waveout,WAVE_MAPPER,&format,NULL,0,0))
                waveout = (HWAVEOUT)0;
            }
                _except(EXCEPTION_EXECUTE_HANDLER) {
                waveout = (HWAVEOUT)0;
            }
        }
        else
            waveout = (HWAVEOUT)0;
        if (waveout) {
            int loop = WAVEBUFFERS;
            while (loop--) {
                wavedata[loop] = (BYTE *)GlobalLock(GlobalAlloc(GMEM_MOVEABLE, buffersize));
                wavehdr[loop] = (WAVEHDR *)GlobalLock(GlobalAlloc(GMEM_MOVEABLE,
                    sizeof(WAVEHDR)));
                ZeroMemory(wavedata[loop], buffersize);
                ZeroMemory(wavehdr[loop], sizeof(WAVEHDR));
            }
            waveprep = 0;
            SubmitWaveBuffer(0);
            bufferuse = 0;
            RegLoadValue(TEXT("Calibration"), TEXT("Wave Packet Size"), 0, &bufferuse);
            bufferuse = MAX(2048, MIN(buffersize, bufferuse));
            RegSaveValue(TEXT("Calibration"), TEXT("Wave Packet Size"), 0, bufferuse);
            if (bufferuse == 2048)
                TestPacketSize();
        }
    }

    // IF NONE IS, THEN DETERMINE WHETHER WE HAVE DIRECT ACCESS TO THE
    // PC SPEAKER PORT
    if (!waveout) {
        if (soundtype == SOUND_WAVE)
            soundtype = SOUND_SMART;
        directio = 0;
        if (soundtype == SOUND_DIRECT)
            soundtype = SOUND_SMART;
    }

}

//===========================================================================
BOOL SpkrNeedsAccurateCycleCount() {
    return ((soundtype == SOUND_WAVE) && (toggles || waveoffset));
}

//===========================================================================
BOOL SpkrNeedsFineGrainTiming() {
    return ((soundtype == SOUND_DIRECT) ||
        ((soundtype == SOUND_SMART) && soundeffect));
}

//===========================================================================
BOOL SpkrSetEmulationType(HWND window, DWORD newtype) {
    if (soundtype != SOUND_NONE)
        SpkrDestroy();
    soundtype = newtype;
    if (soundtype != SOUND_NONE)
        SpkrInitialize();
    if (soundtype != newtype)
        switch (newtype) {

            case SOUND_DIRECT:
                MessageBox(window,
                    TEXT("Direct emulation is not available because the ")
                    TEXT("operating system you are using does not allow ")
                    TEXT("direct control of the speaker."),
                    TEXT("Configuration"),
                    MB_ICONEXCLAMATION);
                return 0;

            case SOUND_WAVE:
                MessageBox(window,
                    TEXT("The emulator is unable to initialize a waveform ")
                    TEXT("output device.  Make sure you have a sound card ")
                    TEXT("and a driver installed and that windows is ")
                    TEXT("correctly configured to use the driver.  Also ")
                    TEXT("ensure that no other program is currently using ")
                    TEXT("the device."),
                    TEXT("Configuration"),
                    MB_ICONEXCLAMATION);
                return 0;

        }
    return 1;
}

//===========================================================================
BYTE __stdcall SpkrToggle(WORD, BYTE address, BYTE write, BYTE) {
    needsprecision = cumulativecycles;
    if (extbench) {
        DisplayBenchmarkResults();
        extbench = 0;
    }
    if (waveout) {
        ++toggles;
        int loop = (lastcyclenum >> cycleshift) + waveoffset;
        int max = (cyclenum >> cycleshift) + waveoffset;
        if (max <= loop)
            max = loop + 1;
        if (max > buffersize - 1)
            max = buffersize - 1;
        while (loop < max)
            * (wavedata[waveprep] + (loop++)) = toggleval;
        lastcyclenum = cyclenum;
        toggleval = ~toggleval;
    }
    else if (soundtype != SOUND_NONE) {

        // IF WE ARE CURRENTLY PLAYING A SOUND EFFECT OR ARE IN DIRECT
        // EMULATION MODE, TOGGLE THE SPEAKER
        if ((soundeffect > 2) || (soundtype == SOUND_DIRECT))
            if (directio)
                __asm {
                push eax
                in   al, 0x61
                xor al, 2
                out  0x61, al
                pop  eax
            }
            else {
                Beep(37, (DWORD)-1);
                Beep(0, 0);
            }

        // SAVE INFORMATION ABOUT THE FREQUENCY OF SPEAKER TOGGLING FOR POSSIBLE
        // LATER USE BY SOUND AVERAGING
        if (lastcyclenum) {
            toggles++;
            DWORD delta = cyclenum - lastcyclenum;

            // DETERMINE WHETHER WE ARE PLAYING A SOUND EFFECT
            if (directio &&
                ((delta < 250) ||
                (lastdelta[0] && lastdelta[1] &&
                    (delta - lastdelta[0] > 250) && (lastdelta[0] - delta > 250) &&
                    (delta - lastdelta[1] > 250) && (lastdelta[1] - delta > 250))))
                soundeffect = MIN(35, soundeffect + 2);

            lastdelta[1] = lastdelta[0];
            lastdelta[0] = delta;
            totaldelta += delta;
        }
        lastcyclenum = cyclenum;

    }
    return MemReturnRandomData(1);
}

//===========================================================================
void SpkrUpdate(DWORD totalcycles) {
    if (waveout) {

        // IF AT LEAST ONE TOGGLE WAS MADE DURING THIS CLOCK TICK, OR DURING A
        // PREVIOUS CLOCK TICK WHOSE BUFFER WE ARE SHARING, THEN SUBMIT THE
        // BUFFER TO THE WAVE DEVICE
        if (toggles || waveoffset) {
            int loop = lastcyclenum >> cycleshift;
            int max = totalcycles >> cycleshift;
            if (max + waveoffset > buffersize - 1)
                max = buffersize - (waveoffset + 1);
            while (loop < max)
                * (wavedata[waveprep] + waveoffset + (loop++)) = toggleval;
            if (waveoffset + (max << 1) >= bufferuse) {
                SubmitWaveBuffer(waveoffset + max);
                toggles = 0;
                waveoffset = 0;
            }
            else
                waveoffset += max;
            lastcyclenum = 0;
        }

        // OTHERWISE, CHECK FOR COMPLETED BUFFERS
        else {
            waveprep = 0;
            SubmitWaveBuffer(0);
        }

    }
    else {

        // IF WE ARE NOT PLAYING A SOUND EFFECT, PERFORM FREQUENCY AVERAGING
        static DWORD currenthertz = 0;
        static BOOL  lastfull = 0;
        static DWORD lasttoggles = 0;
        static DWORD lastval = 0;
        if ((soundeffect > 2) || (soundtype == SOUND_DIRECT)) {
            lastval = 0;
            if (currenthertz && (soundeffect > 4)) {
                InternalBeep(0, 0);
                currenthertz = 0;
            }
        }
        else if (toggles && totaldelta) {
            DWORD newval = 1000000 * toggles / totaldelta;
            if (lastval && lastfull &&
                (newval - currenthertz > 50) &&
                (currenthertz - newval > 50)) {
                InternalBeep(newval, (DWORD)-1);
                currenthertz = newval;
                lasttoggles = 0;
            }
            lastfull = (totaldelta + ((totaldelta / toggles) << 1) >= totalcycles);
            lasttoggles += toggles;
            lastval = newval;
        }
        else if (currenthertz) {
            InternalBeep(0, 0);
            currenthertz = 0;
            lastfull = 0;
            lasttoggles = 0;
            lastval = 0;
        }
        else if (lastval) {
            currenthertz = (lasttoggles > 4) ? lastval : 0;
            if (currenthertz)
                InternalBeep(lastval, (DWORD)-1);
            else
                InternalClick();
            lastfull = 0;
            lasttoggles = 0;
            lastval = 0;
        }

        // RESET THE FREQUENCY GATHERING VARIABLES
        lastcyclenum = 0;
        lastdelta[0] = 0;
        lastdelta[1] = 0;
        quietcycles = toggles ? 0 : (quietcycles + totalcycles);
        toggles = 0;
        totaldelta = 0;
        if (soundeffect)
            soundeffect--;

    }
}
