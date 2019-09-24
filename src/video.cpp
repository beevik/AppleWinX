/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

#define PNG_SETJMP_NOT_SUPPORTED
#include <png.h>

constexpr int32_t CYCLES_PER_SCANLINE = 65;
constexpr int32_t NTSC_SCANLINES      = 262;
constexpr int32_t DISPLAY_LINES       = 192;

constexpr int SCREEN_CX     = 560;
constexpr int SCREEN_CY     = 384;

constexpr int SRCX_40COL    = 0;
constexpr int SRCX_80COL    = 256;
constexpr int SRCX_LORES    = 384;
constexpr int SRCX_HIRES    = 400;
constexpr int SRCX_DHIRES   = 528;
constexpr int SRCX_UNUSED   = 536;
constexpr int SRC_CX        = 544;
constexpr int SRC_CY        = 512;

constexpr DWORD LOADPNG_CONVERT_RGB = 1 << 0;
constexpr DWORD LOADPNG_BOTTOM_UP   = 1 << 1;

constexpr DWORD VF_80COL    = 0x00000001;
constexpr DWORD VF_DHIRES   = 0x00000002;
constexpr DWORD VF_HIRES    = 0x00000004;
constexpr DWORD VF_MASK2    = 0x00000008;
constexpr DWORD VF_MIXED    = 0x00000010;
constexpr DWORD VF_PAGE2    = 0x00000020;
constexpr DWORD VF_TEXT     = 0x00000040;

#define  SW_80COL()     (videoMode & VF_80COL)
#define  SW_DHIRES()    (videoMode & VF_DHIRES)
#define  SW_HIRES()     (videoMode & VF_HIRES)
#define  SW_MASK2()     (videoMode & VF_MASK2)
#define  SW_MIXED()     (videoMode & VF_MIXED)
#define  SW_PAGE2()     (videoMode & VF_PAGE2)
#define  SW_TEXT()      (videoMode & VF_TEXT)

typedef void (* fupdate)(int x, int y, int xpixel, int ypixel, int offset);

BOOL optMonochrome = FALSE;

static HBITMAP       deviceBitmap;
static HDC           deviceDC;
static uint32_t *    frameBuffer;
static LPBYTE        hiResAuxPtr;
static LPBYTE        hiResMainPtr;
static uint32_t *    sourceLookup;
static LPBYTE        textAuxPtr;
static LPBYTE        textMainPtr;
static uint32_t *    screenPixels;
static fupdate       updateLower;
static fupdate       updateUpper;

static DWORD   charOffset     = 0;
static BOOL    displayPage2   = FALSE;
static HDC     frameDC        = (HDC)0;
static DWORD   modeSwitches   = 0;
static BOOL    redrawFull     = TRUE;
static HFONT   videoFont      = (HFONT)0;
static DWORD   videoMode      = 0;

static const COLORREF colorLores[16] = {
    0x000000, 0x7C0B93, 0xD3351F, 0xFF36BB, // black, red, dkblue, purple
    0x0C7600, 0x7E7E7E, 0xE0A807, 0xFFAC9D, // dkgreen, grey, medblue, ltblue
    0x004C62, 0x1D56F9, 0x7E7E7E, 0xEC81FF, // brown, orange, grey, pink
    0x00C843, 0x16CDDC, 0x84F75D, 0xFFFFFF, // ltgreen, yellow, aqua, white
};

static const COLORREF colorHires[6] = {
    0xFF36BB, 0xE0A807, 0x00C843,   // purple, blue, green
    0x1D56F9, 0x000000, 0xFFFFFF,   // orange, black, white
};

struct pngReadState {
    const uint8_t * data;
    size_t          bytesTotal;
    size_t          bytesRead;
};

//===========================================================================
static void BitBltCell(
    int dstx,
    int dsty,
    int xsize,
    int ysize,
    int srcx,
    int srcy
) {
    const uint32_t * src  = sourceLookup + SRC_CX * srcy + srcx;
    uint32_t *       dst1 = frameBuffer + SCREEN_CX * (SCREEN_CY - 1 - dsty) + dstx;
    uint32_t *       dst2 = screenPixels + SCREEN_CX * dsty + dstx;
    for (int y = 0; y < ysize; ++y) {
        for (int x = xsize - 1; x >= 0; x--) {
            dst1[x] = src[x];
            dst2[x] = src[x];
        }
        dst1 -= SCREEN_CX;
        dst2 += SCREEN_CX;
        src += SRC_CX;
    }
}

//===========================================================================
static void DrawDHiResSource(HDC dc) {
    for (int value = 0; value < 256; value++) {
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 2; y++) {
                int color = (x < 4) ? (value & 0xF) : (value >> 4);
                SetPixel(dc, SRCX_DHIRES + x, (value << 1) + y, colorLores[color]);
            }
        }
    }
}

