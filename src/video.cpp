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

typedef void (* fblt)(int destx, int desty, int xsize, int ysize, int sourcex, int sourcey);
typedef BOOL (* fupdate)(int x, int y, int xpixel, int ypixel, int offset);

BOOL graphicsmode = FALSE;

static fblt          bltfunc;
static BYTE          celldirty[40][32];
static HBITMAP       devicebitmap;
static HDC           devicedc;
static LPBYTE        framebufferbits;
static LPBYTE        framebufferdibits;
static LPBITMAPINFO  framebufferinfo;
static LPBYTE        frameoffsettable[384];
static LPBYTE        hiresauxptr;
static LPBYTE        hiresmainptr;
static HANDLE        logofile;
static HANDLE        logomap;
static LPBITMAPINFO  logoptr;
static LPBYTE        logoview;
static HPALETTE      palette;
static LPBYTE        sourcebits;
static LPBYTE        sourceoffsettable[512];
static LPBYTE        textauxptr;
static LPBYTE        textmainptr;

static int       bitsperpixel    = 0;
static int       charoffs        = 0;
static BOOL      displaypage2    = FALSE;
static HDC       framedc         = (HDC)0;
static BOOL      hasrefreshed    = FALSE;
static DWORD     lastpageflip    = 0;
static DWORD     modeswitches    = 0;
static int       pixelbits       = 0;
static int       pixelformat     = 0;
static BOOL      rebuiltsource   = FALSE;
static BOOL      redrawfull      = TRUE;
static int       srcpixelbits    = 0;
static int       srcpixelformat  = 0;
static BOOL      usingdib        = FALSE;
static DWORD     vblcounter      = 0;
static DWORD     videocompatible = 1;
static HFONT     videofont       = (HFONT)0;
static LPBYTE    vidlastmem      = NULL;
static DWORD     vidmode         = VF_TEXT;

static const COLORREF colorval16[16] = {
    0x000000, 0x0000FF, 0x800000, 0xFF00FF,
    0x008000, 0x808080, 0xFF0000, 0xFFFF00,
    0x008080, 0x0080FF, 0xC0C0C0, 0x8000FF,
    0x00FF00, 0x00FFFF, 0x80FF00, 0xFFFFFF,
};

static BOOL LoadSourceImages();
static void SaveSourceImages();

//===========================================================================
static void BitBlt104(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPBYTE destptr   = frameoffsettable[desty] + (destx >> 1);
    LPBYTE sourceptr = sourceoffsettable[sourcey] + (sourcex >> 1);
    int bytesleft;
    while (ysize--) {
        bytesleft = xsize >> 1;
        while (bytesleft--)
            * (destptr + bytesleft) = *(sourceptr + bytesleft);
        destptr += 280;
        sourceptr += (SRCOFFS_TOTAL >> 1);
    }
}

//===========================================================================
static void BitBlt108(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPBYTE destptr   = frameoffsettable[desty] + destx;
    LPBYTE sourceptr = sourceoffsettable[sourcey] + sourcex;
    int bytesleft;
    while (ysize--) {
        bytesleft = xsize;
        while (bytesleft & 3) {
            --bytesleft;
            *(destptr + bytesleft) = *(sourceptr + bytesleft);
        }
        while (bytesleft) {
            bytesleft -= 4;
            *(LPDWORD)(destptr + bytesleft) = *(LPDWORD)(sourceptr + bytesleft);
        }
        destptr += 560;
        sourceptr += SRCOFFS_TOTAL;
    }
}

//===========================================================================
static void BitBlt108d(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPBYTE destptr   = frameoffsettable[desty] + destx;
    LPBYTE sourceptr = sourceoffsettable[sourcey] + sourcex;
    int bytesleft;
    while (ysize--) {
        bytesleft = xsize;
        while (bytesleft & 3) {
            --bytesleft;
            *(destptr + bytesleft) = *(sourceptr + bytesleft);
        }
        while (bytesleft) {
            bytesleft -= 4;
            *(LPDWORD)(destptr + bytesleft) = *(LPDWORD)(sourceptr + bytesleft);
        }
        destptr -= 560;
        sourceptr += SRCOFFS_TOTAL;
    }
}

//===========================================================================
static void BitBlt110(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPWORD destptr   = ((LPWORD)(frameoffsettable[desty])) + destx;
    LPWORD sourceptr = ((LPWORD)(sourceoffsettable[sourcey])) + sourcex;
    int pixelsleft;
    while (ysize--) {
        pixelsleft = xsize;
        while (pixelsleft--)
            * (destptr + pixelsleft) = *(sourceptr + pixelsleft);
        destptr += 560;
        sourceptr += SRCOFFS_TOTAL;
    }
}

//===========================================================================
static void BitBlt110d(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPWORD destptr   = ((LPWORD)(frameoffsettable[desty])) + destx;
    LPWORD sourceptr = ((LPWORD)(sourceoffsettable[sourcey])) + sourcex;
    int pixelsleft;
    while (ysize--) {
        pixelsleft = xsize;
        while (pixelsleft--)
            * (destptr + pixelsleft) = *(sourceptr + pixelsleft);
        destptr -= 560;
        sourceptr += SRCOFFS_TOTAL;
    }
}

//===========================================================================
static void BitBlt118(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPBYTE destptr   = frameoffsettable[desty] + (destx * 3);
    LPBYTE sourceptr = sourceoffsettable[sourcey] + (sourcex * 3);
    xsize *= 3;
    int pixelsleft;
    while (ysize--) {
        pixelsleft = xsize;
        while (pixelsleft >= 4) {
            pixelsleft -= 4;
            *(LPDWORD)(destptr + pixelsleft) = *(LPDWORD)(sourceptr + pixelsleft);
        }
        while (pixelsleft--)
            *(destptr + pixelsleft) = *(sourceptr + pixelsleft);
        destptr += 560 * 3;
        sourceptr += SRCOFFS_TOTAL * 3;
    }
}

