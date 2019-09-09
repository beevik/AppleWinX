/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

constexpr int SRCOFFS_40COL    = 0;
constexpr int SRCOFFS_80COL    = 256;
constexpr int SRCOFFS_LORES    = 384;
constexpr int SRCOFFS_HIRES    = 400;
constexpr int SRCOFFS_DHIRES   = 528;
constexpr int SRCOFFS_UNUSED   = 536;
constexpr int SRCOFFS_TOTAL    = 544;

constexpr DWORD VF_80COL       = 0x00000001;
constexpr DWORD VF_DHIRES      = 0x00000002;
constexpr DWORD VF_HIRES       = 0x00000004;
constexpr DWORD VF_MASK2       = 0x00000008;
constexpr DWORD VF_MIXED       = 0x00000010;
constexpr DWORD VF_PAGE2       = 0x00000020;
constexpr DWORD VF_TEXT        = 0x00000040;

#define  SW_80COL()     (vidmode & VF_80COL)
#define  SW_DHIRES()    (vidmode & VF_DHIRES)
#define  SW_HIRES()     (vidmode & VF_HIRES)
#define  SW_MASK2()     (vidmode & VF_MASK2)
#define  SW_MIXED()     (vidmode & VF_MIXED)
#define  SW_PAGE2()     (vidmode & VF_PAGE2)
#define  SW_TEXT()      (vidmode & VF_TEXT)

typedef void (* fupdate)(int x, int y, int xpixel, int ypixel, int offset);

BOOL graphicsmode = FALSE;

static HBITMAP       devicebitmap;
static HDC           devicedc;
static uint32_t *    framebuffer;
static LPBYTE        hiresauxptr;
static LPBYTE        hiresmainptr;
static HANDLE        logofile;
static HANDLE        logomap;
static LPBITMAPINFO  logoptr;
static LPVOID        logoview;
static uint32_t *    sourcelookup;
static LPBYTE        textauxptr;
static LPBYTE        textmainptr;
static fupdate       updatelower;
static fupdate       updateupper;

static DWORD  charoffs     = 0;
static BOOL   displaypage2 = FALSE;
static HDC    framedc      = (HDC)0;
static BOOL   hasrefreshed = FALSE;
static DWORD  lastpageflip = 0;
static DWORD  modeswitches = 0;
static BOOL   redrawfull   = TRUE;
static DWORD  vblcounter   = 0;
static HFONT  videofont    = (HFONT)0;
static LPBYTE vidlastmem   = NULL;
static DWORD  vidmode      = 0;

static const COLORREF locolor[16] = {
    0x000000, 0x7C0B93, 0xD3351F, 0xFF36BB, // black, red, dkblue, purple
    0x0C7600, 0x7E7E7E, 0xE0A807, 0xFFAC9D, // dkgreen, grey, medblue, ltblue
    0x004C62, 0x1D56F9, 0x7E7E7E, 0xEC81FF, // brown, orange, grey, pink
    0x00C843, 0x16CDDC, 0x84F75D, 0xFFFFFF, // ltgreen, yellow, aqua, white
};

static const COLORREF hicolor[6] = {
    0xFF36BB, 0xE0A807, 0x00C843,   // purple, blue, green
    0x1D56F9, 0x000000, 0xFFFFFF,   // orange, black, white
};

static BOOL LoadSourceLookup();
static void SaveSourceLookup();

//===========================================================================
static void BitBltCell(
    int dstx,
    int dsty,
    int xsize,
    int ysize,
    int srcx,
    int srcy
) {
    uint32_t * dstptr = framebuffer + 560 * (383 - dsty) + dstx;
    uint32_t * srcptr = sourcelookup + SRCOFFS_TOTAL * srcy + srcx;
    while (ysize--) {
        for (int x = xsize - 1; x >= 0; x--)
            dstptr[x] = srcptr[x];
        dstptr -= 560;
        srcptr += SRCOFFS_TOTAL;
    }
}