//===========================================================================
static void DrawLoResSource(HDC dc) {
    for (int color = 0; color < 16; color++) {
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++)
                SetPixelV(dc, SRCX_LORES + x, (color << 4) + y, colorLores[color]);
        }
    }
}

//===========================================================================
static void DrawHiResSource(HDC dc) {
    for (int column = 0; column < 4; ++column) {
        BOOL bit[9];
        bit[0] = (column & 2) != 0;
        bit[8] = (column & 1) != 0;

        const int coloffs = column << 5;
        for (int byteval = 0; byteval < 256; ++byteval) {
            for (int pixel = 1, mask = 1; pixel < 8; ++pixel, mask <<= 1)
                bit[pixel] = ((byteval & mask) != 0);
            const int hibit = (byteval & 0x80) ? 1 : 0;
            int x = 0;
            int y = byteval << 1;
            while (x < 28) {
                const int adj = (x >= 14 ? 2 : 0);
                const int odd = (x >= 14 ? 1 : 0);
                for (int pixel = 1; pixel < 8; pixel++, x += 2) {
                    int color = 4;
                    if (bit[pixel]) {
                        if (bit[pixel - 1] || bit[pixel + 1])
                            color = 5;
                        else
                            color = ((odd ^ !(pixel & 1)) << 1) | hibit;
                    }
                    else if (bit[pixel - 1] && bit[pixel + 1]) {
                        color = ((odd ^ (pixel & 1)) << 1) | hibit;
                    }

                    SetPixelV(dc, SRCX_HIRES + coloffs + x + adj + 0, y + 0, colorHires[color]);
                    SetPixelV(dc, SRCX_HIRES + coloffs + x + adj + 1, y + 0, colorHires[color]);
                    SetPixelV(dc, SRCX_HIRES + coloffs + x + adj + 0, y + 1, colorHires[color]);
                    SetPixelV(dc, SRCX_HIRES + coloffs + x + adj + 1, y + 1, colorHires[color]);
                }
            }
        }
    }
}

//===========================================================================
static void DrawMonoDHiResSource(HDC dc) {
    for (int value = 0; value < 256; value++) {
        int val = value;
        for (int x = 0; x < 8; x++) {
            COLORREF color = (val & 1) ? 0x00FF00 : 0;
            val >>= 1;
            SetPixel(dc, SRCX_DHIRES + x, (value << 1) + 1, color);
        }
    }
}

//===========================================================================
static void DrawMonoHiResSource(HDC dc) {
    int column = 0;
    do {
        int y = 0;
        do {
            int x = 0;
            unsigned val = (y >> 1);
            do {
                COLORREF colorval = (val & 1) ? 0x00FF00 : 0;
                val >>= 1;
                SetPixelV(dc, SRCX_HIRES + column + x, y + 1, colorval);
                SetPixelV(dc, SRCX_HIRES + column + x + 1, y + 1, colorval);
            } while ((x += 2) < 16);
        } while ((y += 2) < SRC_CY);
    } while ((column += 16) < 128);
}

//===========================================================================
static void DrawMonoTextSource(HDC dc) {
    HDC     memdc = CreateCompatibleDC(dc);
    HBITMAP bitmap = LoadBitmap(instance, "CHARSET40");
    SelectObject(memdc, bitmap);
    BitBlt(
        dc,
        SRCX_40COL, 0,
        SRCX_80COL - SRCX_40COL, SRC_CY,
        memdc,
        0, 0,
        MERGECOPY
    );
    StretchBlt(
        dc,
        SRCX_80COL, 0,
        SRCX_LORES - SRCX_80COL, SRC_CY,
        memdc,
        0, 0,
        256, 512,
        MERGECOPY
    );
    DeleteDC(memdc);
    DeleteObject(bitmap);
}

//===========================================================================
static void DrawTextSource(HDC dc) {
    HDC     memdc = CreateCompatibleDC(dc);
    HBITMAP bitmap = LoadBitmap(instance, "CHARSET40");
    SelectObject(memdc, bitmap);
    BitBlt(
        dc,
        SRCX_40COL, 0,
        SRCX_80COL - SRCX_40COL, SRC_CY,
        memdc,
        0, 0,
        SRCCOPY
    );
    StretchBlt(
        dc,
        SRCX_80COL, 0,
        SRCX_LORES - SRCX_80COL, SRC_CY,
        memdc,
        0, 0,
        256, 512,
        SRCCOPY
    );
    DeleteDC(memdc);
    DeleteObject(bitmap);
}

//===========================================================================
static void LoadImageData(png_structp read, png_bytep buf, size_t bytes) {
    pngReadState * state = (pngReadState *)png_get_io_ptr(read);
    memcpy(buf, state->data + state->bytesRead, bytes);
    state->bytesRead += bytes;
}

