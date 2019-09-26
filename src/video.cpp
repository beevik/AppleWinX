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


/****************************************************************************
*
*   Constants and Types
*
***/

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

constexpr DWORD VF_80COL    = 1 << 0;
constexpr DWORD VF_DHIRES   = 1 << 1;
constexpr DWORD VF_HIRES    = 1 << 2;
constexpr DWORD VF_MASK2    = 1 << 3;
constexpr DWORD VF_MIXED    = 1 << 4;
constexpr DWORD VF_PAGE2    = 1 << 5;
constexpr DWORD VF_TEXT     = 1 << 6;

#define  SW_80COL()    ((s_videoMode & VF_80COL)  != 0)
#define  SW_DHIRES()   ((s_videoMode & VF_DHIRES) != 0)
#define  SW_HIRES()    ((s_videoMode & VF_HIRES)  != 0)
#define  SW_MASK2()    ((s_videoMode & VF_MASK2)  != 0)
#define  SW_MIXED()    ((s_videoMode & VF_MIXED)  != 0)
#define  SW_PAGE2()    ((s_videoMode & VF_PAGE2)  != 0)
#define  SW_TEXT()     ((s_videoMode & VF_TEXT)   != 0)

typedef void (* FUpdate)(int x, int y, int xpixel, int ypixel, int offset);

struct PngReadState {
    const uint8_t * data;
    size_t          bytesTotal;
    size_t          bytesRead;
};


/****************************************************************************
*
*   Variables
*
***/

BOOL g_optMonochrome = FALSE;

static HBITMAP    s_deviceBitmap;
static HDC        s_deviceDC;
static uint32_t * s_frameBuffer;
static LPBYTE     s_hiResAuxPtr;
static LPBYTE     s_hiResMainPtr;
static uint32_t * s_sourceLookup;
static LPBYTE     s_textAuxPtr;
static LPBYTE     s_textMainPtr;
static uint32_t * s_screenPixels;
static FUpdate    s_updateLower;
static FUpdate    s_updateUpper;

static DWORD      s_charOffset;
static HDC        s_frameDC;
static int64_t    s_lastRefreshMs;
static DWORD      s_videoMode;

static const COLORREF s_colorLoRes[16] = {
    0x000000, 0x7C0B93, 0xD3351F, 0xFF36BB, // black, red, dkblue, purple
    0x0C7600, 0x7E7E7E, 0xE0A807, 0xFFAC9D, // dkgreen, grey, medblue, ltblue
    0x004C62, 0x1D56F9, 0x7E7E7E, 0xEC81FF, // brown, orange, grey, pink
    0x00C843, 0x16CDDC, 0x84F75D, 0xFFFFFF, // ltgreen, yellow, aqua, white
};

static const COLORREF s_colorHiRes[6] = {
    0xFF36BB, 0xE0A807, 0x00C843,   // purple, blue, green
    0x1D56F9, 0x000000, 0xFFFFFF,   // orange, black, white
};


/****************************************************************************
*
*   Local functions
*
***/

//===========================================================================
static void BitBltCell(
    int dstx,
    int dsty,
    int xsize,
    int ysize,
    int srcx,
    int srcy
) {
    const uint32_t * src = s_sourceLookup + SRC_CX * srcy + srcx;
    uint32_t *       dst = s_screenPixels + SCREEN_CX * dsty + dstx;
    for (int y = 0; y < ysize; ++y) {
        for (int x = xsize - 1; x >= 0; x--)
            dst[x] = src[x];
        dst += SCREEN_CX;
        src += SRC_CX;
    }
}

//===========================================================================
static void DrawDHiResSource(HDC dc) {
    for (int value = 0; value < 256; value++) {
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 2; y++) {
                int color = (x < 4) ? (value & 0xF) : (value >> 4);
                SetPixel(dc, SRCX_DHIRES + x, (value << 1) + y, s_colorLoRes[color]);
            }
        }
    }
}