//===========================================================================
static void BitBlitHiResRow(int y) {
    const int       yoffset = (y & 7) << 10 | (y & 0x38) << 4 | (y & 0xc0) >> 1 | (y & 0xc0) >> 3;
    const uint8_t * srcptr  = hiresmainptr + yoffset;
    uint32_t *      dstptr  = framebuffer + 560 * (383 - y);
    for (int x = 0; x < 40; x += 2) {
        srcptr[x];
    }
}

//===========================================================================
static void DrawDHiResSource(HDC dc) {
    for (int value = 0; value < 256; value++) {
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 2; y++) {
                int color = (x < 4) ? (value & 0xF) : (value >> 4);
                SetPixel(dc, SRCOFFS_DHIRES + x, (value << 1) + y, locolor[color]);
            }
        }
    }
}

//===========================================================================
static void DrawLoResSource(HDC dc) {
    for (int color = 0; color < 16; color++) {
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++)
                SetPixelV(dc, SRCOFFS_LORES + x, (color << 4) + y, locolor[color]);
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

                    SetPixelV(dc, SRCOFFS_HIRES + coloffs + x + adj + 0, y + 0, hicolor[color]);
                    SetPixelV(dc, SRCOFFS_HIRES + coloffs + x + adj + 1, y + 0, hicolor[color]);
                    SetPixelV(dc, SRCOFFS_HIRES + coloffs + x + adj + 0, y + 1, hicolor[color]);
                    SetPixelV(dc, SRCOFFS_HIRES + coloffs + x + adj + 1, y + 1, hicolor[color]);
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
            SetPixel(dc, SRCOFFS_DHIRES + x, (value << 1) + 1, color);
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
                SetPixelV(dc, SRCOFFS_HIRES + column + x, y + 1, colorval);
                SetPixelV(dc, SRCOFFS_HIRES + column + x + 1, y + 1, colorval);
            } while ((x += 2) < 16);
        } while ((y += 2) < 512);
    } while ((column += 16) < 128);
}

//===========================================================================
static void DrawMonoTextSource(HDC dc) {
    HDC     memdc = CreateCompatibleDC(dc);
    HBITMAP bitmap = LoadBitmap(instance, "CHARSET40");
    SelectObject(memdc, bitmap);
    BitBlt(dc, SRCOFFS_40COL, 0, 256, 512, memdc, 0, 0, MERGECOPY);
    StretchBlt(dc, SRCOFFS_80COL, 0, 128, 512, memdc, 0, 0, 256, 512, MERGECOPY);
    DeleteDC(memdc);
    DeleteObject(bitmap);
}

//===========================================================================
static void DrawTextSource(HDC dc) {
    HDC     memdc = CreateCompatibleDC(dc);
    HBITMAP bitmap = LoadBitmap(instance, "CHARSET40");
    SelectObject(memdc, bitmap);
    BitBlt(dc, SRCOFFS_40COL, 0, 256, 512, memdc, 0, 0, SRCCOPY);
    StretchBlt(dc, SRCOFFS_80COL, 0, 128, 512, memdc, 0, 0, 256, 512, SRCCOPY);
    DeleteDC(memdc);
    DeleteObject(bitmap);
}