//===========================================================================
static BOOL LoadPngImage(
    LPCSTR  name,
    DWORD   width,
    DWORD   height,
    LPVOID  buf,
    DWORD   flags
) {
    HRSRC handle = FindResourceA(instance, name, "IMAGE");
    if (!handle)
        return FALSE;

    HGLOBAL resource = LoadResource(NULL, handle);
    if (!resource)
        return FALSE;

    pngReadState state;
    state.data       = (const uint8_t *)LockResource(resource);
    state.bytesTotal = SizeofResource(NULL, handle);
    state.bytesRead  = 0;

    png_structp read = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(read);
    png_set_read_fn(read, &state, LoadImageData);
    png_read_info(read, info);

    png_uint_32 iwidth, iheight;
    int bitDepth, colorType, interlaceType;
    png_get_IHDR(
        read,
        info,
        &iwidth,
        &iheight,
        &bitDepth,
        &colorType,
        &interlaceType,
        NULL,
        NULL
    );

    if (iwidth != width || iheight != height || bitDepth != 8) {
        png_destroy_read_struct(&read, &info, NULL);
        return FALSE;
    }

    if (colorType == PNG_COLOR_TYPE_RGB)
        png_set_add_alpha(read, 0xff, PNG_FILLER_AFTER);
    if (flags & LOADPNG_CONVERT_RGB)
        png_set_bgr(read);

    LPBYTE * rows = new LPBYTE[height];
    if (flags & LOADPNG_BOTTOM_UP) {
        for (DWORD y = 0; y < height; y++)
            rows[y] = (png_bytep)((LPDWORD)buf + (height - y - 1) * width);
    }
    else {
        for (DWORD y = 0, offset = 0; y < height; y++, offset += width)
            rows[y] = (png_bytep)((LPDWORD)buf + offset);
    }

    png_read_image(read, rows);
    png_read_end(read, info);
    delete[] rows;

    png_destroy_read_struct(&read, &info, NULL);
    return TRUE;
}

//===========================================================================
static BOOL LoadSourceLookup() {
    if (!sourceLookup)
        return FALSE;

    return LoadPngImage(
        optMonochrome ? "SOURCE_MONO" : "SOURCE_COLOR",
        SRC_CX,
        SRC_CY,
        sourceLookup,
        0
    );
}

//===========================================================================
static void Update40ColCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE ch = textMainPtr[offset];

    // Handle flashing text (1.87Hz blink rate)
    constexpr DWORD BLINK_PERIOD = (DWORD)(CPU_CYCLES_PER_MS * 1000.0 / 1.87);
    if (ch >= 64 && ch < 96) {
        if (g_cyclesEmulated % BLINK_PERIOD > BLINK_PERIOD / 2)
            ch += 128;
    }
    else if (ch >= 96 && ch < 128) {
        if (g_cyclesEmulated % BLINK_PERIOD > BLINK_PERIOD / 2)
            ch += 64;
        else
            ch -= 64;
    }

    BitBltCell(
        xpixel, ypixel,
        14, 16,
        SRCX_40COL + ((ch & 0x0F) << 4),
        (ch & 0xF0) + charOffset
    );
}

//===========================================================================
static void Update80ColCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE auxval  = textAuxPtr[offset];
    BYTE mainval = textMainPtr[offset];
    BitBltCell(
        xpixel, ypixel,
        7, 16,
        SRCX_80COL + ((auxval & 15) << 3),
        ((auxval >> 4) << 4) + charOffset
    );
    BitBltCell(
        xpixel + 7, ypixel,
        7, 16,
        SRCX_80COL + ((mainval & 15) << 3),
        ((mainval >> 4) << 4) + charOffset
    );
}

//===========================================================================
static void UpdateDHiResCell(int x, int y, int xpixel, int ypixel, int offset) {
    for (int yoffset = 0; yoffset < 0x2000; yoffset += 0x400) {
        BYTE auxval  = hiResAuxPtr[offset + yoffset];
        BYTE mainval = hiResMainPtr[offset + yoffset];
        BOOL draw    = true;
        if (offset & 1) {
            BYTE thirdval = hiResMainPtr[offset + yoffset - 1];
            int value1 = ((auxval & 0x3F) << 2) | ((thirdval & 0x60) >> 5);
            int value2 = ((mainval & 0x7F) << 1) | ((auxval & 0x40) >> 6);
            BitBltCell(
                xpixel - 2, ypixel + (yoffset >> 9),
                8, 2,
                SRCX_DHIRES,
                value1 << 1
            );
            BitBltCell(
                xpixel + 6, ypixel + (yoffset >> 9),
                8, 2,
                SRCX_DHIRES,
                value2 << 1
            );
        }
        else {
            BYTE thirdval = hiResAuxPtr[offset + yoffset + 1];
            int value1 = (auxval & 0x7F) | ((mainval & 1) << 7);
            int value2 = ((mainval & 0x7E) >> 1) | ((thirdval & 3) << 6);
            BitBltCell(
                xpixel, ypixel + (yoffset >> 9),
                8, 2,
                SRCX_DHIRES,
                value1 << 1
            );
            BitBltCell(
                xpixel + 8, ypixel + (yoffset >> 9),
                8, 2,
                SRCX_DHIRES,
                value2 << 1
            );
        }
    }
}