//===========================================================================
static void DrawLoResSource(HDC dc) {
    for (int color = 0; color < 16; color++) {
        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 16; y++)
                SetPixelV(dc, SRCX_LORES + x, (color << 4) + y, s_colorLoRes[color]);
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

                    SetPixelV(dc, SRCX_HIRES + coloffs + x + adj + 0, y + 0, s_colorHiRes[color]);
                    SetPixelV(dc, SRCX_HIRES + coloffs + x + adj + 1, y + 0, s_colorHiRes[color]);
                    SetPixelV(dc, SRCX_HIRES + coloffs + x + adj + 0, y + 1, s_colorHiRes[color]);
                    SetPixelV(dc, SRCX_HIRES + coloffs + x + adj + 1, y + 1, s_colorHiRes[color]);
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
    HBITMAP bitmap = LoadBitmap(g_instance, "CHARSET40");
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
    HBITMAP bitmap = LoadBitmap(g_instance, "CHARSET40");
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
    PngReadState * state = (PngReadState *)png_get_io_ptr(read);
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
    int size;
    const void * image = ResourceLoad(name, "IMAGE", &size);
    if (!image)
        return FALSE;

    PngReadState state;
    state.data       = (const uint8_t *)image;
    state.bytesTotal = size;
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
        ResourceFree(image);
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
    ResourceFree(image);
    return TRUE;
}

//===========================================================================
static BOOL LoadSourceLookup() {
    if (!s_sourceLookup)
        return FALSE;

    return LoadPngImage(
        g_optMonochrome ? "SOURCE_MONO" : "SOURCE_COLOR",
        SRC_CX,
        SRC_CY,
        s_sourceLookup,
        0
    );
}

//===========================================================================
static void Update40ColCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE ch = s_textMainPtr[offset];

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
        (ch & 0xF0) + s_charOffset
    );
}

//===========================================================================
static void Update80ColCell(int x, int y, int xpixel, int ypixel, int offset) {
    BYTE auxval  = s_textAuxPtr[offset];
    BYTE mainval = s_textMainPtr[offset];
    BitBltCell(
        xpixel, ypixel,
        7, 16,
        SRCX_80COL + ((auxval & 15) << 3),
        ((auxval >> 4) << 4) + s_charOffset
    );
    BitBltCell(
        xpixel + 7, ypixel,
        7, 16,
        SRCX_80COL + ((mainval & 15) << 3),
        ((mainval >> 4) << 4) + s_charOffset
    );
}