//===========================================================================
static void BitBlt118d(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPBYTE destptr   = frameoffsettable[desty] + (destx * 3);
    LPBYTE sourceptr = sourceoffsettable[sourcey] + (sourcex * 3);
    xsize *= 3;
    int pixelsleft;
    while (ysize--) {
        pixelsleft = xsize;
        while (pixelsleft >= 4) {
            pixelsleft -= 4;
            *(LPDWORD)(destptr + pixelsleft) = *(LPDWORD)(sourceptr + pixelsleft);
        }
        while (pixelsleft--)
            * (destptr + pixelsleft) = *(sourceptr + pixelsleft);
        destptr -= 560 * 3;
        sourceptr += SRCOFFS_TOTAL * 3;
    }
}

//===========================================================================
static void BitBlt120(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPDWORD destptr   = ((LPDWORD)(frameoffsettable[desty])) + destx;
    LPDWORD sourceptr = ((LPDWORD)(sourceoffsettable[sourcey])) + sourcex;
    int pixelsleft;
    while (ysize--) {
        pixelsleft = xsize;
        while (pixelsleft--)
            destptr[pixelsleft] = sourceptr[pixelsleft];
        destptr += 560;
        sourceptr += SRCOFFS_TOTAL;
    }
}

//===========================================================================
static void BitBlt120d(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPDWORD destptr   = ((LPDWORD)(frameoffsettable[desty])) + destx;
    LPDWORD sourceptr = ((LPDWORD)(sourceoffsettable[sourcey])) + sourcex;
    int pixelsleft;
    while (ysize--) {
        pixelsleft = xsize;
        while (pixelsleft--)
            destptr[pixelsleft] = sourceptr[pixelsleft];
        destptr -= 560;
        sourceptr += SRCOFFS_TOTAL;
    }
}

//===========================================================================
static void BitBlt401(
    int destx,
    int desty,
    int xsize,
    int ysize,
    int sourcex,
    int sourcey
) {
    LPBYTE destptr   = (LPBYTE)(frameoffsettable[desty] + (destx >> 3));
    LPBYTE sourceptr = (LPBYTE)(sourceoffsettable[sourcey] + (sourcex >> 3));
    ysize <<= 2;
    destx &= 7;
    DWORD mask = 0xFFFFFFFF;
    mask >>= (32 - xsize);
    mask <<= (32 - xsize) - destx;
    while (ysize--) {
        BYTE source = 0;
        BYTE bytemask = 0;
        int  offset = 0;
        while (offset <= ((xsize + destx - 1) >> 3)) {
            bytemask = (BYTE)(mask >> ((3 - offset) << 3));
            source |= *(sourceptr + offset) >> destx;
            *(destptr + offset) = (source & bytemask)
                | (*(destptr + offset) & ~bytemask);
            source = *(sourceptr + offset++) << (8 - destx);
        }
        destptr += 70;
        sourceptr += (SRCOFFS_TOTAL >> 3);
    }
}

//===========================================================================
static void CheckPixel(
    int         x,
    int         y,
    COLORREF    expected,
    BOOL *      success,
    char *      interference
) {
    if (optmonochrome) {
        if (y & 1)
            expected = expected ? 0x00FF00 : 0;
        else
            expected = 0;
    }
    if (GetPixel(framedc, x, y) != expected)
        *success = FALSE;
    POINT pt = { x + VIEWPORTX,y + VIEWPORTY };
    ClientToScreen(framewindow, &pt);
    HWND window = WindowFromPoint(pt);
    if (window != framewindow) {
        GetWindowText(window, interference, 63);
        interference[63] = 0;
    }
}

//===========================================================================
static void ConvertToBottomUp8() {
    int     linesleft = 384;
    LPDWORD sourceptr = (LPDWORD)framebufferbits;
    LPDWORD destptr = (LPDWORD)(framebufferdibits + 383 * 560);
    while (linesleft--) {
        int loop = 140;
        while (loop--)
            *destptr++ = *sourceptr++;
        destptr -= 280;
    }
}