//===========================================================================
static void UpdateLoResCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE val = textMainPtr[offset];
    BitBltCell(
        xpixel, ypixel,
        14, 8,
        SRCX_LORES,
        (int)((val & 0xF) << 4)
    );
    BitBltCell(
        xpixel, ypixel + 8,
        14, 8,
        SRCX_LORES,
        (int)(val & 0xF0)
    );
}

//===========================================================================
static void UpdateHiResCell(int x, int y, int xpixel, int ypixel, int offset) {
    for (int yoffset = 0; yoffset < 0x2000; yoffset += 0x400) {
        BYTE prev = (x > 0) ? hiResMainPtr[offset + yoffset - 1] : 0;
        BYTE curr = hiResMainPtr[offset + yoffset];
        BYTE next = (x < 39) ? hiResMainPtr[offset + yoffset + 1] : 0;
        const int c1 = (x > 0  && (prev & 64)) ? 1 : 0;
        const int c2 = (x < 39 && (next & 1))  ? 1 : 0;
        const int coloroffs = c1 << 6 | c2 << 5;
        BitBltCell(
            xpixel, ypixel + (yoffset >> 9),
            14, 2,
            SRCX_HIRES + coloroffs + ((x & 1) << 4),
            (int)curr << 1
        );
    }
}

//===========================================================================
static void UpdateVideoScreen(int64_t cycle) {
    VideoRefreshScreen();
    SchedulerEnqueue(Event(cycle + CYCLES_PER_SCANLINE * NTSC_SCANLINES, UpdateVideoScreen));
}

//===========================================================================
static void UpdateVideoMode(DWORD newMode) {
    videoMode = newMode;

    if (SW_TEXT()) {
        if (SW_80COL())
            updateUpper = Update80ColCell;
        else
            updateUpper = Update40ColCell;
    }
    else if SW_HIRES() {
        if (SW_DHIRES() && SW_80COL())
            updateUpper = UpdateDHiResCell;
        else
            updateUpper = UpdateHiResCell;
    }
    else {
        updateUpper = UpdateLoResCell;
    }

    updateLower = updateUpper;
    if (SW_MIXED()) {
        if (SW_80COL())
            updateLower = Update80ColCell;
        else
            updateLower = Update40ColCell;
    }
}