//===========================================================================
static void UpdateDHiResCell(int x, int y, int xpixel, int ypixel, int offset) {
    for (int yoffset = 0; yoffset < 0x2000; yoffset += 0x400) {
        BYTE auxval  = s_hiResAuxPtr[offset + yoffset];
        BYTE mainval = s_hiResMainPtr[offset + yoffset];
        BOOL draw    = true;
        if (offset & 1) {
            BYTE thirdval = s_hiResMainPtr[offset + yoffset - 1];
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
            BYTE thirdval = s_hiResAuxPtr[offset + yoffset + 1];
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
    BYTE val = s_textMainPtr[offset];
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
        BYTE prev = (x > 0) ? s_hiResMainPtr[offset + yoffset - 1] : 0;
        BYTE curr = s_hiResMainPtr[offset + yoffset];
        BYTE next = (x < 39) ? s_hiResMainPtr[offset + yoffset + 1] : 0;
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
static void UpdateVideoMode(DWORD newMode) {
    s_videoMode = newMode;

    if (SW_TEXT()) {
        if (SW_80COL())
            s_updateUpper = Update80ColCell;
        else
            s_updateUpper = Update40ColCell;
    }
    else if (SW_HIRES()) {
        if (SW_DHIRES() && SW_80COL())
            s_updateUpper = UpdateDHiResCell;
        else
            s_updateUpper = UpdateHiResCell;
    }
    else {
        s_updateUpper = UpdateLoResCell;
    }

    s_updateLower = s_updateUpper;
    if (SW_MIXED()) {
        if (SW_80COL())
            s_updateLower = Update80ColCell;
        else
            s_updateLower = Update40ColCell;
    }
}

//===========================================================================
static void UpdateVideoScreen(int64_t cycle) {
    if (TimerIsFullSpeed() || EmulatorIsBehind()) {
        // In full-speed mode, refresh the screen at most 4 times per second.
        if (TimerGetMsElapsed() - s_lastRefreshMs >= 250)
            VideoRefreshScreen();
    }
    else
        VideoRefreshScreen();

    SchedulerEnqueue(cycle + CYCLES_PER_SCANLINE * NTSC_SCANLINES, UpdateVideoScreen);
}


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
BYTE VideoCheckMode(WORD, BYTE address, BYTE, BYTE) {
    if (address == 0x7F)
        return MemReturnRandomData(SW_DHIRES());

    bool result;
    switch (address) {
        case 0x1A: result = SW_TEXT();         break;
        case 0x1B: result = SW_MIXED();        break;
        case 0x1D: result = SW_HIRES();        break;
        case 0x1E: result = s_charOffset != 0; break;
        case 0x1F: result = SW_80COL();        break;
        default:   result = false;             break;
    }
    return KeybGetKeycode() | (result ? 0x80 : 0);
}

//===========================================================================
BYTE VideoCheckVbl(WORD pc, BYTE address, BYTE write, BYTE value) {
    int64_t frameCycle = g_cyclesEmulated % (CYCLES_PER_SCANLINE * NTSC_SCANLINES);
    return MemReturnRandomData(frameCycle < (CYCLES_PER_SCANLINE * DISPLAY_LINES));
}

//===========================================================================
void VideoDestroy() {
    VideoReleaseFrameDC();

    delete[] s_sourceLookup;
    s_sourceLookup = NULL;

    DeleteDC(s_deviceDC);
    DeleteObject(s_deviceBitmap);
    s_deviceDC     = 0;
    s_deviceBitmap = 0;
}

//===========================================================================
void VideoDisplayLogo() {
    s_screenPixels = WindowLockPixels();
    LoadPngImage("LOGO", SCREEN_CX, SCREEN_CY, s_screenPixels, LOADPNG_CONVERT_RGB);
    WindowUnlockPixels();

    if (!s_frameDC)
        s_frameDC = FrameGetDC();

    BOOL success = LoadPngImage(
        "LOGO",
        SCREEN_CX,
        SCREEN_CY,
        s_frameBuffer,
        LOADPNG_CONVERT_RGB | LOADPNG_BOTTOM_UP
    );
    if (success) {
        BitBlt(
            s_frameDC,
            0, 0,
            SCREEN_CX, SCREEN_CY,
            s_deviceDC,
            0, 0,
            SRCCOPY
        );
        GdiFlush();
    }

    FrameReleaseDC(s_frameDC);
    s_frameDC = (HDC)0;
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
        GetBitmapBits(bitmap, SRC_CX * SRC_CY * sizeof(uint32_t), s_sourceLookup);
        DeleteObject(bitmap);
        DeleteObject(brush);

        char filename[MAX_PATH];
        StrPrintf(filename, ARRSIZE(filename), "%ssource%s.png", EmulatorGetProgramDirectory(), mono ? "mono" : "color");

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

        const uint32_t * row = s_sourceLookup;
        for (int y = 0; y < SRC_CY; y++, row += SRC_CX)
            png_write_row(write, (png_bytep)row);

        png_write_end(write, info);
        png_destroy_write_struct(&write, &info);
        fclose(fp);
    }
}

//===========================================================================
void VideoInitialize() {
    s_charOffset    = 0;
    s_lastRefreshMs = 0;
    s_videoMode     = 0;

    // THE DEVICE MUST BE 32 BPP AND A SINGLE PLANE
    HWND window       = GetDesktopWindow();
    HDC  dc           = GetDC(window);
    int  planes       = GetDeviceCaps(dc, PLANES);
    int  bitsPerPixel = planes * GetDeviceCaps(dc, BITSPIXEL);
    ReleaseDC(window, dc);
    if (bitsPerPixel != 32 && planes != 1)
        return;

    // CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
    int infoSize = sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
    BITMAPINFO * frameBufferInfo = (BITMAPINFO *)new BYTE[infoSize];
    ZeroMemory(frameBufferInfo, infoSize);
    frameBufferInfo->bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
    frameBufferInfo->bmiHeader.biWidth    = SCREEN_CX;
    frameBufferInfo->bmiHeader.biHeight   = SCREEN_CY;
    frameBufferInfo->bmiHeader.biPlanes   = 1;
    frameBufferInfo->bmiHeader.biBitCount = 32;
    frameBufferInfo->bmiHeader.biClrUsed  = 256;

    // CREATE A BIT BUFFER FOR THE SOURCE LOOKUP
    int sourceDwords = SRC_CX * SRC_CY;
    s_sourceLookup = new uint32_t[sourceDwords];
    ZeroMemory(s_sourceLookup, sourceDwords * sizeof(uint32_t));

    // CREATE THE DEVICE DEPENDENT BITMAP AND DEVICE CONTEXT
    {
        HWND window = GetDesktopWindow();
        HDC  dc = GetDC(window);
        s_frameBuffer = NULL;
        s_deviceBitmap = CreateDIBSection(
            dc,
            frameBufferInfo,
            DIB_RGB_COLORS,
            (LPVOID *)&s_frameBuffer,
            0,
            0
        );
        s_deviceDC = CreateCompatibleDC(dc);
        ReleaseDC(window, dc);
        SelectObject(s_deviceDC, s_deviceBitmap);
    }
    delete[] frameBufferInfo;

    LoadSourceLookup();
    VideoResetState();

    s_lastRefreshMs = TimerGetMsElapsed();
    SchedulerEnqueue(CYCLES_PER_SCANLINE * NTSC_SCANLINES, UpdateVideoScreen);
}

//===========================================================================
void VideoRefreshScreen() {
    if (!s_frameDC)
        s_frameDC = FrameGetDC();

    int shift = SW_PAGE2() ? 1 : 0;
    s_hiResAuxPtr  = MemGetAuxPtr(0x2000 << shift);
    s_hiResMainPtr = MemGetMainPtr(0x2000 << shift);
    s_textAuxPtr   = MemGetAuxPtr(0x400 << shift);
    s_textMainPtr  = MemGetMainPtr(0x400 << shift);
    s_screenPixels = WindowLockPixels();

    int y      = 0;
    int ypixel = 0;
    for (; y < 20; y++, ypixel += 16) {
        int offset = ((y & 7) << 7) + ((y >> 3) * 40);
        for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14)
            s_updateUpper(x, y, xpixel, ypixel, offset + x);
    }
    for (; y < 24; y++, ypixel += 16) {
        int offset = ((y & 7) << 7) + ((y >> 3) * 40);
        for (int x = 0, xpixel = 0; x < 40; x++, xpixel += 14)
            s_updateLower(x, y, xpixel, ypixel, offset + x);
    }

    WindowUnlockPixels();

    s_lastRefreshMs = TimerGetMsElapsed();
}

//===========================================================================
void VideoReinitialize() {
    LoadSourceLookup();
}

//===========================================================================
void VideoReleaseFrameDC() {
    if (s_frameDC) {
        FrameReleaseDC(s_frameDC);
        s_frameDC = (HDC)0;
    }
}

//===========================================================================
void VideoResetState() {
    s_charOffset = 0;
    UpdateVideoMode(VF_TEXT);
}

//===========================================================================
BYTE VideoSetMode(WORD, BYTE address, BYTE write, BYTE) {
    bool  wasPage2 = SW_PAGE2();
    DWORD newMode  = s_videoMode;
    switch (address) {
        case 0x00: newMode &= ~VF_MASK2;   break;
        case 0x01: newMode |=  VF_MASK2;   break;
        case 0x0C: newMode &= ~VF_80COL;   break;
        case 0x0D: newMode |=  VF_80COL;   break;
        case 0x0E: s_charOffset = 0;       break;
        case 0x0F: s_charOffset = 256;     break;
        case 0x50: newMode &= ~VF_TEXT;    break;
        case 0x51: newMode |=  VF_TEXT;    break;
        case 0x52: newMode &= ~VF_MIXED;   break;
        case 0x53: newMode |=  VF_MIXED;   break;
        case 0x54: newMode &= ~VF_PAGE2;   break;
        case 0x55: newMode |=  VF_PAGE2;   break;
        case 0x56: newMode &= ~VF_HIRES;   break;
        case 0x57: newMode |=  VF_HIRES;   break;
        case 0x5E: newMode |=  VF_DHIRES;  break;
        case 0x5F: newMode &= ~VF_DHIRES;  break;
    }
    if (newMode & VF_MASK2)
        newMode &= ~VF_PAGE2;
    UpdateVideoMode(newMode);

    if (wasPage2 != SW_PAGE2())
        VideoRefreshScreen();

    if (address == 0x50)
        return VideoCheckVbl(0, 0, 0, 0);
    else
        return MemReturnRandomData(TRUE);
}