//===========================================================================
static void InitializeSourceLookup() {
    if (!LoadSourceLookup()) {
        HWND    window = GetDesktopWindow();
        HDC     dc     = GetDC(window);
        HDC     memdc  = CreateCompatibleDC(dc);
        HBRUSH  brush  = CreateSolidBrush(0x00FF00);
        HBITMAP bitmap = CreateBitmap(SRCOFFS_TOTAL, 512, 1, 32, NULL);
        SelectObject(memdc, bitmap);
        SelectObject(memdc, GetStockObject(BLACK_BRUSH));
        SelectObject(memdc, GetStockObject(NULL_PEN));
        Rectangle(memdc, 0, 0, SRCOFFS_TOTAL, 512);
        SelectObject(memdc, brush);
        if (optmonochrome) {
            DrawMonoTextSource(memdc);
            DrawMonoHiResSource(memdc);
            DrawMonoDHiResSource(memdc);
            SelectObject(memdc, GetStockObject(BLACK_PEN));
            for (int loop = 510; loop >= 0; loop -= 2) {
                MoveToEx(memdc, 0, loop, NULL);
                LineTo(memdc, SRCOFFS_TOTAL, loop);
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
        GetBitmapBits(bitmap, SRCOFFS_TOTAL * 512 * sizeof(uint32_t), sourcelookup);
        DeleteObject(bitmap);
        DeleteObject(brush);
        SaveSourceLookup();
    }
}

//===========================================================================
static BOOL LoadSourceLookup() {
    if (!sourcelookup)
        return FALSE;

    char filename[MAX_PATH];
    StrPrintf(
        filename,
        ARRSIZE(filename),
        "%sVid%03X%s.dat",
        progdir,
        0x120,
        optmonochrome ? "m" : "c"
    );

    HANDLE file = CreateFile(
        filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    if (file != INVALID_HANDLE_VALUE) {
        DWORD bytestoread = SRCOFFS_TOTAL * 512 * sizeof(uint32_t);
        DWORD bytesread   = 0;
        (void)ReadFile(file, sourcelookup, bytestoread, &bytesread, NULL);
        CloseHandle(file);
        return bytesread == bytestoread;
    }

    return FALSE;
}

//===========================================================================
static void SaveSourceLookup() {
    if (!sourcelookup)
        return;

    char filename[MAX_PATH];
    StrPrintf(
        filename,
        ARRSIZE(filename),
        "%sVid%03X%s.dat",
        progdir,
        0x120,
        optmonochrome ? "m" : "c"
    );

    HANDLE file = CreateFile(
        filename,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    if (file != INVALID_HANDLE_VALUE) {
        DWORD bytestowrite = SRCOFFS_TOTAL * 512 * sizeof(uint32_t);
        DWORD byteswritten = 0;
        WriteFile(file, sourcelookup, bytestowrite, &byteswritten, NULL);
        CloseHandle(file);
        if (byteswritten != bytestowrite)
            DeleteFile(filename);
    }
}

//===========================================================================
static void SetLastDrawnImage() {
    memcpy(vidlastmem + 0x400, textmainptr, 0x400);

    if (SW_HIRES())
        memcpy(vidlastmem + 0x2000, hiresmainptr, 0x2000);

    if (SW_DHIRES())
        memcpy(vidlastmem, hiresauxptr, 0x2000);
    else if (SW_80COL())
        memcpy(vidlastmem, textauxptr, 0x400);

    for (int loop = 0; loop < 256; loop++)
        memdirty[loop] &= ~2;
}

//===========================================================================
static void Update40ColCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE ch = textmainptr[offset];
    BitBltCell(
        xpixel, ypixel,
        14, 16,
        SRCOFFS_40COL + ((ch & 0x0F) << 4),
        (ch & 0xF0) + charoffs
    );
}

//===========================================================================
static void Update80ColCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE auxval  = textauxptr[offset];
    BYTE mainval = textmainptr[offset];
    BitBltCell(
        xpixel, ypixel,
        7, 16,
        SRCOFFS_80COL + ((auxval & 15) << 3),
        ((auxval >> 4) << 4) + charoffs
    );
    BitBltCell(
        xpixel + 7, ypixel,
        7, 16,
        SRCOFFS_80COL + ((mainval & 15) << 3),
        ((mainval >> 4) << 4) + charoffs
    );
}

//===========================================================================
static void UpdateDHiResCell(int x, int y, int xpixel, int ypixel, int offset) {
    for (int yoffset = 0; yoffset < 0x2000; yoffset += 0x400) {
        BYTE auxval  = hiresauxptr[offset + yoffset];
        BYTE mainval = hiresmainptr[offset + yoffset];
        BOOL draw = (auxval != vidlastmem[offset + yoffset]) ||
            (mainval != vidlastmem[offset + yoffset + 0x2000]) ||
            redrawfull;
        if (offset & 1) {
            BYTE thirdval = hiresmainptr[offset + yoffset - 1];
            int value1 = ((auxval & 0x3F) << 2) | ((thirdval & 0x60) >> 5);
            int value2 = ((mainval & 0x7F) << 1) | ((auxval & 0x40) >> 6);
            BitBltCell(
                xpixel - 2, ypixel + (yoffset >> 9),
                8, 2,
                SRCOFFS_DHIRES,
                value1 << 1
            );
            BitBltCell(
                xpixel + 6, ypixel + (yoffset >> 9),
                8, 2,
                SRCOFFS_DHIRES,
                value2 << 1
            );
        }
        else {
            BYTE thirdval = hiresauxptr[offset + yoffset + 1];
            int value1 = (auxval & 0x7F) | ((mainval & 1) << 7);
            int value2 = ((mainval & 0x7E) >> 1) | ((thirdval & 3) << 6);
            BitBltCell(
                xpixel, ypixel + (yoffset >> 9),
                8, 2,
                SRCOFFS_DHIRES,
                value1 << 1
            );
            BitBltCell(
                xpixel + 8, ypixel + (yoffset >> 9),
                8, 2,
                SRCOFFS_DHIRES,
                value2 << 1
            );
        }
    }
}

//===========================================================================
static void UpdateLoResCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE val = textmainptr[offset];
    BitBltCell(
        xpixel, ypixel,
        14, 8,
        SRCOFFS_LORES,
        (int)((val & 0xF) << 4)
    );
    BitBltCell(
        xpixel, ypixel + 8,
        14, 8,
        SRCOFFS_LORES,
        (int)(val & 0xF0)
    );
}

//===========================================================================
static void UpdateHiResCell(int x, int y, int xpixel, int ypixel, int offset) {
    for (int yoffset = 0; yoffset < 0x2000; yoffset += 0x400) {
        BYTE prev = (x > 0) ? hiresmainptr[offset + yoffset - 1] : 0;
        BYTE curr = hiresmainptr[offset + yoffset];
        BYTE next = (x < 39) ? hiresmainptr[offset + yoffset + 1] : 0;
        const int c1 = (x > 0  && (prev & 64)) ? 1 : 0;
        const int c2 = (x < 39 && (next & 1))  ? 1 : 0;
        const int coloroffs = c1 << 6 | c2 << 5;
        BitBltCell(
            xpixel, ypixel + (yoffset >> 9),
            14, 2,
            SRCOFFS_HIRES + coloroffs + ((x & 1) << 4),
            (int)curr << 1
        );
    }
}

//===========================================================================
static void UpdateVideoMode(DWORD newmode) {
    vidmode = newmode;

    if (SW_TEXT()) {
        if (SW_80COL())
            updateupper = Update80ColCell;
        else
            updateupper = Update40ColCell;
    }
    else if SW_HIRES() {
        if (SW_DHIRES() && SW_80COL())
            updateupper = UpdateDHiResCell;
        else
            updateupper = UpdateHiResCell;
    }
    else {
        updateupper = UpdateLoResCell;
    }

    updatelower = updateupper;
    if (SW_MIXED()) {
        if (SW_80COL())
            updatelower = Update80ColCell;
        else
            updatelower = Update40ColCell;
    }
}


//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BOOL VideoApparentlyDirty() {
    if (SW_MIXED() || redrawfull)
        return TRUE;

    DWORD page  = (SW_HIRES() && !SW_TEXT()) ? (0x20 << displaypage2) : (0x04 << displaypage2);
    DWORD count = (SW_HIRES() && !SW_TEXT()) ? 0x20 : 0x04;
    while (count--) {
        if (memdirty[page++] & 2)
            return TRUE;
    }
    return FALSE;
}

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
        modeswitches = 0;
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
        modeswitches = 0;
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
            CpuExecute(100000);
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
            BOOL error  = FALSE;
            WORD lastpc = 0x300;
            int  loop   = 0;
            while ((loop < 10000) && !error) {
                CpuSetupBenchmark();
                CpuExecute(loop);
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
        DWORD cycle = 0;
        do {
            if (realisticfps < 10) {
                int cycles = 100000;
                while (cycles > 0) {
                    DWORD executedcycles = CpuExecute(103);
                    cycles -= executedcycles;
                    DiskUpdatePosition(executedcycles);
                    JoyUpdatePosition(executedcycles);
                    VideoUpdateVbl(executedcycles, 0);
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
            case 0x1A: result = SW_TEXT();    break;
            case 0x1B: result = SW_MIXED();   break;
            case 0x1D: result = SW_HIRES();   break;
            case 0x1E: result = charoffs;     break;
            case 0x1F: result = SW_80COL();   break;
            case 0x7F: result = SW_DHIRES();  break;
        }
        return KeybGetKeycode() | (result ? 0x80 : 0);
    }
}

//===========================================================================
void VideoCheckPage(BOOL force) {
    if ((displaypage2 != (SW_PAGE2() != 0)) && (force || (emulmsec - lastpageflip > 500))) {
        displaypage2 = (SW_PAGE2() != 0);
        VideoRefreshScreen();
        hasrefreshed = TRUE;
        lastpageflip = emulmsec;
    }
}

//===========================================================================
BYTE VideoCheckVbl(WORD pc, BYTE address, BYTE write, BYTE value) {
    return MemReturnRandomData(vblcounter < 22);
}

//===========================================================================
void VideoDestroy() {
    VideoReleaseFrameDC();

    delete[] sourcelookup;
    delete[] vidlastmem;
    sourcelookup = NULL;
    vidlastmem = NULL;

    DeleteDC(devicedc);
    DeleteObject(devicebitmap);
    DeleteObject(videofont);
    devicedc = (HDC)0;
    devicebitmap = (HBITMAP)0;
    videofont = (HFONT)0;

    if (logoptr)
        VideoDestroyLogo();
}

//===========================================================================
void VideoDestroyLogo() {
    if (logoptr) {
        UnmapViewOfFile(logoview);
        CloseHandle(logomap);
        CloseHandle(logofile);
        logofile = INVALID_HANDLE_VALUE;
        logomap = INVALID_HANDLE_VALUE;
        logoptr = NULL;
        logoview = NULL;
    }
}

//===========================================================================
void VideoDisplayLogo() {
    if (!framedc)
        framedc = FrameGetDC();

    if (logoptr) {
        SetDIBitsToDevice(
            framedc,
            0,
            0,
            logoptr->bmiHeader.biWidth,
            logoptr->bmiHeader.biHeight,
            0,
            0,
            0,
            logoptr->bmiHeader.biHeight,
            (LPBYTE)logoptr + sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD),
            logoptr,
            DIB_RGB_COLORS
        );
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
    HFONT oldfont = (HFONT)SelectObject(framedc, font);
    SetTextAlign(framedc, TA_RIGHT | TA_TOP);
    SetBkMode(framedc, TRANSPARENT);
    char version[16];
    StrPrintf(version, ARRSIZE(version), "Version %d.%d", VERSIONMAJOR, VERSIONMINOR);
#define DRAWVERSION(x,y,c) SetTextColor(framedc, c); TextOut(framedc, 540 + x, 358 + y, version, StrLen(version));
    DRAWVERSION( 1,  1, 0x6A2136);
    DRAWVERSION(-1, -1, 0xE76BBD);
    DRAWVERSION( 0,  0, 0xD63963);
#undef  DRAWVERSION
    SetTextAlign(framedc, TA_RIGHT | TA_TOP);
    SelectObject(framedc, oldfont);
    DeleteObject(font);

    FrameReleaseDC(framedc);
    framedc = (HDC)0;
}

//===========================================================================
void VideoDisplayMode(BOOL flashon) {
    if (!framedc)
        framedc = FrameGetDC();

    char * text = "        ";
    if (mode == MODE_PAUSED) {
        SetBkColor(framedc, 0x000000);
        SetTextColor(framedc, 0x00FFFFF);
        if (flashon)
            text = " PAUSED ";
    }
    else {
        SetBkColor(framedc, 0xFFFFFF);
        SetTextColor(framedc, 0x800000);
        text = "STEPPING";
    }

    SelectObject(framedc, videofont);
    SetTextAlign(framedc, TA_LEFT | TA_TOP);
    RECT rect { 492, 0, 560, 16 };
    ExtTextOut(framedc, 495, 0, ETO_CLIPPED | ETO_OPAQUE, &rect, text, 8, NULL);
}

//===========================================================================
BOOL VideoHasRefreshed() {
    BOOL result = hasrefreshed;
    hasrefreshed = FALSE;
    return result;
}

//===========================================================================
void VideoInitialize() {
    // CREATE A FONT FOR DRAWING TEXT ABOVE THE SCREEN
    videofont = CreateFont(
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

    // CREATE A BUFFER FOR AN IMAGE OF THE LAST DRAWN MEMORY
    vidlastmem = new BYTE[0x10000];
    ZeroMemory(vidlastmem, 0x10000);

    // THE DEVICE MUST BE 32 BPP AND A SINGLE PLANE
    HWND window       = GetDesktopWindow();
    HDC  dc           = GetDC(window);
    int  planes       = GetDeviceCaps(dc, PLANES);
    int  bitsperpixel = planes * GetDeviceCaps(dc, BITSPIXEL);
    ReleaseDC(window, dc);
    if (bitsperpixel != 32 && planes != 1)
        return;

    // LOAD THE LOGO
    VideoLoadLogo();

    // CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
    int framebuffersize = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
    BITMAPINFO * framebufferinfo = (BITMAPINFO *)new BYTE[framebuffersize];
    ZeroMemory(framebufferinfo, framebuffersize);
    framebufferinfo->bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    framebufferinfo->bmiHeader.biWidth    = 560;
    framebufferinfo->bmiHeader.biHeight   = 384;
    framebufferinfo->bmiHeader.biPlanes   = 1;
    framebufferinfo->bmiHeader.biBitCount = 32;
    framebufferinfo->bmiHeader.biClrUsed  = 256;

    // CREATE A BIT BUFFER FOR THE SOURCE LOOKUP
    int sourcedwords = SRCOFFS_TOTAL * 512;
    sourcelookup = new uint32_t[sourcedwords];
    ZeroMemory(sourcelookup, sourcedwords * sizeof(uint32_t));

    // CREATE THE DEVICE DEPENDENT BITMAP AND DEVICE CONTEXT
    {
        HWND window = GetDesktopWindow();
        HDC  dc = GetDC(window);
        framebuffer = NULL;
        devicebitmap = CreateDIBSection(
            dc,
            framebufferinfo,
            DIB_RGB_COLORS,
            (LPVOID *)&framebuffer,
            0,
            0
        );
        devicedc = CreateCompatibleDC(dc);
        ReleaseDC(window, dc);
        SelectObject(devicedc, devicebitmap);
    }
    delete[] framebufferinfo;

    InitializeSourceLookup();
    VideoResetState();
}

//===========================================================================
void VideoLoadLogo() {
    if (logoptr)
        return;

    char filename[MAX_PATH];
    StrCopy(filename, progdir, ARRSIZE(filename));
    StrCat(filename, "applewin.lgo", ARRSIZE(filename));

    logofile = CreateFile(
        filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    logomap = CreateFileMapping(
        logofile,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );

    logoview = (LPBYTE)MapViewOfFile(logomap, FILE_MAP_READ, 0, 0, 0);
    if (logoview)
        logoptr = (LPBITMAPINFO)((LPBYTE)logoview + 0x200 + sizeof(BITMAPFILEHEADER));
    else
        logoptr = NULL;
}

//===========================================================================
void VideoRedrawScreen() {
    redrawfull = TRUE;
    VideoRefreshScreen();
}

//===========================================================================
void VideoRefreshScreen() {
    if (!framedc)
        framedc = FrameGetDC();

    // IF THE MODE HAS BEEN SWITCHED MORE THAN TWICE IN THE LAST FRAME, THE
    // PROGRAM IS PROBABLY TRYING TO DO A FLASHING EFFECT, SO JUST FLASH THE
    // SCREEN WHITE AND RETURN
    if (modeswitches > 2) {
        modeswitches = 0;
        SelectObject(framedc, GetStockObject(WHITE_BRUSH));
        SelectObject(framedc, GetStockObject(WHITE_PEN));
        Rectangle(framedc, 0, 0, 560, 384);
        return;
    }
    modeswitches = 0;

    hiresauxptr  = MemGetAuxPtr(0x2000 << (displaypage2 ? 1 : 0));
    hiresmainptr = MemGetMainPtr(0x2000 << (displaypage2 ? 1 : 0));
    textauxptr   = MemGetAuxPtr(0x400 << (displaypage2 ? 1 : 0));
    textmainptr  = MemGetMainPtr(0x400 << (displaypage2 ? 1 : 0));

    int y      = 0;
    int ypixel = 0;
    for (; y < 20; y++, ypixel += 16) {
        int offset = ((y & 7) << 7) + ((y >> 3) * 40);
        for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14)
            updateupper(x, y, xpixel, ypixel, offset + x);
    }
    for (; y < 24; y++, ypixel += 16) {
        int offset = ((y & 7) << 7) + ((y >> 3) * 40);
        for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14)
            updatelower(x, y, xpixel, ypixel, offset + x);
    }

    BitBlt(
        framedc,
        0, 0,
        560, 384,
        devicedc,
        0, 0,
        SRCCOPY
    );

    GdiFlush();
    SetLastDrawnImage();
    redrawfull = FALSE;

    if ((mode == MODE_PAUSED) || (mode == MODE_STEPPING))
        VideoDisplayMode(TRUE);
}

//===========================================================================
void VideoReinitialize() {
    InitializeSourceLookup();
}

//===========================================================================
void VideoReleaseFrameDC() {
    if (framedc) {
        FrameReleaseDC(framedc);
        framedc = (HDC)0;
    }
}

//===========================================================================
void VideoResetState() {
    charoffs     = 0;
    displaypage2 = FALSE;
    redrawfull   = FALSE;
    UpdateVideoMode(VF_TEXT);
}

//===========================================================================
BYTE VideoSetMode(WORD, BYTE address, BYTE write, BYTE) {
    DWORD oldpage2  = SW_PAGE2();
    DWORD oldmodeex = charoffs | (vidmode & ~(VF_MASK2 | VF_PAGE2));

    DWORD newmode = vidmode;
    switch (address) {
        case 0x00: newmode &= ~VF_MASK2;   break;
        case 0x01: newmode |= VF_MASK2;    break;
        case 0x0C: newmode &= ~VF_80COL;   break;
        case 0x0D: newmode |= VF_80COL;    break;
        case 0x0E: charoffs = 0;           break;
        case 0x0F: charoffs = 0x100;       break;
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

    DWORD newmodeex = charoffs | (vidmode & ~(VF_MASK2 | VF_PAGE2));
    if (oldmodeex != newmodeex) {
        if ((SW_80COL() != 0) == (SW_DHIRES() != 0))
            modeswitches++;
        graphicsmode = !SW_TEXT();
        redrawfull = TRUE;
    }

    if (fullspeed && oldpage2 && !SW_PAGE2()) {
        static DWORD lasttime = 0;
        DWORD currtime = GetTickCount();
        if (currtime - lasttime >= 20)
            lasttime = currtime;
        else
            oldpage2 = SW_PAGE2();
    }

    if (oldpage2 != SW_PAGE2()) {
        static DWORD lastrefresh = 0;
        if ((displaypage2 && !SW_PAGE2()) || !behind) {
            displaypage2 = (SW_PAGE2() != 0);
            if (!redrawfull) {
                VideoRefreshScreen();
                hasrefreshed = TRUE;
                lastrefresh  = emulmsec;
            }
        }
        else if (!SW_PAGE2() && !redrawfull && (emulmsec - lastrefresh >= 20)) {
            displaypage2 = FALSE;
            VideoRefreshScreen();
            hasrefreshed = TRUE;
            lastrefresh  = emulmsec;
        }
        lastpageflip = emulmsec;
    }

    if (address == 0x50)
        return VideoCheckVbl(0, 0, 0, 0);
    else
        return MemReturnRandomData(TRUE);
}

//===========================================================================
void VideoUpdateVbl(DWORD cycles, BOOL nearrefresh) {
    if (vblcounter)
        vblcounter -= MIN(vblcounter, cycles >> 6);
    else if (!nearrefresh)
        vblcounter = 250;
}