//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void VideoBenchmark() {
    Sleep(500);

    // PREPARE TWO DIFFERENT FRAME BUFFERS, EACH OF WHICH HAVE HALF OF THE
    // BYTES SET TO 0x14 AND THE OTHER HALF SET TO 0xAA
    {
        LPDWORD mem32 = (LPDWORD)mem;
        for (int loop = 4096; loop < 6144; loop++)
            mem32[loop] = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0x14141414 : 0xAAAAAAAA;
        for (int loop = 6144; loop < 8192; loop++)
            mem32[loop] = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0xAAAAAAAA : 0x14141414;
    }

    // SEE HOW MANY TEXT FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
    // GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
    // SIMULATE THE ACTIVITY OF AN AVERAGE GAME
    DWORD totaltextfps = 0;
    {
        UpdateVideoMode(VF_TEXT);
        modeSwitches = 0;
        FillMemory(mem + 0x400, 0x400, 0x14);
        VideoRedrawScreen();

        DWORD milliseconds = GetTickCount();
        while (GetTickCount() == milliseconds) /* do nothing */;
        milliseconds = GetTickCount();
        DWORD cycle = 0;
        do {
            if (cycle & 1)
                FillMemory(mem + 0x400, 0x400, 0x14);
            else
                CopyMemory(mem + 0x400, mem + ((cycle & 2) ? 0x4000 : 0x6000), 0x400);
            VideoRefreshScreen();
            if (cycle++ >= 3)
                cycle = 0;
            totaltextfps++;
        } while (GetTickCount() - milliseconds < 1000);
    }

    // SEE HOW MANY HIRES FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
    // GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
    // SIMULATE THE ACTIVITY OF AN AVERAGE GAME
    DWORD totalhiresfps = 0;
    {
        UpdateVideoMode(VF_HIRES);
        modeSwitches = 0;
        FillMemory(mem + 0x2000, 0x2000, 0x14);
        VideoRedrawScreen();

        DWORD milliseconds = GetTickCount();
        while (GetTickCount() == milliseconds) /* do nothing */;
        milliseconds = GetTickCount();
        DWORD cycle = 0;
        do {
            if (cycle & 1)
                FillMemory(mem + 0x2000, 0x2000, 0x14);
            else
                CopyMemory(mem + 0x2000, mem + ((cycle & 2) ? 0x4000 : 0x6000), 0x2000);
            VideoRefreshScreen();
            if (cycle++ >= 3)
                cycle = 0;
            totalhiresfps++;
        } while (GetTickCount() - milliseconds < 1000);
    }

    // DETERMINE HOW MANY 65C02 CLOCK CYCLES WE CAN EMULATE PER SECOND WITH
    // NOTHING ELSE GOING ON
    CpuSetupBenchmark();
    DWORD totalmhz10 = 0;
    {
        DWORD milliseconds = GetTickCount();
        while (GetTickCount() == milliseconds) /* do nothing */;
        milliseconds = GetTickCount();
        DWORD cycle = 0;
        do {
            int64_t cyclecounter = 0;
            CpuExecute(100000, &cyclecounter);
            totalmhz10++;
        } while (GetTickCount() - milliseconds < 1000);
    }

    // IF THE PROGRAM COUNTER IS NOT IN THE EXPECTED RANGE AT THE END OF THE
    // CPU BENCHMARK, REPORT AN ERROR AND OPTIONALLY TRACK IT DOWN
    if ((regs.pc < 0x300) || (regs.pc > 0x400)) {
        int response = MessageBox(
            framewindow,
            "The emulator has detected a problem while running "
            "the CPU benchmark.  Would you like to gather more "
            "information?",
            "Benchmarks",
            MB_ICONQUESTION | MB_YESNO
        );
        if (response == IDYES) {
            BOOL    error        = FALSE;
            WORD    lastpc       = 0x300;
            int     loop         = 0;
            int64_t cyclecounter = 0;
            while ((loop < 10000) && !error) {
                CpuSetupBenchmark();
                CpuExecute(loop, &cyclecounter);
                if ((regs.pc < 0x300) || (regs.pc > 0x400))
                    error = TRUE;
                else {
                    lastpc = regs.pc;
                    ++loop;
                }
            }
            if (error) {
                char outstr[256];
                StrPrintf(
                    outstr,
                    ARRSIZE(outstr),
                    "The emulator experienced an error %u clock cycles "
                    "into the CPU benchmark.  Prior to the error, the "
                    "program counter was at $%04X.  After the error, it "
                    "had jumped to $%04X.",
                    (unsigned)loop,
                    (unsigned)lastpc,
                    (unsigned)regs.pc
                );
                MessageBox(
                    framewindow,
                    outstr,
                    "Benchmarks",
                    MB_ICONINFORMATION
                );
            }
            else
                MessageBox(
                    framewindow,
                    "The emulator was unable to locate the exact "
                    "point of the error.  This probably means that "
                    "the problem is external to the emulator, "
                    "happening asynchronously, such as a problem in "
                    "a timer interrupt handler.",
                    "Benchmarks",
                    MB_ICONINFORMATION
                );
        }
    }

    // DO A REALISTIC TEST OF HOW MANY FRAMES PER SECOND WE CAN PRODUCE
    // WITH FULL EMULATION OF THE CPU, JOYSTICK, AND DISK HAPPENING AT
    // THE SAME TIME
    DWORD realisticfps = 0;
    {
        FillMemory(mem + 0x2000, 0x2000, 0xAA);
        VideoRedrawScreen();
        DWORD milliseconds = GetTickCount();
        while (GetTickCount() == milliseconds) /* do nothing */;
        milliseconds = GetTickCount();
        int64_t cyclecounter = 0;
        DWORD cycle = 0;
        do {
            if (realisticfps < 10) {
                int cycles = 100000;
                while (cycles > 0) {
                    int executedcycles = CpuExecute(103, &cyclecounter);
                    cycles -= executedcycles;
                    DiskUpdatePosition(executedcycles);
                    JoyUpdatePosition(executedcycles);
                }
            }
            if (cycle & 1)
                FillMemory(mem + 0x2000, 0x2000, 0xAA);
            else
                CopyMemory(mem + 0x2000, mem + ((cycle & 2) ? 0x4000 : 0x6000), 0x2000);
            VideoRefreshScreen();
            if (cycle++ >= 3)
                cycle = 0;
            realisticfps++;
        } while (GetTickCount() - milliseconds < 1000);
    }

    // DISPLAY THE RESULTS
    VideoDisplayLogo();
    {
        char outstr[256];
        StrPrintf(
            outstr,
            ARRSIZE(outstr),
            "Pixel Format:\t%x\n"
            "Pure Video FPS:\t%u hires, %u text\n"
            "Pure CPU MHz:\t%u.%u%s\n\n"
            "EXPECTED AVERAGE VIDEO GAME\n"
            "PERFORMANCE: %u FPS",
            (unsigned)0x120,
            (unsigned)totalhiresfps,
            (unsigned)totaltextfps,
            (unsigned)(totalmhz10 / 10),
            (unsigned)(totalmhz10 % 10),
            apple2e ? "" : " (6502)",
            (unsigned)realisticfps
        );
        MessageBox(
            framewindow,
            outstr,
            "Benchmarks",
            MB_ICONINFORMATION
        );
    }
}

