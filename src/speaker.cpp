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
    char buffer[64];
    StrPrintf(
        buffer,
        ARRSIZE(buffer),
        "This benchmark took %u.%02u seconds.",
        (unsigned)(totaltime / 1000),
        (unsigned)((totaltime / 10) % 100)
    );
    MessageBox(
        framewindow,
        buffer,
        "Benchmark Results",
        MB_ICONINFORMATION
    );
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
}

//===========================================================================
void SpkrInitialize() {
    // DETERMINE THE DEFAULT BUFFER SIZE
    buffersize = 8192;
    bufferrate = 30994;

    // DETERMINE WHETHER A WAVEFORM OUTPUT DEVICE IS AVAILABLE
    if (soundtype == SOUND_WAVE) {
        if (waveOutGetNumDevs()) {
            _try {
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
                wavehdr[loop] = (WAVEHDR *)GlobalLock(GlobalAlloc(GMEM_MOVEABLE, sizeof(WAVEHDR)));
                ZeroMemory(wavedata[loop], buffersize);
                ZeroMemory(wavehdr[loop], sizeof(WAVEHDR));
            }
            waveprep = 0;
            SubmitWaveBuffer(0);
            bufferuse = 0;
            RegLoadValue("Calibration", "Wave Packet Size", &bufferuse);
            bufferuse = MAX(2048, MIN(buffersize, bufferuse));
            RegSaveValue("Calibration", "Wave Packet Size", bufferuse);
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
                MessageBox(
                    window,
                    "Direct emulation is not available because the "
                    "operating system you are using does not allow "
                    "direct control of the speaker.",
                    "Configuration",
                    MB_ICONEXCLAMATION
                );
                return FALSE;

            case SOUND_WAVE:
                MessageBox(
                    window,
                    "The emulator is unable to initialize a waveform "
                    "output device.  Make sure you have a sound card "
                    "and a driver installed and that windows is "
                    "correctly configured to use the driver.  Also "
                    "ensure that no other program is currently using "
                    "the device.",
                    "Configuration",
                    MB_ICONEXCLAMATION
                );
                return FALSE;
        }
    return TRUE;
}

//===========================================================================
BYTE SpkrToggle(WORD, BYTE address, BYTE write, BYTE) {
    needsprecision = totalcycles;
    if (extbench) {
        DisplayBenchmarkResults();
        extbench = 0;
    }

    if (waveout) {
        ++toggles;
        DWORD loop = (lastcyclenum / 32) + waveoffset;
        DWORD max  = (cyclenum / 32) + waveoffset;
        if (max <= loop)
            max = loop + 1;
        if (max > buffersize - 1)
            max = buffersize - 1;
        while (loop < max)
            wavedata[waveprep][loop++] = toggleval;
        lastcyclenum = cyclenum;
        toggleval = ~toggleval;
    }

    return MemReturnRandomData(TRUE);
}

//===========================================================================
void SpkrUpdate(DWORD cycles) {
    if (!waveout)
        return;

    // IF AT LEAST ONE TOGGLE WAS MADE DURING THIS CLOCK TICK, OR DURING A
    // PREVIOUS CLOCK TICK WHOSE BUFFER WE ARE SHARING, THEN SUBMIT THE BUFFER
    // TO THE WAVE DEVICE
    if (toggles || waveoffset) {
        int loop = lastcyclenum / 32;
        int max = cycles / 32;
        if (max + waveoffset > buffersize - 1)
            max = buffersize - (waveoffset + 1);
        while (loop < max)
            wavedata[waveprep][waveoffset + loop++] = toggleval;
        if (waveoffset + 2 * max >= bufferuse) {
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