//===========================================================================
static void CreateIdentityPalette(RGBQUAD * srctable, RGBQUAD * rgbtable) {
    HWND window = GetDesktopWindow();
    HDC  dc     = GetDC(window);
    int  colors = GetDeviceCaps(dc, SIZEPALETTE);
    int  system = GetDeviceCaps(dc, NUMCOLORS);

    // IF WE ARE IN A PALETTIZED VIDEO MODE, CREATE AN IDENTITY PALETTE
    if ((GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) && (colors <= 256)) {
        SetSystemPaletteUse(dc, SYSPAL_NOSTATIC);
        SetSystemPaletteUse(dc, SYSPAL_STATIC);

        // CREATE A PALETTE ENTRY ARRAY
        int palsize = sizeof(LOGPALETTE) + 256 * sizeof(PALETTEENTRY);
        LOGPALETTE * paldata = (LOGPALETTE *)new BYTE[palsize];
        paldata->palVersion    = 0x300;
        paldata->palNumEntries = colors;
        GetSystemPaletteEntries(dc, 0, colors, paldata->palPalEntry);

        // FILL IN THE PALETTE ENTRIES
        {
            int destindex     = 0;
            int srcindex      = 0;
            int halftoneindex = 0;

            // COPY THE SYSTEM PALETTE ENTRIES AT THE BEGINNING OF THE PALETTE
            for (; destindex < system / 2; destindex++)
                paldata->palPalEntry[destindex].peFlags = 0;

            // FILL IN THE MIDDLE PORTION OF THE PALETTE WITH OUR OWN COLORS
            for (; destindex < colors - system / 2; destindex++) {
                if (srctable) {

                    // IF THIS PALETTE ENTRY IS NEEDED FOR THE LOGO, COPY IN THE LOGO
                    // COLOR
                    if ((srctable + srcindex)->rgbRed &&
                        (srctable + srcindex)->rgbGreen &&
                        (srctable + srcindex)->rgbBlue) {
                        paldata->palPalEntry[destindex].peRed = (srctable + srcindex)->rgbRed;
                        paldata->palPalEntry[destindex].peGreen = (srctable + srcindex)->rgbGreen;
                        paldata->palPalEntry[destindex].peBlue = (srctable + srcindex)->rgbBlue;
                    }

                    // OTHERWISE, ADD A HALFTONING COLOR, SO THAT OTHER APPLICATIONS
                    // RUNNING IN THE BACKGROUND WILL HAVE SOME REASONABLE COLORS TO
                    // USE
                    else {
                        static BYTE halftonetable[6] = { 32,64,96,160,192,224 };
                        paldata->palPalEntry[destindex].peRed = halftonetable[halftoneindex % 6];
                        paldata->palPalEntry[destindex].peGreen = halftonetable[halftoneindex / 6 % 6];
                        paldata->palPalEntry[destindex].peBlue = halftonetable[halftoneindex / 36 % 6];
                        ++halftoneindex;
                    }

                    ++srcindex;
                }
                paldata->palPalEntry[destindex].peFlags = PC_NOCOLLAPSE;
            }

            // COPY THE SYSTEM PALETTE ENTRIES AT THE END OF THE PALETTE
            for (; destindex < colors; destindex++)
                paldata->palPalEntry[destindex].peFlags = 0;

        }

        // FILL THE RGB TABLE WITH COLORS FROM OUR PALETTE
        if (rgbtable) {
            for (int loop = 0; loop < colors; loop++) {
                (rgbtable + loop)->rgbRed = paldata->palPalEntry[loop].peRed;
                (rgbtable + loop)->rgbGreen = paldata->palPalEntry[loop].peGreen;
                (rgbtable + loop)->rgbBlue = paldata->palPalEntry[loop].peBlue;
            }
        }

        // CREATE THE PALETTE
        palette = CreatePalette(paldata);

        delete[] paldata;
    }

    // OTHERWISE, FILL THE RGB TABLE WITH THE STANDARD WINDOWS COLORS
    else {
#define SETRGBENTRY(a,b,c,d) (rgbtable+(a))->rgbRed   = (b); \
                             (rgbtable+(a))->rgbGreen = (c); \
                             (rgbtable+(a))->rgbBlue  = (d);
        SETRGBENTRY(0x00, 0x00, 0x00, 0x00);
        SETRGBENTRY(0x01, 0x80, 0x00, 0x00);
        SETRGBENTRY(0x02, 0x00, 0x80, 0x00);
        SETRGBENTRY(0x03, 0x80, 0x80, 0x00);
        SETRGBENTRY(0x04, 0x00, 0x00, 0x80);
        SETRGBENTRY(0x05, 0x80, 0x00, 0x80);
        SETRGBENTRY(0x06, 0x00, 0x80, 0x80);
        SETRGBENTRY(0x07, 0xC0, 0xC0, 0xC0);
        SETRGBENTRY(0x08, 0x80, 0x80, 0x80);
        SETRGBENTRY(0x09, 0xFF, 0x00, 0x00);
        SETRGBENTRY(0x0A, 0x00, 0xFF, 0x00);
        SETRGBENTRY(0x0B, 0xFF, 0xFF, 0x00);
        SETRGBENTRY(0x0C, 0x00, 0x00, 0xFF);
        SETRGBENTRY(0x0D, 0xFF, 0x00, 0xFF);
        SETRGBENTRY(0x0E, 0x00, 0xFF, 0xFF);
        SETRGBENTRY(0x0F, 0xFF, 0xFF, 0xFF);
        SETRGBENTRY(0xF8, 0x80, 0x80, 0x80);
        SETRGBENTRY(0xF9, 0xFF, 0x00, 0x00);
        SETRGBENTRY(0xFA, 0x00, 0xFF, 0x00);
        SETRGBENTRY(0xFB, 0xFF, 0xFF, 0x00);
        SETRGBENTRY(0xFC, 0x00, 0x00, 0xFF);
        SETRGBENTRY(0xFD, 0xFF, 0x00, 0xFF);
        SETRGBENTRY(0xFE, 0x00, 0xFF, 0xFF);
        SETRGBENTRY(0xFF, 0xFF, 0xFF, 0xFF);
#undef SETRGBENTRY
    }

    ReleaseDC(window, dc);
}

//===========================================================================
static void DrawDHiResSource(HDC dc) {
    for (int value = 0; value < 256; value++) {
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 2; y++) {
                int color = (x < 4) ? (value & 0xF) : (value >> 4);
                SetPixel(dc, SRCOFFS_DHIRES + x, (value << 1) + y, colorval16[color]);
            }
        }
    }
}

//===========================================================================
static void DrawLoResSource(HDC dc) {
    for (int color = 0; color < 16; color++) {
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++)
                SetPixelV(dc, SRCOFFS_LORES + x, (color << 4) + y, colorval16[color]);
        }
    }
}