//===========================================================================
BYTE VideoCheckMode(WORD, BYTE address, BYTE, BYTE) {
    if (address == 0x7F)
        return MemReturnRandomData(SW_DHIRES() != 0);
    else {
        BOOL result = 0;
        switch (address) {
            case 0x1A: result = SW_TEXT();       break;
            case 0x1B: result = SW_MIXED();      break;
            case 0x1D: result = SW_HIRES();      break;
            case 0x1E: result = charOffset != 0; break;
            case 0x1F: result = SW_80COL();      break;
            case 0x7F: result = SW_DHIRES();     break;
        }
        return KeybGetKeycode() | (result ? 0x80 : 0);
    }
}

//===========================================================================
BYTE VideoCheckVbl(WORD pc, BYTE address, BYTE write, BYTE value) {
    int64_t frameCycle = g_cyclesEmulated % (CYCLES_PER_SCANLINE * NTSC_SCANLINES);
    return MemReturnRandomData(frameCycle < (CYCLES_PER_SCANLINE * DISPLAY_LINES));
}

//===========================================================================
void VideoDestroy() {
    VideoReleaseFrameDC();

    delete[] sourceLookup;
    sourceLookup = NULL;

    DeleteDC(deviceDC);
    DeleteObject(deviceBitmap);
    DeleteObject(videoFont);
    deviceDC     = (HDC)0;
    deviceBitmap = (HBITMAP)0;
    videoFont    = (HFONT)0;
}

//===========================================================================
void VideoDisplayLogo() {
    screenPixels = WindowLockPixels();
    LoadPngImage("LOGO", SCREEN_CX, SCREEN_CY, screenPixels, LOADPNG_CONVERT_RGB);
    WindowUnlockPixels();

    if (!frameDC)
        frameDC = FrameGetDC();

    BOOL success = LoadPngImage(
        "LOGO",
        SCREEN_CX,
        SCREEN_CY,
        frameBuffer,
        LOADPNG_CONVERT_RGB | LOADPNG_BOTTOM_UP
    );
    if (success) {
        BitBlt(
            frameDC,
            0, 0,
            SCREEN_CX, SCREEN_CY,
            deviceDC,
            0, 0,
            SRCCOPY
        );
        GdiFlush();
    }

    // DRAW THE VERSION NUMBER
    HFONT font = CreateFont(
        -20,
        0,
        0,
        0,
        FW_NORMAL,
        0,
        0,
        0,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        VARIABLE_PITCH | 4 | FF_SWISS,
        "Arial"
    );
    HFONT oldfont = (HFONT)SelectObject(frameDC, font);
    SetTextAlign(frameDC, TA_RIGHT | TA_TOP);
    SetBkMode(frameDC, TRANSPARENT);
    char version[16];
    StrPrintf(version, ARRSIZE(version), "Version %d.%d", VERSIONMAJOR, VERSIONMINOR);
    SetTextColor(frameDC, 0x2727D2);
    TextOut(frameDC, 540, 358, version, StrLen(version));
    SetTextAlign(frameDC, TA_RIGHT | TA_TOP);
    SelectObject(frameDC, oldfont);
    DeleteObject(font);

    FrameReleaseDC(frameDC);
    frameDC = (HDC)0;
}

//===========================================================================
void VideoDisplayMode(BOOL flashon) {
    if (!frameDC)
        frameDC = FrameGetDC();

    char * text = "        ";
    if (GetMode() == EMULATOR_MODE_PAUSED) {
        SetBkColor(frameDC, 0x000000);
        SetTextColor(frameDC, 0x00FFFFF);
        if (flashon)
            text = " PAUSED ";
    }
    else {
        SetBkColor(frameDC, 0xFFFFFF);
        SetTextColor(frameDC, 0x800000);
        text = "STEPPING";
    }

    SelectObject(frameDC, videoFont);
    SetTextAlign(frameDC, TA_LEFT | TA_TOP);
    RECT rect { SCREEN_CX - 68, 0, SCREEN_CX, 16 };
    ExtTextOut(frameDC, SCREEN_CX - 65, 0, ETO_CLIPPED | ETO_OPAQUE, &rect, text, 8, NULL);
}

//===========================================================================
void VideoGenerateSourceFiles() {
    bool mono = false;
    for (int i = 0; i < 2; i++, mono = !mono) {
        HWND    window = GetDesktopWindow();
        HDC     dc     = GetDC(window);
        HDC     memdc  = CreateCompatibleDC(dc);
        HBRUSH  brush  = CreateSolidBrush(0x00FF00);
        HBITMAP bitmap = CreateBitmap(SRC_CX, SRC_CY, 1, 32, NULL);
        SelectObject(memdc, bitmap);
        SelectObject(memdc, GetStockObject(BLACK_BRUSH));
        SelectObject(memdc, GetStockObject(NULL_PEN));
        Rectangle(memdc, 0, 0, SRC_CX, SRC_CY);
        SelectObject(memdc, brush);
        if (mono) {
            DrawMonoTextSource(memdc);
            DrawMonoHiResSource(memdc);
            DrawMonoDHiResSource(memdc);
            SelectObject(memdc, GetStockObject(BLACK_PEN));
            for (int loop = 510; loop >= 0; loop -= 2) {
                MoveToEx(memdc, 0, loop, NULL);
                LineTo(memdc, SRC_CX, loop);
            }
        }
        else {
            DrawTextSource(memdc);
            DrawLoResSource(memdc);
            DrawHiResSource(memdc);
            DrawDHiResSource(memdc);
        }
        DeleteDC(memdc);
        ReleaseDC(window, dc);
        GetBitmapBits(bitmap, SRC_CX * SRC_CY * sizeof(uint32_t), sourceLookup);
        DeleteObject(bitmap);
        DeleteObject(brush);

        char filename[MAX_PATH];
        StrPrintf(filename, ARRSIZE(filename), "%ssource%s.png", programDir, mono ? "mono" : "color");

        FILE * fp = fopen(filename, "wb");
        if (!fp)
            continue;

        png_structp write = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        png_infop info = png_create_info_struct(write);
        png_init_io(write, fp);

        png_set_IHDR(
            write,
            info,
            SRC_CX,
            SRC_CY,
            8,
            PNG_COLOR_TYPE_RGBA,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE,
            PNG_FILTER_TYPE_BASE
        );

        png_write_info(write, info);

        const uint32_t * row = sourceLookup;
        for (int y = 0; y < SRC_CY; y++, row += SRC_CX)
            png_write_row(write, (png_bytep)row);

        png_write_end(write, info);
        png_destroy_write_struct(&write, &info);
        fclose(fp);
    }
}

//===========================================================================
void VideoInitialize() {
    // CREATE A FONT FOR DRAWING TEXT ABOVE THE SCREEN
    videoFont = CreateFont(
        16,
        0,
        0,
        0,
        FW_MEDIUM,
        0,
        0,
        0,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        FIXED_PITCH | 4 | FF_MODERN,
        "Courier New"
    );

    // THE DEVICE MUST BE 32 BPP AND A SINGLE PLANE
    HWND window       = GetDesktopWindow();
    HDC  dc           = GetDC(window);
    int  planes       = GetDeviceCaps(dc, PLANES);
    int  bitsperpixel = planes * GetDeviceCaps(dc, BITSPIXEL);
    ReleaseDC(window, dc);
    if (bitsperpixel != 32 && planes != 1)
        return;

    // CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
    int framebuffersize = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
    BITMAPINFO * framebufferinfo = (BITMAPINFO *)new BYTE[framebuffersize];
    ZeroMemory(framebufferinfo, framebuffersize);
    framebufferinfo->bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    framebufferinfo->bmiHeader.biWidth    = SCREEN_CX;
    framebufferinfo->bmiHeader.biHeight   = SCREEN_CY;
    framebufferinfo->bmiHeader.biPlanes   = 1;
    framebufferinfo->bmiHeader.biBitCount = 32;
    framebufferinfo->bmiHeader.biClrUsed  = 256;

    // CREATE A BIT BUFFER FOR THE SOURCE LOOKUP
    int sourcedwords = SRC_CX * SRC_CY;
    sourceLookup = new uint32_t[sourcedwords];
    ZeroMemory(sourceLookup, sourcedwords * sizeof(uint32_t));

    // CREATE THE DEVICE DEPENDENT BITMAP AND DEVICE CONTEXT
    {
        HWND window = GetDesktopWindow();
        HDC  dc = GetDC(window);
        frameBuffer = NULL;
        deviceBitmap = CreateDIBSection(
            dc,
            framebufferinfo,
            DIB_RGB_COLORS,
            (LPVOID *)&frameBuffer,
            0,
            0
        );
        deviceDC = CreateCompatibleDC(dc);
        ReleaseDC(window, dc);
        SelectObject(deviceDC, deviceBitmap);
    }
    delete[] framebufferinfo;

    LoadSourceLookup();
    VideoResetState();
    SchedulerEnqueue(Event(CYCLES_PER_SCANLINE * NTSC_SCANLINES, UpdateVideoScreen));
}

//===========================================================================
void VideoRedrawScreen() {
    redrawFull = TRUE;
    VideoRefreshScreen();
}