//===========================================================================
static void DrawHiResSource(HDC dc) {
    static const COLORREF colorval[6] = {
        0xFF00FF, 0xFF0000, 0x00FF00, 0x0080FF, 0x000000, 0xFFFFFF,
    };

    for (int column = 0; column < 4; ++column) {
        BOOL pixelon[9];
        pixelon[0] = (column & 2) != 0;
        pixelon[8] = (column & 1) != 0;

        const int coloffs = column << 5;
        for (int byteval = 0; byteval < 256; ++byteval) {
            for (int pixel = 1, mask = 1; pixel < 8; ++pixel, mask <<= 1)
                pixelon[pixel] = ((byteval & mask) != 0);
            const int hibit = (byteval & 0x80) ? 1 : 0;
            int x = 0;
            int y = byteval << 1;
            while (x < 28) {
                const int adj = (x >= 14 ? 2 : 0);
                const int odd = (x >= 14 ? 1 : 0);
                for (int pixel = 1; pixel < 8; pixel++, x += 2) {
                    int color = 4;
                    if (pixelon[pixel]) {
                        if (pixelon[pixel - 1] || pixelon[pixel + 1])
                            color = 5;
                        else
                            color = ((odd ^ !(pixel & 1)) << 1) | hibit;
                    }
                    else if (pixelon[pixel - 1] && pixelon[pixel + 1]) {
                        color = ((odd ^ (pixel & 1)) << 1) | hibit;
                    }

                    SetPixelV(dc, SRCOFFS_HIRES + coloffs + x + adj + 0, y + 0, colorval[color]);
                    SetPixelV(dc, SRCOFFS_HIRES + coloffs + x + adj + 1, y + 0, colorval[color]);
                    SetPixelV(dc, SRCOFFS_HIRES + coloffs + x + adj + 0, y + 1, colorval[color]);
                    SetPixelV(dc, SRCOFFS_HIRES + coloffs + x + adj + 1, y + 1, colorval[color]);
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
static void InitializeSourceImages() {
    if (!LoadSourceImages()) {
        HWND    window = GetDesktopWindow();
        HDC     dc     = GetDC(window);
        HDC     memdc  = CreateCompatibleDC(dc);
        HBRUSH  brush  = CreateSolidBrush(0x00FF00);
        HBITMAP bitmap = CreateBitmap(
            SRCOFFS_TOTAL,
            512,
            srcpixelformat >> 8,
            srcpixelformat & 0xFF,
            NULL
        );
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
            int loop = 512;
            while ((loop -= 2) >= 0) {
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
        GetBitmapBits(bitmap, SRCOFFS_TOTAL * 64 * srcpixelbits, sourcebits);
        DeleteObject(bitmap);
        DeleteObject(brush);
        SaveSourceImages();
        rebuiltsource = TRUE;
    }
}

//===========================================================================
static BOOL LoadSourceImages() {
    if (!sourcebits)
        return FALSE;

    char filename[MAX_PATH];
    StrPrintf(
        filename,
        ARRSIZE(filename),
        "%sVid%03X%s.dat",
        progdir,
        (unsigned)srcpixelformat & 0xFFF,
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
        DWORD bytestoread = SRCOFFS_TOTAL * 64 * srcpixelbits;
        DWORD bytesread = 0;
        (void)ReadFile(file, sourcebits, bytestoread, &bytesread, NULL);
        CloseHandle(file);
        return (bytesread == bytestoread);
    }

    return FALSE;
}

//===========================================================================
static void SaveSourceImages() {
    if (!sourcebits)
        return;

    char filename[MAX_PATH];
    StrPrintf(
        filename,
        ARRSIZE(filename),
        "%sVid%03X%s.dat",
        progdir,
        (unsigned)(srcpixelformat & 0xFFF),
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
        DWORD bytestowrite = SRCOFFS_TOTAL * 64 * srcpixelbits;
        DWORD byteswritten = 0;
        WriteFile(file, sourcebits, bytestowrite, &byteswritten, NULL);
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
static BOOL Update40ColCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE ch = textmainptr[offset];
    if ((ch != vidlastmem[offset + 0x400]) || redrawfull) {
        bltfunc(
            xpixel,
            ypixel,
            14,
            16,
            SRCOFFS_40COL + ((ch & 0x0F) << 4),
            (ch & 0xF0) + charoffs
        );
        return TRUE;
    }
    return FALSE;
}

//===========================================================================
static BOOL Update80ColCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE auxval  = textauxptr[offset];
    BYTE mainval = textmainptr[offset];
    if ((auxval != vidlastmem[offset]) ||
        (mainval != vidlastmem[offset + 0x400]) ||
        redrawfull)
    {
        bltfunc(
            xpixel,
            ypixel,
            7,
            16,
            SRCOFFS_80COL + ((auxval & 15) << 3),
            ((auxval >> 4) << 4) + charoffs
        );
        bltfunc(
            xpixel + 7,
            ypixel,
            7,
            16,
            SRCOFFS_80COL + ((mainval & 15) << 3),
            ((mainval >> 4) << 4) + charoffs
        );
        return TRUE;
    }
    return FALSE;
}

//===========================================================================
static BOOL UpdateDHiResCell(int x, int y, int xpixel, int ypixel, int offset) {
    BOOL dirty   = FALSE;
    int  yoffset = 0;
    while (yoffset < 0x2000) {
        BYTE auxval  = hiresauxptr[offset + yoffset];
        BYTE mainval = hiresmainptr[offset + yoffset];
        BOOL draw = (auxval != vidlastmem[offset + yoffset]) ||
            (mainval != vidlastmem[offset + yoffset + 0x2000]) ||
            redrawfull;
        if (offset & 1) {
            BYTE thirdval = hiresmainptr[offset + yoffset - 1];
            draw |= (thirdval != vidlastmem[offset + yoffset + 0x1FFF]);
            if (draw) {
                int value1 = ((auxval & 0x3F) << 2) | ((thirdval & 0x60) >> 5);
                int value2 = ((mainval & 0x7F) << 1) | ((auxval & 0x40) >> 6);
                bltfunc(
                    xpixel - 2,
                    ypixel + (yoffset >> 9),
                    8,
                    2,
                    SRCOFFS_DHIRES,
                    value1 << 1
                );
                bltfunc(
                    xpixel + 6,
                    ypixel + (yoffset >> 9),
                    8,
                    2,
                    SRCOFFS_DHIRES,
                    value2 << 1
                );
            }
        }
        else {
            BYTE thirdval = hiresauxptr[offset + yoffset + 1];
            draw |= (thirdval != vidlastmem[offset + yoffset + 1]);
            if (draw) {
                int value1 = (auxval & 0x7F) | ((mainval & 1) << 7);
                int value2 = ((mainval & 0x7E) >> 1) | ((thirdval & 3) << 6);
                bltfunc(
                    xpixel,
                    ypixel + (yoffset >> 9),
                    8,
                    2,
                    SRCOFFS_DHIRES,
                    value1 << 1
                );
                bltfunc(
                    xpixel + 8,
                    ypixel + (yoffset >> 9),
                    8,
                    2,
                    SRCOFFS_DHIRES,
                    value2 << 1
                );
            }
        }
        if (draw)
            dirty = TRUE;
        yoffset += 0x400;
    }
    return dirty;
}

//===========================================================================
static BOOL UpdateLoResCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE val = textmainptr[offset];
    if ((val != vidlastmem[offset + 0x400]) || redrawfull) {
        bltfunc(
            xpixel,
            ypixel,
            14,
            8,
            SRCOFFS_LORES,
            (int)((val & 0xF) << 4)
        );
        bltfunc(
            xpixel,
            ypixel + 8,
            14,
            8,
            SRCOFFS_LORES,
            (int)(val & 0xF0)
        );
        return TRUE;
    }
    return FALSE;
}

//===========================================================================
static BOOL UpdateHiResCell(int x, int y, int xpixel, int ypixel, int offset) {
    BOOL dirty   = FALSE;
    int  yoffset = 0;
    while (yoffset < 0x2000) {
        BYTE prev = (x > 0) ? hiresmainptr[offset + yoffset - 1] : 0;
        BYTE curr = hiresmainptr[offset + yoffset];
        BYTE next = (x < 39) ? hiresmainptr[offset + yoffset + 1] : 0;
        if ((curr != vidlastmem[offset + yoffset + 0x2000]) ||
            ((x > 0)  && ((prev & 64) != (vidlastmem[offset + yoffset + 0x1FFF] & 64))) ||
            ((x < 39) && ((next & 1)  != (vidlastmem[offset + yoffset + 0x2001] & 1)))  ||
            redrawfull)
        {
            const int c1 = (x > 0  && (prev & 64)) ? 1 : 0;
            const int c2 = (x < 39 && (next & 1))  ? 1 : 0;
            const int coloffs = c1 << 6 | c2 << 5;
            bltfunc(
                xpixel,
                ypixel + (yoffset >> 9),
                14,
                2,
                SRCOFFS_HIRES + coloffs + ((x & 1) << 4),
                (int)curr << 1
            );
            dirty = TRUE;
        }
        yoffset += 0x400;
    }
    return dirty;
}


//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BOOL VideoApparentlyDirty() {
    // TODO: Handle mixed mode and double hires dirty page checks properly.
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
        vidmode      = VF_TEXT;
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
        vidmode = VF_HIRES;
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
            (unsigned)pixelformat,
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

    delete[] framebufferinfo;
    delete[] sourcebits;
    delete[] vidlastmem;
    framebufferinfo = NULL;
    sourcebits = NULL;
    vidlastmem = NULL;

    if (!usingdib) {
        delete[] framebufferbits;
        delete[] framebufferdibits;
        framebufferbits = NULL;
        framebufferdibits = NULL;
    }

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

    // DRAW THE LOGO, USING SETDIBITSTODEVICE() IF IT IS AVAILABLE OR
    // BITBLT() IF IT IS NOT
    if (logoptr) {
        int logosize = sizeof(BITMAPINFOHEADER) + ((bitsperpixel <= 4) ? 16 : 256) * sizeof(RGBQUAD);
        int result = SetDIBitsToDevice(
            framedc,
            0,
            0,
            logoptr->bmiHeader.biWidth,
            logoptr->bmiHeader.biHeight,
            0,
            0,
            0,
            logoptr->bmiHeader.biHeight,
            (LPBYTE)logoptr + logosize,
            logoptr,
            (pixelformat == 0x108) ? 2 /* DIB_PAL_INDICES */ : DIB_RGB_COLORS
        );
        if (result == 0) {
            int bitmapsize = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
            BITMAPINFO * info = (BITMAPINFO *)new BYTE[bitmapsize];
            ZeroMemory(info, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
            info->bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
            info->bmiHeader.biWidth    = 560;
            info->bmiHeader.biHeight   = 384;
            info->bmiHeader.biPlanes   = 1;
            info->bmiHeader.biBitCount = ((bitsperpixel <= 4) ? 4 : 8);
            info->bmiHeader.biClrUsed  = ((bitsperpixel <= 4) ? 16 : 256);
            int databytes = (bitsperpixel <= 4) ? 16 * sizeof(RGBQUAD) : 256 * sizeof(RGBQUAD);
            CopyMemory(info->bmiColors, logoptr->bmiColors, databytes);
            SetDIBits(
                devicedc,
                devicebitmap,
                0,
                384,
                (LPBYTE)logoptr + sizeof(BITMAPINFOHEADER) + databytes,
                info,
                DIB_RGB_COLORS
            );
            BitBlt(framedc, 0, 0, 560, 384, devicedc, 0, 0, SRCCOPY);
            delete[] info;
        }
    }

    // DRAW THE VERSION NUMBER
    if (bitsperpixel > 4) {
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
#define  DRAWVERSION(x,y,c)  SetTextColor(framedc,c);   \
                             TextOut(framedc,           \
                                     540 + x, 358 + y,  \
                                     version,           \
                                     StrLen(version));
        if (GetDeviceCaps(framedc, RASTERCAPS) & RC_PALETTE) {
            int offset = GetDeviceCaps(framedc, NUMCOLORS) / 2;
            DRAWVERSION( 1,  1, PALETTEINDEX(2 + offset));
            DRAWVERSION(-1, -1, PALETTEINDEX(122 + offset));
            DRAWVERSION( 0,  0, PALETTEINDEX(32 + offset));
        }
        else {
            DRAWVERSION( 1,  1, 0x6A2136);
            DRAWVERSION(-1, -1, 0xE76BBD);
            DRAWVERSION( 0,  0, 0xD63963);
        }
#undef  DRAWVERSION
        SetTextAlign(framedc, TA_RIGHT | TA_TOP);
        SelectObject(framedc, oldfont);
        DeleteObject(font);
    }

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

    // DETERMINE THE NUMBER OF BITS PER PIXEL USED BY THE CURRENT DEVICE
    if (videocompatible) {
        HWND window = GetDesktopWindow();
        HDC  dc = GetDC(window);
        bitsperpixel = GetDeviceCaps(dc, PLANES) * GetDeviceCaps(dc, BITSPIXEL);
        pixelformat  = (GetDeviceCaps(dc, PLANES) << 8) | GetDeviceCaps(dc, BITSPIXEL);
        ReleaseDC(window, dc);
    }
    else {
        pixelformat  = 0x801;
        bitsperpixel = 8;
    }
    if ((pixelformat == 0x104) || (pixelformat == 0x401) ||
        (pixelformat == 0x110) || (pixelformat == 0x118) ||
        (pixelformat == 0x120))
        srcpixelformat = pixelformat;
    else
        srcpixelformat = 0x108;
    pixelbits = (pixelformat & 0xFF) * (pixelformat >> 8);
    srcpixelbits = (srcpixelformat & 0xFF) * (srcpixelformat >> 8);

    // LOAD THE LOGO
    VideoLoadLogo();

    // CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
    int framebuffersize = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
    framebufferinfo = (BITMAPINFO *)new BYTE[framebuffersize];
    ZeroMemory(framebufferinfo, framebuffersize);
    framebufferinfo->bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    framebufferinfo->bmiHeader.biWidth    = 560;
    framebufferinfo->bmiHeader.biHeight   = 384;
    framebufferinfo->bmiHeader.biPlanes   = srcpixelformat >> 8;
    framebufferinfo->bmiHeader.biBitCount = srcpixelformat & 0xFF;
    framebufferinfo->bmiHeader.biClrUsed  = 256;

    // CREATE AN IDENTITY PALETTE AND FILL IN THE CORRESPONDING COLORS IN
    // THE BITMAPINFO STRUCTURE
    CreateIdentityPalette(
        logoptr ? logoptr->bmiColors : NULL,
        framebufferinfo->bmiColors
    );

    // CREATE A BIT BUFFER FOR THE SOURCE IMAGES
    int sourcebitssize = SRCOFFS_TOTAL * 64 * srcpixelbits + 4;
    sourcebits = new BYTE[sourcebitssize];
    ZeroMemory(sourcebits, sourcebitssize);

    // DETERMINE WHETHER TO USE THE CREATEDIBSECTION() OR SETBITS() METHOD
    // OF BITMAP DATA UPDATING
    if ((pixelformat == 0x108) || (pixelformat == 0x110) || (pixelformat == 0x118) || (pixelformat == 0x120)) {
        usingdib = TRUE;
    }

    // CREATE THE DEVICE DEPENDENT BITMAP AND DEVICE CONTEXT
    {
        HWND window = GetDesktopWindow();
        HDC  dc = GetDC(window);
        if (usingdib) {
            framebufferbits = NULL;
            devicebitmap = CreateDIBSection(
                dc,
                framebufferinfo,
                DIB_RGB_COLORS,
                (LPVOID *)&framebufferbits,
                0,
                0
            );
            usingdib = (devicebitmap && framebufferinfo);
        }
        devicedc = CreateCompatibleDC(dc);
        if (!usingdib)
            devicebitmap = CreateCompatibleBitmap(dc, 560, 384);
        ReleaseDC(window, dc);
        SelectPalette(devicedc, palette, 0);
        RealizePalette(devicedc);
        SelectObject(devicedc, devicebitmap);
    }

    // IF WE ARE NOT USING CREATEDIBSECTION() THEN CREATE BIT BUFFERS FOR THE
    // FRAME BUFFER AND DIB FRAME BUFFER
    if (!usingdib) {
        int buffersize    = 70 * 384 * pixelbits + 4;
        framebufferbits   = new BYTE[buffersize];
        framebufferdibits = new BYTE[buffersize];
        ZeroMemory(framebufferbits, buffersize);
        ZeroMemory(framebufferdibits, buffersize);
    }

    // CREATE OFFSET TABLES FOR EACH SCAN LINE IN THE SOURCE IMAGES AND
    // FRAME BUFFER
    if ((srcpixelformat <= 0x108) || (srcpixelformat >= 0x200)) {
        for (int loop = 0; loop < 512; ++loop) {
            if (loop < 384) {
                frameoffsettable[loop] = framebufferbits
                    + (bitsperpixel == 4 ? 280 : 560)
                    * (usingdib ? (383 - loop) : loop);
            }
            sourceoffsettable[loop] = sourcebits + ((SRCOFFS_TOTAL * loop) >> ((bitsperpixel == 4) ? 1 : 0));
        }
    }
    else {
        const int bytespixel = srcpixelbits >> 3;
        for (int loop = 0; loop < 512; ++loop) {
            if (loop < 384)
                frameoffsettable[loop] = framebufferbits + (560 * bytespixel) * (usingdib ? (383 - loop) : loop);
            sourceoffsettable[loop] = sourcebits + SRCOFFS_TOTAL * bytespixel * loop;
        }
    }

    // DETERMINE WHICH BITBLT FUNCTION TO USE
    switch (srcpixelformat) {
        case 0x104: bltfunc = BitBlt104;                          break;
        case 0x108: bltfunc = usingdib ? BitBlt108d : BitBlt108;  break;
        case 0x110: bltfunc = usingdib ? BitBlt110d : BitBlt110;  break;
        case 0x118: bltfunc = usingdib ? BitBlt118d : BitBlt118;  break;
        case 0x120: bltfunc = usingdib ? BitBlt120d : BitBlt120;  break;
        case 0x401: bltfunc = BitBlt401;                          break;
        default:    bltfunc = NULL;                               break;
    }

    // LOAD THE SOURCE IMAGES FROM DISK, OR DRAW THEM AND TRANSFER THEM
    // INTO THE SOURCE BIT BUFFER
    InitializeSourceImages();

    // RESET THE VIDEO MODE SWITCHES AND THE CHARACTER SET OFFSET
    VideoResetState();

}

//===========================================================================
void VideoLoadLogo() {
    if (logoptr || bitsperpixel <= 4)
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
        logoptr = (LPBITMAPINFO)(logoview + 0x200 + sizeof(BITMAPFILEHEADER));
    else
        logoptr = NULL;
}

//===========================================================================
void VideoRealizePalette(HDC dc) {
    if (!dc) {
        if (!framedc)
            framedc = FrameGetDC();
        dc = framedc;
    }
    SelectPalette(dc, palette, 0);
    RealizePalette(dc);
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

    // CHECK EACH CELL FOR CHANGED BYTES.  REDRAW PIXELS FOR THE CHANGED BYTES
    // IN THE FRAME BUFFER.  MARK CELLS IN WHICH REDRAWING HAS TAKEN PLACE AS
    // DIRTY.
    hiresauxptr  = MemGetAuxPtr(0x2000 << (displaypage2 ? 1 : 0));
    hiresmainptr = MemGetMainPtr(0x2000 << (displaypage2 ? 1 : 0));
    textauxptr   = MemGetAuxPtr(0x400 << (displaypage2 ? 1 : 0));
    textmainptr  = MemGetMainPtr(0x400 << (displaypage2 ? 1 : 0));
    ZeroMemory(celldirty, 40 * 32);
    {
        fupdate updateupper =
            SW_TEXT()
                ? SW_80COL()
                    ? Update80ColCell
                    : Update40ColCell
                : SW_HIRES()
                    ? (SW_DHIRES() && SW_80COL())
                        ? UpdateDHiResCell
                        : UpdateHiResCell
                    : UpdateLoResCell;

        fupdate updatelower =
            SW_MIXED()
                ? SW_80COL()
                    ? Update80ColCell
                    : Update40ColCell
                : updateupper;

        BOOL anydirty = FALSE;
        int  y        = 0;
        int  ypixel   = 0;
        for (; y < 20; y++, ypixel += 16) {
            int offset = ((y & 7) << 7) + ((y >> 3) * 40);
            for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14) {
                celldirty[x][y] = updateupper(x, y, xpixel, ypixel, offset + x);
                anydirty |= (celldirty[x][y] != 0);
            }
        }
        for (; y < 24; y++, ypixel += 16) {
            int offset = ((y & 7) << 7) + ((y >> 3) * 40);
            for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14) {
                celldirty[x][y] = updatelower(x, y, xpixel, ypixel, offset + x);
                anydirty |= (celldirty[x][y] != 0);
            }
        }
        if (!anydirty) {
            SetLastDrawnImage();
            return;
        }
    }

    // CONVERT THE FRAME BUFFER BITS INTO A DEVICE DEPENDENT BITMAP
    if (!usingdib) {
        switch (pixelformat) {

            case 0x104:
            case 0x401:
                SetBitmapBits(devicebitmap, 280 * 384, framebufferbits);
                break;

            case 0x108:
                SetBitmapBits(devicebitmap, 560 * 384, framebufferbits);
                break;

            case 0x110:
                SetBitmapBits(devicebitmap, 560 * 384 * 2, framebufferbits);
                break;

            case 0x118:
                SetBitmapBits(devicebitmap, 560 * 384 * 3, framebufferbits);
                break;

            case 0x120:
                SetBitmapBits(devicebitmap, 560 * 384 * 4, framebufferbits);
                break;

            default:
                ConvertToBottomUp8();
                SetDIBits(
                    devicedc,
                    devicebitmap,
                    0,
                    384,
                    framebufferdibits,
                    framebufferinfo,
                    DIB_RGB_COLORS
                );
                break;

        }
    }

    // COPY DIRTY CELLS FROM THE DEVICE DEPENDENT BITMAP ONTO THE SCREEN
    // IN LONG HORIZONTAL RECTANGLES
    BOOL remainingdirty = FALSE;
    for (int y = 0, ypixel = 0; y < 24; y++, ypixel += 16) {
        int start  = -1;
        int startx = 0;
        for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14) {
            if ((x == 39) && celldirty[x][y]) {
                if (start >= 0) {
                    xpixel += 14;
                    celldirty[x][y] = 0;
                }
                else
                    remainingdirty = TRUE;
            }
            if ((start >= 0) && !celldirty[x][y]) {
                if ((x - startx > 1) || ((x == 39) && (xpixel == 560))) {
                    int height = 1;
                    while ((y + height < 24)
                        && celldirty[startx][y + height]
                        && celldirty[x - 1][y + height]
                        && celldirty[(startx + x - 1) >> 1][y + height])
                    {
                        height++;
                    }
                    BitBlt(
                        framedc,
                        start,
                        ypixel,
                        xpixel - start,
                        height << 4,
                        devicedc,
                        start,
                        ypixel,
                        SRCCOPY
                    );
                    while (height--) {
                        int loop = startx;
                        while (loop < x + (xpixel == 560))
                            celldirty[loop++][y + height] = 0;
                    }
                    start = -1;
                }
                else
                    remainingdirty = TRUE;
                start = -1;
            }
            else if ((start == -1) && celldirty[x][y] && (x < 39)) {
                start = xpixel;
                startx = x;
            }
        }
    }

    // COPY ANY REMAINING DIRTY CELLS FROM THE DEVICE DEPENDENT BITMAP
    // ONTO THE SCREEN IN VERTICAL RECTANGLES
    if (remainingdirty) {
        for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14) {
            int start  = -1;
            for (int y = 0, ypixel = 0; y < 24; y++, ypixel += 16) {
                if ((y == 23) && celldirty[x][y]) {
                    if (start == -1)
                        start = ypixel;
                    ypixel += 16;
                    celldirty[x][y] = 0;
                }
                if ((start >= 0) && !celldirty[x][y]) {
                    BitBlt(
                        framedc,
                        xpixel,
                        start,
                        14,
                        ypixel - start,
                        devicedc,
                        xpixel,
                        start,
                        SRCCOPY
                    );
                    start = -1;
                }
                else if ((start == -1) && celldirty[x][y])
                    start = ypixel;
            }
        }
    }

    GdiFlush();
    SetLastDrawnImage();
    redrawfull = FALSE;

    if ((mode == MODE_PAUSED) || (mode == MODE_STEPPING))
        VideoDisplayMode(TRUE);
}

//===========================================================================
void VideoReinitialize() {
    InitializeSourceImages();
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
    vidmode      = VF_TEXT;
    redrawfull   = FALSE;
}

//===========================================================================
BYTE VideoSetMode(WORD, BYTE address, BYTE write, BYTE) {
    DWORD oldpage2 = SW_PAGE2();
    int   oldvalue = charoffs + (int)(vidmode & ~(VF_MASK2 | VF_PAGE2));
    switch (address) {
        case 0x00: vidmode &= ~VF_MASK2;   break;
        case 0x01: vidmode |= VF_MASK2;    break;
        case 0x0C: vidmode &= ~VF_80COL;   break;
        case 0x0D: vidmode |= VF_80COL;    break;
        case 0x0E: charoffs = 0;           break;
        case 0x0F: charoffs = 256;         break;
        case 0x50: vidmode &= ~VF_TEXT;    break;
        case 0x51: vidmode |= VF_TEXT;     break;
        case 0x52: vidmode &= ~VF_MIXED;   break;
        case 0x53: vidmode |= VF_MIXED;    break;
        case 0x54: vidmode &= ~VF_PAGE2;   break;
        case 0x55: vidmode |= VF_PAGE2;    break;
        case 0x56: vidmode &= ~VF_HIRES;   break;
        case 0x57: vidmode |= VF_HIRES;    break;
        case 0x5E: vidmode |= VF_DHIRES;   break;
        case 0x5F: vidmode &= ~VF_DHIRES;  break;
    }

    if (SW_MASK2())
        vidmode &= ~VF_PAGE2;

    if (oldvalue != charoffs + (int)(vidmode & ~(VF_MASK2 | VF_PAGE2))) {
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
        static DWORD lastrefresh      = 0;
        BOOL         fastvideoslowcpu = FALSE;
        if ((displaypage2 && !SW_PAGE2()) || !behind || fastvideoslowcpu) {
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
void VideoTestCompatibility() {
    if (!mem)
        return;

    // PERFORM THE TEST ONLY ONCE EACH TIME THE EMULATOR IS RUN
    static BOOL firsttime = TRUE;
    if (!firsttime)
        return;
    firsttime = FALSE;

    // DETERMINE THE NAME OF THIS VIDEO MODE
    char modename[64];
    StrPrintf(
        modename,
        ARRSIZE(modename),
        "Video Mode %ux%u %ubpp",
        (unsigned)GetSystemMetrics(SM_CXSCREEN),
        (unsigned)GetSystemMetrics(SM_CYSCREEN),
        (unsigned)bitsperpixel
    );

    // IF WE HAVE ALREADY TESTED THIS VIDEO MODE AND FOUND IT COMPATIBLE,
    // JUST RETURN
    BOOL samemode = TRUE;
    if (!RegLoadValue("Compatibility", modename, &videocompatible)) {
        samemode        = FALSE;
        videocompatible = 1;
    }
    char savedmodename[64] = "";
    RegLoadString("Compatibility", "Last Video Mode", savedmodename, 63);
    if (StrCmp(modename, savedmodename) || rebuiltsource)
        samemode = FALSE;
    if (samemode && videocompatible)
        return;
    if (!samemode)
        videocompatible = 1;

      // ENTER HIRES GRAPHICS MODE AND DRAW TWO PIXELS ON THE SCREEN
    DWORD oldvidmode = vidmode;
    vidmode = VF_HIRES;
    modeswitches = 0;
    mem[0x2000] = 0x02;
    mem[0x3FF7] = 0x20;

    // EXAMINE THE SCREEN TO MAKE SURE THAT THE PIXELS WERE DRAWN CORRECTLY
    char interference[64];
    BOOL success;
    do {
        VideoRedrawScreen();
        success = TRUE;
        interference[0] = 0;
        for (int loop = 1; loop <= 4; ++loop) {
            CheckPixel(loop, 0, ((loop == 2) || (loop == 3)) ? 0x00FF00 : 0, &success, interference);
            CheckPixel(loop, 1, ((loop == 2) || (loop == 3)) ? 0x00FF00 : 0, &success, interference);
            CheckPixel(loop, 2, 0, &success, interference);
        }
        for (int loop = 555; loop <= 558; ++loop) {
            CheckPixel(loop, 381, 0, &success, interference);
            CheckPixel(loop, 382, ((loop == 556) || (loop == 557)) ? 0xFF00FF : 0, &success, interference);
            CheckPixel(loop, 383, ((loop == 556) || (loop == 557)) ? 0xFF00FF : 0, &success, interference);
        }
        if (interference[0]) {
            char buffer[256];
            StrPrintf(
                buffer,
                ARRSIZE(buffer),
                "AppleWin needs to perform a routine test on your "
                "video driver.  Please move %s so that it does not "
                "obscure the emulator window, then click OK.",
                interference
            );
            MessageBox(framewindow, buffer, title, MB_ICONEXCLAMATION);
        }
    } while (interference[0]);

    // RESTORE THE VIDEO MODE
    vidmode = oldvidmode;
    mem[0x2000] = 0;
    mem[0x3FF7] = 0;

    // IF THE RESULTS WERE UNEXPECTED, INFORM THE USER
    if (videocompatible && !success) {
        MessageBox(
            framewindow,
            "AppleWin has detected a compatibility problem with your "
            "video driver.  You may want to use a different video "
            "mode, or obtain an updated driver from your vendor.\n\n"
            "In the meantime, AppleWin will attempt to work around "
            "the problem by limiting its use of the driver.  This "
            "may significantly reduce performance.",
            title,
            MB_ICONEXCLAMATION
        );
    }
    else if (success && !videocompatible) {
        MessageBox(
            framewindow,
            "AppleWin had previously reported a compatibility "
            "problem in your video driver.  The problem seems "
            "to have been resolved.",
            title,
            MB_ICONINFORMATION
        );
    }

    // SAVE THE RESULTS
    videocompatible = success ? 1 : 0;
    RegSaveValue("Compatibility", modename, videocompatible);
    RegSaveString("Compatibility", "Last Video Mode", modename);

    // IF WE DISCOVERED A COMPATIBILITY PROBLEM, THEN REINITIALIZE VIDEO
    if (!videocompatible) {
        VideoDestroy();
        VideoInitialize();
    }
}

//===========================================================================
void VideoUpdateVbl(DWORD cycles, BOOL nearrefresh) {
    if (vblcounter)
        vblcounter -= MIN(vblcounter, cycles >> 6);
    else if (!nearrefresh)
        vblcounter = 250;
}