//===========================================================================
void VideoRefreshScreen() {
    if (!frameDC)
        frameDC = FrameGetDC();

    // IF THE MODE HAS BEEN SWITCHED MORE THAN TWICE IN THE LAST FRAME, THE
    // PROGRAM IS PROBABLY TRYING TO DO A FLASHING EFFECT, SO JUST FLASH THE
    // SCREEN WHITE AND RETURN
    if (modeSwitches > 2) {
        modeSwitches = 0;
        SelectObject(frameDC, GetStockObject(WHITE_BRUSH));
        SelectObject(frameDC, GetStockObject(WHITE_PEN));
        Rectangle(frameDC, 0, 0, SCREEN_CX, SCREEN_CY);
        return;
    }
    modeSwitches = 0;

    displayPage2 = (SW_PAGE2() != 0);
    hiResAuxPtr  = MemGetAuxPtr(0x2000 << (displayPage2 ? 1 : 0));
    hiResMainPtr = MemGetMainPtr(0x2000 << (displayPage2 ? 1 : 0));
    textAuxPtr   = MemGetAuxPtr(0x400 << (displayPage2 ? 1 : 0));
    textMainPtr  = MemGetMainPtr(0x400 << (displayPage2 ? 1 : 0));
    screenPixels = WindowLockPixels();

    int y      = 0;
    int ypixel = 0;
    for (; y < 20; y++, ypixel += 16) {
        int offset = ((y & 7) << 7) + ((y >> 3) * 40);
        for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14)
            updateUpper(x, y, xpixel, ypixel, offset + x);
    }
    for (; y < 24; y++, ypixel += 16) {
        int offset = ((y & 7) << 7) + ((y >> 3) * 40);
        for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14)
            updateLower(x, y, xpixel, ypixel, offset + x);
    }

    WindowUnlockPixels();

    BitBlt(
        frameDC,
        0, 0,
        SCREEN_CX, SCREEN_CY,
        deviceDC,
        0, 0,
        SRCCOPY
    );

    GdiFlush();
    redrawFull = FALSE;

    EEmulatorMode mode = GetMode();
    if ((mode == EMULATOR_MODE_PAUSED) || (mode == EMULATOR_MODE_STEPPING))
        VideoDisplayMode(TRUE);
}

//===========================================================================
void VideoReinitialize() {
    LoadSourceLookup();
}

//===========================================================================
void VideoReleaseFrameDC() {
    if (frameDC) {
        FrameReleaseDC(frameDC);
        frameDC = (HDC)0;
    }
}

//===========================================================================
void VideoResetState() {
    charOffset     = 0;
    displayPage2 = FALSE;
    redrawFull   = FALSE;
    UpdateVideoMode(VF_TEXT);
}

//===========================================================================
BYTE VideoSetMode(WORD, BYTE address, BYTE write, BYTE) {
    DWORD oldpage2  = SW_PAGE2();
    DWORD oldmodeex = charOffset | (videoMode & ~(VF_MASK2 | VF_PAGE2));

    DWORD newmode = videoMode;
    switch (address) {
        case 0x00: newmode &= ~VF_MASK2;   break;
        case 0x01: newmode |= VF_MASK2;    break;
        case 0x0C: newmode &= ~VF_80COL;   break;
        case 0x0D: newmode |= VF_80COL;    break;
        case 0x0E: charOffset = 0;         break;
        case 0x0F: charOffset = 256;       break;
        case 0x50: newmode &= ~VF_TEXT;    break;
        case 0x51: newmode |= VF_TEXT;     break;
        case 0x52: newmode &= ~VF_MIXED;   break;
        case 0x53: newmode |= VF_MIXED;    break;
        case 0x54: newmode &= ~VF_PAGE2;   break;
        case 0x55: newmode |= VF_PAGE2;    break;
        case 0x56: newmode &= ~VF_HIRES;   break;
        case 0x57: newmode |= VF_HIRES;    break;
        case 0x5E: newmode |= VF_DHIRES;   break;
        case 0x5F: newmode &= ~VF_DHIRES;  break;
    }
    if (newmode & VF_MASK2)
        newmode &= ~VF_PAGE2;

    UpdateVideoMode(newmode);

    DWORD newmodeex = charOffset | (videoMode & ~(VF_MASK2 | VF_PAGE2));
    if (oldmodeex != newmodeex) {
        if ((SW_80COL() != 0) == (SW_DHIRES() != 0))
            modeSwitches++;
        redrawFull = TRUE;
    }

    DWORD currtime = DWORD(g_cyclesEmulated / CPU_CYCLES_PER_MS);

    if (fullSpeed && oldpage2 && !SW_PAGE2()) {
        static DWORD lasttime = 0;
        if (currtime - lasttime >= 20)
            lasttime = currtime;
        else
            oldpage2 = SW_PAGE2();
    }

    if (oldpage2 != SW_PAGE2()) {
        displayPage2 = (SW_PAGE2() != 0);
        if (!redrawFull)
            VideoRefreshScreen();
    }

    if (address == 0x50)
        return VideoCheckVbl(0, 0, 0, 0);
    else
        return MemReturnRandomData(TRUE);
}
