/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS                    
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "stdhdr.h"
#pragma  hdrstop
#include "applewin.h"

#define  SRCOFFS_40COL    0
#define  SRCOFFS_80COL    256
#define  SRCOFFS_LORES    384
#define  SRCOFFS_HIRES    400
#define  SRCOFFS_DHIRES   528
#define  SRCOFFS_UNUSED   536
#define  SRCOFFS_TOTAL    544

#define  VF_80COL         0x00000001
#define  VF_DHIRES        0x00000002
#define  VF_HIRES         0x00000004
#define  VF_MASK2         0x00000008
#define  VF_MIXED         0x00000010
#define  VF_PAGE2         0x00000020
#define  VF_TEXT          0x00000040

#define  SW_80COL         (vidmode & VF_80COL)
#define  SW_DHIRES        (vidmode & VF_DHIRES)
#define  SW_HIRES         (vidmode & VF_HIRES)
#define  SW_MASK2         (vidmode & VF_MASK2)
#define  SW_MIXED         (vidmode & VF_MIXED)
#define  SW_PAGE2         (vidmode & VF_PAGE2)
#define  SW_TEXT          (vidmode & VF_TEXT)

#ifndef  DIB_PAL_INDICES
#define  DIB_PAL_INDICES  2
#endif

typedef void    (__stdcall *bitblttype     )(int,int,int,int,int,int);
typedef HBITMAP (WINAPI    *createdibtype  )(HDC,CONST BITMAPINFO *,UINT,
                                             VOID **,HANDLE,WORD);
typedef void    (__stdcall *fastbltinittype)(LPBYTE,LPBYTE,LPVOID,LPVOID);
typedef BOOL    (          *updatetype     )(int,int,int,int,int);

bitblttype    bitbltfunc;
BYTE          celldirty[40][32];
createdibtype createdibsection;
HBITMAP       devicebitmap;
HDC           devicedc;
LPBYTE        framebufferbits;
LPBYTE        framebufferdibits;
LPBITMAPINFO  framebufferinfo;
LPBYTE        frameoffsettable[384];
LPBYTE        hiresauxptr;
LPBYTE        hiresmainptr;
HANDLE        logofile;
HANDLE        logomap;
LPBITMAPINFO  logoptr;
LPBYTE        logoview;
HPALETTE      palette;
LPBYTE        sourcebits;
LPBYTE        sourceoffsettable[512];
LPBYTE        textauxptr;
LPBYTE        textmainptr;

int       bitsperpixel    = 0;
int       charoffs        = 0;
BOOL      displaypage2    = 0;
HINSTANCE fastinst        = (HINSTANCE)0;
HDC       framedc         = (HDC)0;
BOOL      graphicsmode    = 0;
BOOL      hasrefreshed    = 0;
DWORD     lastpageflip    = 0;
DWORD     modeswitches    = 0;
int       pixelbits       = 0;
int       pixelformat     = 0;
BOOL      rebuiltsource   = 0;
BOOL      redrawfull      = 1;
int       srcpixelbits    = 0;
int       srcpixelformat  = 0;
BOOL      usingdib        = 0;
DWORD     vblcounter      = 0;
DWORD     videocompatible = 1;
HFONT     videofont       = (HFONT)0;
LPBYTE    vidlastmem      = NULL;
DWORD     vidmode         = VF_TEXT;

BOOL LoadSourceImages ();
void SaveSourceImages ();

//===========================================================================
void __stdcall BitBlt104 (int destx, int desty,
                          int xsize, int ysize,
                          int sourcex, int sourcey) {
  LPBYTE currdestptr   = frameoffsettable[desty]+(destx >> 1);
  LPBYTE currsourceptr = sourceoffsettable[sourcey]+(sourcex >> 1);
  int bytesleft;
  while (ysize--) {
    bytesleft = xsize >> 1;
    while (bytesleft--)
      *(currdestptr+bytesleft) = *(currsourceptr+bytesleft);
    currdestptr   += 280;
    currsourceptr += (SRCOFFS_TOTAL >> 1);
  }
}

//===========================================================================
void __stdcall BitBlt108 (int destx, int desty,
                          int xsize, int ysize,
                          int sourcex, int sourcey) {
  LPBYTE currdestptr   = frameoffsettable[desty]+destx;
  LPBYTE currsourceptr = sourceoffsettable[sourcey]+sourcex;
  int bytesleft;
  while (ysize--) {
    bytesleft = xsize;
    while (bytesleft & 3) {
      --bytesleft;
      *(currdestptr+bytesleft) = *(currsourceptr+bytesleft);
    }
    while (bytesleft) {
      bytesleft -= 4;
      *(LPDWORD)(currdestptr+bytesleft) = *(LPDWORD)(currsourceptr+bytesleft);
    }
    currdestptr   += 560;
    currsourceptr += SRCOFFS_TOTAL;
  }
}

//===========================================================================
void __stdcall BitBlt108d (int destx, int desty,
                           int xsize, int ysize,
                           int sourcex, int sourcey) {
  LPBYTE currdestptr   = frameoffsettable[desty]+destx;
  LPBYTE currsourceptr = sourceoffsettable[sourcey]+sourcex;
  int bytesleft;
  while (ysize--) {
    bytesleft = xsize;
    while (bytesleft & 3) {
      --bytesleft;
      *(currdestptr+bytesleft) = *(currsourceptr+bytesleft);
    }
    while (bytesleft) {
      bytesleft -= 4;
      *(LPDWORD)(currdestptr+bytesleft) = *(LPDWORD)(currsourceptr+bytesleft);
    }
    currdestptr   -= 560;
    currsourceptr += SRCOFFS_TOTAL;
  }
}

//===========================================================================
void __stdcall BitBlt110 (int destx, int desty,
                          int xsize, int ysize,
                          int sourcex, int sourcey) {
  LPWORD currdestptr   = ((LPWORD)(frameoffsettable[desty]))+destx;
  LPWORD currsourceptr = ((LPWORD)(sourceoffsettable[sourcey]))+sourcex;
  int pixelsleft;
  while (ysize--) {
    pixelsleft = xsize;
    while (pixelsleft--)
      *(currdestptr+pixelsleft) = *(currsourceptr+pixelsleft);
    currdestptr   += 560;
    currsourceptr += SRCOFFS_TOTAL;
  }
}

//===========================================================================
void __stdcall BitBlt110d (int destx, int desty,
                           int xsize, int ysize,
                           int sourcex, int sourcey) {
  LPWORD currdestptr   = ((LPWORD)(frameoffsettable[desty]))+destx;
  LPWORD currsourceptr = ((LPWORD)(sourceoffsettable[sourcey]))+sourcex;
  int pixelsleft;
  while (ysize--) {
    pixelsleft = xsize;
    while (pixelsleft--)
      *(currdestptr+pixelsleft) = *(currsourceptr+pixelsleft);
    currdestptr   -= 560;
    currsourceptr += SRCOFFS_TOTAL;
  }
}

//===========================================================================
void __stdcall BitBlt118 (int destx, int desty,
                          int xsize, int ysize,
                          int sourcex, int sourcey) {
  LPBYTE currdestptr   = frameoffsettable[desty]+(destx*3);
  LPBYTE currsourceptr = sourceoffsettable[sourcey]+(sourcex*3);
  xsize *= 3;
  int pixelsleft;
  while (ysize--) {
    pixelsleft = xsize;
    while (pixelsleft >= 4) {
      pixelsleft -= 4;
      *(LPDWORD)(currdestptr+pixelsleft) = *(LPDWORD)(currsourceptr+pixelsleft);
    }
    while (pixelsleft--)
      *(currdestptr+pixelsleft) = *(currsourceptr+pixelsleft);
    currdestptr   += 560*3;
    currsourceptr += SRCOFFS_TOTAL*3;
  }
}

//===========================================================================
void __stdcall BitBlt118d (int destx, int desty,
                           int xsize, int ysize,
                           int sourcex, int sourcey) {
  LPBYTE currdestptr   = frameoffsettable[desty]+(destx*3);
  LPBYTE currsourceptr = sourceoffsettable[sourcey]+(sourcex*3);
  xsize *= 3;
  int pixelsleft;
  while (ysize--) {
    pixelsleft = xsize;
    while (pixelsleft >= 4) {
      pixelsleft -= 4;
      *(LPDWORD)(currdestptr+pixelsleft) = *(LPDWORD)(currsourceptr+pixelsleft);
    }
    while (pixelsleft--)
      *(currdestptr+pixelsleft) = *(currsourceptr+pixelsleft);
    currdestptr   -= 560*3;
    currsourceptr += SRCOFFS_TOTAL*3;
  }
}

//===========================================================================
void __stdcall BitBlt120 (int destx, int desty,
                          int xsize, int ysize,
                          int sourcex, int sourcey) {
  LPDWORD currdestptr   = ((LPDWORD)(frameoffsettable[desty]))+destx;
  LPDWORD currsourceptr = ((LPDWORD)(sourceoffsettable[sourcey]))+sourcex;
  int pixelsleft;
  while (ysize--) {
    pixelsleft = xsize;
    while (pixelsleft--)
      *(currdestptr+pixelsleft) = *(currsourceptr+pixelsleft);
    currdestptr   += 560;
    currsourceptr += SRCOFFS_TOTAL;
  }
}

//===========================================================================
void __stdcall BitBlt120d (int destx, int desty,
                           int xsize, int ysize,
                           int sourcex, int sourcey) {
  LPDWORD currdestptr   = ((LPDWORD)(frameoffsettable[desty]))+destx;
  LPDWORD currsourceptr = ((LPDWORD)(sourceoffsettable[sourcey]))+sourcex;
  int pixelsleft;
  while (ysize--) {
    pixelsleft = xsize;
    while (pixelsleft--)
      *(currdestptr+pixelsleft) = *(currsourceptr+pixelsleft);
    currdestptr   -= 560;
    currsourceptr += SRCOFFS_TOTAL;
  }
}

//===========================================================================
void __stdcall BitBlt401 (int destx, int desty,
                          int xsize, int ysize,
                          int sourcex, int sourcey) {
  LPBYTE currdestptr   = (LPBYTE)(frameoffsettable[desty]+(destx >> 3));
  LPBYTE currsourceptr = (LPBYTE)(sourceoffsettable[sourcey]+(sourcex >> 3));
  ysize <<= 2;
  destx  &= 7;
  DWORD mask = 0xFFFFFFFF;
  mask >>= (32-xsize);
  mask <<= (32-xsize)-destx;
  while (ysize--) {
    BYTE source   = 0;
    BYTE bytemask = 0;
    int  offset   = 0;
    while (offset <= ((xsize+destx-1) >> 3)) {
      bytemask = (BYTE)(mask >> ((3-offset) << 3));
      source  |= *(currsourceptr+offset) >> destx;
      *(currdestptr+offset) = (source & bytemask)
                                | (*(currdestptr+offset) & ~bytemask);
      source = *(currsourceptr+offset++) << (8-destx);
    }
    currdestptr   += 70;
    currsourceptr += (SRCOFFS_TOTAL >> 3);
  }
}

//===========================================================================
void __stdcall BitBlt401b (int destx, int desty,
                           int xsize, int ysize,
                           int sourcex, int sourcey) {
  LPBYTE currdestptr   = (LPBYTE)(frameoffsettable[desty]+(destx >> 3));
  LPBYTE currsourceptr = (LPBYTE)(sourceoffsettable[sourcey]+(sourcex >> 3));
  destx &= 7;
  DWORD mask = 0xFFFFFFFF;
  mask >>= (32-xsize);
  mask <<= (32-xsize)-destx;
  while (ysize--) {
    int yoffset1 = 0;
    int yoffset2 = 0;
    while (yoffset1 < (SRCOFFS_TOTAL >> 1)) {
      BYTE source   = 0;
      BYTE bytemask = 0;
      int  offset   = 0;
      while (offset <= ((xsize+destx-1) >> 3)) {
        bytemask = (BYTE)(mask >> ((3-offset) << 3));
        source  |= *(currsourceptr+yoffset1+offset) >> destx;
        *(currdestptr+yoffset2+offset) =
          (source & bytemask) | (*(currdestptr+yoffset2+offset) & ~bytemask);
        source = *(currsourceptr+yoffset1+offset++) << (8-destx);
      }
      yoffset1 += (SRCOFFS_TOTAL >> 3);
      yoffset2 += 560*384/8;
    }
    currdestptr   += 70;
    currsourceptr += (SRCOFFS_TOTAL >> 1);
  }
}

//===========================================================================
void CheckPixel (int x, int y, COLORREF expected, BOOL *success, LPTSTR interference) {
  if (optmonochrome)
    if (y & 1)
      expected = expected ? 0x00FF00 : 0;
    else
      expected = 0;
  if (GetPixel(framedc,x,y) != expected)
    *success = 0;
  POINT pt = {x+VIEWPORTX,y+VIEWPORTY};
  ClientToScreen(framewindow,&pt);
  HWND window = WindowFromPoint(pt);
  if (window != framewindow) {
    GetWindowText(window,interference,63);
    interference[63] = 0;
  }
}

//===========================================================================
void ConvertToBottomUp8 () {
  int     linesleft = 384;
  LPDWORD sourceptr = (LPDWORD)framebufferbits;
  LPDWORD destptr   = (LPDWORD)(framebufferdibits+383*560);
  while (linesleft--) {
    int loop = 140;
    while (loop--)
      *(destptr++) = *(sourceptr++);
    destptr -= 280;
  }
}

//===========================================================================
void CreateIdentityPalette (RGBQUAD *srctable, RGBQUAD *rgbtable) {
  HWND window = GetDesktopWindow();
  HDC  dc     = GetDC(window);
  int  colors = GetDeviceCaps(dc,SIZEPALETTE);
  int  system = GetDeviceCaps(dc,NUMCOLORS);

  // IF WE ARE IN A PALETTIZED VIDEO MODE, CREATE AN IDENTITY PALETTE
  if ((GetDeviceCaps(dc,RASTERCAPS) & RC_PALETTE) && (colors <= 256)) {
    SetSystemPaletteUse(dc,SYSPAL_NOSTATIC);
    SetSystemPaletteUse(dc,SYSPAL_STATIC);

    // CREATE A PALETTE ENTRY ARRAY
    LOGPALETTE *paldata = (LOGPALETTE *)VirtualAlloc(NULL,
                                                     sizeof(LOGPALETTE)
                                                       +256*sizeof(PALETTEENTRY),
                                                     MEM_COMMIT,
                                                     PAGE_READWRITE);
    paldata->palVersion    = 0x300;
    paldata->palNumEntries = colors;
    GetSystemPaletteEntries(dc,0,colors,paldata->palPalEntry);

    // FILL IN THE PALETTE ENTRIES
    {
      int destindex     = 0;
      int srcindex      = 0;
      int halftoneindex = 0;

      // COPY THE SYSTEM PALETTE ENTRIES AT THE BEGINNING OF THE PALETTE
      for (; destindex < system/2; destindex++)
        paldata->palPalEntry[destindex].peFlags = 0;

      // FILL IN THE MIDDLE PORTION OF THE PALETTE WITH OUR OWN COLORS
      for (; destindex < colors-system/2; destindex++) {
        if (srctable) {

          // IF THIS PALETTE ENTRY IS NEEDED FOR THE LOGO, COPY IN THE LOGO
          // COLOR
          if ((srctable+srcindex)->rgbRed &&
              (srctable+srcindex)->rgbGreen &&
              (srctable+srcindex)->rgbBlue) {
            paldata->palPalEntry[destindex].peRed   = (srctable+srcindex)->rgbRed;
            paldata->palPalEntry[destindex].peGreen = (srctable+srcindex)->rgbGreen;
            paldata->palPalEntry[destindex].peBlue  = (srctable+srcindex)->rgbBlue;
          }

          // OTHERWISE, ADD A HALFTONING COLOR, SO THAT OTHER APPLICATIONS
          // RUNNING IN THE BACKGROUND WILL HAVE SOME REASONABLE COLORS TO
          // USE
          else {
            static BYTE halftonetable[6] = {32,64,96,160,192,224};
            paldata->palPalEntry[destindex].peRed   = halftonetable[halftoneindex    % 6];
            paldata->palPalEntry[destindex].peGreen = halftonetable[halftoneindex/6  % 6];
            paldata->palPalEntry[destindex].peBlue  = halftonetable[halftoneindex/36 % 6];
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
      int loop;
      for (loop = 0; loop < colors; loop++) {
        (rgbtable+loop)->rgbRed   = paldata->palPalEntry[loop].peRed;
        (rgbtable+loop)->rgbGreen = paldata->palPalEntry[loop].peGreen;
        (rgbtable+loop)->rgbBlue  = paldata->palPalEntry[loop].peBlue;
      }
    }

    // CREATE THE PALETTE
    palette = CreatePalette(paldata);

    VirtualFree(paldata,0,MEM_RELEASE);
  }

  // OTHERWISE, FILL THE RGB TABLE WITH THE STANDARD WINDOWS COLORS
  else {
#define SETRGBENTRY(a,b,c,d) (rgbtable+(a))->rgbRed   = (b); \
                             (rgbtable+(a))->rgbGreen = (c); \
                             (rgbtable+(a))->rgbBlue  = (d);
    SETRGBENTRY(0x00,0x00,0x00,0x00);
    SETRGBENTRY(0x01,0x80,0x00,0x00);
    SETRGBENTRY(0x02,0x00,0x80,0x00);
    SETRGBENTRY(0x03,0x80,0x80,0x00);
    SETRGBENTRY(0x04,0x00,0x00,0x80);
    SETRGBENTRY(0x05,0x80,0x00,0x80);
    SETRGBENTRY(0x06,0x00,0x80,0x80);
    SETRGBENTRY(0x07,0xC0,0xC0,0xC0);
    SETRGBENTRY(0x08,0x80,0x80,0x80);
    SETRGBENTRY(0x09,0xFF,0x00,0x00);
    SETRGBENTRY(0x0A,0x00,0xFF,0x00);
    SETRGBENTRY(0x0B,0xFF,0xFF,0x00);
    SETRGBENTRY(0x0C,0x00,0x00,0xFF);
    SETRGBENTRY(0x0D,0xFF,0x00,0xFF);
    SETRGBENTRY(0x0E,0x00,0xFF,0xFF);
    SETRGBENTRY(0x0F,0xFF,0xFF,0xFF);
    SETRGBENTRY(0xF8,0x80,0x80,0x80);
    SETRGBENTRY(0xF9,0xFF,0x00,0x00);
    SETRGBENTRY(0xFA,0x00,0xFF,0x00);
    SETRGBENTRY(0xFB,0xFF,0xFF,0x00);
    SETRGBENTRY(0xFC,0x00,0x00,0xFF);
    SETRGBENTRY(0xFD,0xFF,0x00,0xFF);
    SETRGBENTRY(0xFE,0x00,0xFF,0xFF);
    SETRGBENTRY(0xFF,0xFF,0xFF,0xFF);
#undef SETRGBENTRY
  }

  ReleaseDC(window,dc);
}

//===========================================================================
void DrawDHiResSource (HDC dc) {
  COLORREF colorval[32] = {0x000000,0x800000,0x008000,0xFF0000,
                           0x008080,0xC0C0C0,0x00FF00,0x00FF00,
                           0x0000FF,0xFF00FF,0x808080,0xFFFF00,
                           0x0000FF,0x0000FF,0x00FFFF,0xFFFFFF,
                           0x000000,0x800000,0x008000,0xFF0000,
                           0x008000,0xC0C0C0,0x00FF00,0xFFFF00,
                           0x0000FF,0xFF00FF,0x808080,0xFFFF00,
                           0x00FFFF,0xFF00FF,0x00FFFF,0xFFFFFF};
  int value;
  int x;
  int y;
  int color;
  for (value = 0; value < 256; value++)
    for (x = 0; x < 8; x++)
      for (y = 0; y < 2; y++) {
        color = (x < 4) ? (value & 0xF) : (value >> 4);
        SetPixel(dc,
                 SRCOFFS_DHIRES+x,(value << 1)+y,
                 colorval[color+(((x & 1) ^ (y & 1)) << 4)]);
      }
}

//===========================================================================
void DrawLoResSource (HDC dc) {
  COLORREF colorval[32] = {0x000000,0x0000FF,0x800000,0xFF00FF,
                           0x008000,0x808080,0xFF0000,0xFFFF00,
                           0x008080,0x0000FF,0xC0C0C0,0x0000FF,
                           0x00FF00,0x00FFFF,0x00FF00,0xFFFFFF,
                           0x000000,0x0000FF,0x800000,0xFF00FF,
                           0x008000,0x808080,0xFF0000,0xFFFF00,
                           0x808080,0x00FFFF,0xC0C0C0,0xFF00FF,
                           0x00FF00,0x00FFFF,0xFFFF00,0xFFFFFF};
  int color;
  int x;
  int y;
  for (color = 0; color < 16; color++)
    for (x = 0; x < 16; x++)
      for (y = 0; y < 16; y++)
        SetPixelV(dc,
                  SRCOFFS_LORES+x,(color << 4)+y,
                  colorval[color+(((x & 1) ^ (y & 1)) << 4)]);
}

//===========================================================================
void DrawHiResSource (HDC dc) {
  COLORREF colorval[12] = {0xFF00FF,0xFF0000,0x00FF00,0x0000F0,0x000000,0xFFFFFF,
                           0xFF00FF,0xFF0000,0x00FF00,0x00FFFF,0x000000,0xFFFFFF};
  int column = 0;
  do {
    int coloffs = column << 5;
    unsigned byteval = 0;
    do {
      BOOL pixelon[9];
      {
        int bitval = 1;
        int pixel  = 1;
        do {
          pixelon[pixel] = ((byteval & bitval) != 0);
          pixel++;
          bitval <<= 1;
        } while (pixel < 8);
        pixelon[0] = column & 2;
        pixelon[8] = column & 1;
      }
      {
        int hibit = ((byteval & 0x80) != 0);
        int x     = 0;
        int y     = byteval << 1;
        while (x < 28) {
          int adj   = (x >= 14) << 1;
          int odd   = (x >= 14);
          int pixel = 1;
          do {
            int color = 4;
            if (pixelon[pixel])
              if (pixelon[pixel-1] || pixelon[pixel+1])
                color = 5;
              else
                color = ((odd ^ !(pixel & 1)) << 1) | hibit;
            else if (pixelon[pixel-1] && pixelon[pixel+1])
              color = ((odd ^ (pixel & 1)) << 1) | hibit;
            SetPixelV(dc,SRCOFFS_HIRES+coloffs+x+adj  ,y  ,colorval[color]);
            SetPixelV(dc,SRCOFFS_HIRES+coloffs+x+adj+1,y  ,colorval[color+6]);
            SetPixelV(dc,SRCOFFS_HIRES+coloffs+x+adj  ,y+1,colorval[color+6]);
            SetPixelV(dc,SRCOFFS_HIRES+coloffs+x+adj+1,y+1,colorval[color]);
            pixel++;
            x += 2;
          } while (pixel < 8);
        }
      }
    } while (++byteval < 256);
  } while (++column < 4);
}

//===========================================================================
void DrawMonoDHiResSource (HDC dc) {
  int value;
  int x;
  for (value = 0; value < 256; value++) {
    int val = value;
    for (x = 0; x < 8; x++) {
      COLORREF color = (val & 1) ? 0x00FF00 : 0;
      val >>= 1;
      SetPixel(dc,SRCOFFS_DHIRES+x,(value << 1)+1,color);
    }
  }
}

//===========================================================================
void DrawMonoHiResSource (HDC dc) {
  int column = 0;
  do {
    int y = 0;
    do {
      int x = 0;
      unsigned val = (y >> 1);
      do {
        COLORREF colorval = (val & 1) ? 0x00FF00 : 0;
        val >>= 1;
        SetPixelV(dc,SRCOFFS_HIRES+column+x  ,y+1,colorval);
        SetPixelV(dc,SRCOFFS_HIRES+column+x+1,y+1,colorval);
      } while ((x += 2) < 16);
    } while ((y += 2) < 512);
  } while ((column += 16) < 128);
}

//===========================================================================
void DrawMonoTextSource (HDC dc) {
  HDC     memdc  = CreateCompatibleDC(dc);
  HBITMAP bitmap = LoadBitmap(instance,TEXT("CHARSET40"));
  SelectObject(memdc,bitmap);
  BitBlt(dc,SRCOFFS_40COL,0,256,512,memdc,0,0,MERGECOPY);
  StretchBlt(dc,SRCOFFS_80COL,0,128,512,memdc,0,0,256,512,MERGECOPY);
  DeleteDC(memdc);
  DeleteObject(bitmap);
}

//===========================================================================
void DrawTextSource (HDC dc) {
  HDC     memdc  = CreateCompatibleDC(dc);
  HBITMAP bitmap = LoadBitmap(instance,TEXT("CHARSET40"));
  SelectObject(memdc,bitmap);
  BitBlt(dc,SRCOFFS_40COL,0,256,512,memdc,0,0,SRCCOPY);
  StretchBlt(dc,SRCOFFS_80COL,0,128,512,memdc,0,0,256,512,SRCCOPY);
  DeleteDC(memdc);
  DeleteObject(bitmap);
}

//===========================================================================
void InitializeSourceImages () {
  if (!LoadSourceImages()) {
    HWND    window = GetDesktopWindow();
    HDC     dc     = GetDC(window);
    HDC     memdc  = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateBitmap(SRCOFFS_TOTAL,512,
                                  srcpixelformat >> 8,srcpixelformat & 0xFF,
                                  NULL);
    HBRUSH  brush  = CreateSolidBrush(0x00FF00);
    SelectObject(memdc,bitmap);
    SelectObject(memdc,GetStockObject(BLACK_BRUSH));
    SelectObject(memdc,GetStockObject(NULL_PEN));
    Rectangle(memdc,0,0,SRCOFFS_TOTAL,512);
    SelectObject(memdc,brush);
    if (optmonochrome) {
      DrawMonoTextSource(memdc);
      DrawMonoHiResSource(memdc);
      DrawMonoDHiResSource(memdc);
      SelectObject(memdc,GetStockObject(BLACK_PEN));
      int loop = 512;
      while ((loop -= 2) >= 0) {
        MoveToEx(memdc,0,loop,NULL);
        LineTo(memdc,SRCOFFS_TOTAL,loop);
      }
    }
    else {
      DrawTextSource(memdc);
      DrawLoResSource(memdc);
      DrawHiResSource(memdc);
      DrawDHiResSource(memdc);
    }
    DeleteDC(memdc);
    ReleaseDC(window,dc);
    GetBitmapBits(bitmap,SRCOFFS_TOTAL*64*srcpixelbits,sourcebits);
    DeleteObject(bitmap);
    DeleteObject(brush);
    SaveSourceImages();
    rebuiltsource = 1;
  }
}

//===========================================================================
BOOL LoadSourceImages () {
  if (!sourcebits)
    return 0;
  TCHAR filename[MAX_PATH];
  wsprintf(filename,
           TEXT("%sVid%03X%s.dat"),
           (LPCTSTR)progdir,
           (unsigned)(srcpixelformat & 0xFFF),
           (LPCTSTR)(optmonochrome ? TEXT("m") : TEXT("c")));
  HANDLE file = CreateFile(filename,
                           GENERIC_READ,
                           FILE_SHARE_READ,
                           (LPSECURITY_ATTRIBUTES)NULL,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                           NULL);
  if (file != INVALID_HANDLE_VALUE) {
    DWORD bytestoread = SRCOFFS_TOTAL*64*srcpixelbits;
    DWORD bytesread   = 0;
    ReadFile(file,sourcebits,bytestoread,&bytesread,NULL);
    CloseHandle(file);
    return (bytesread == bytestoread);
  }
  else
    return 0;
}

//===========================================================================
void SaveSourceImages () {
  if (!sourcebits)
    return;
  TCHAR filename[MAX_PATH];
  wsprintf(filename,
           TEXT("%sVid%03X%s.dat"),
           (LPCTSTR)progdir,
           (unsigned)(srcpixelformat & 0xFFF),
           (LPCTSTR)(optmonochrome ? TEXT("m") : TEXT("c")));
  HANDLE file = CreateFile(filename,
                           GENERIC_WRITE,
                           0,
                           (LPSECURITY_ATTRIBUTES)NULL,
                           CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                           NULL);
  if (file != INVALID_HANDLE_VALUE) {
    DWORD bytestowrite = SRCOFFS_TOTAL*64*srcpixelbits;
    DWORD byteswritten = 0;
    WriteFile(file,sourcebits,bytestowrite,&byteswritten,NULL);
    CloseHandle(file);
    if (byteswritten != bytestowrite)
      DeleteFile(filename);
  }
}

//===========================================================================
void SetLastDrawnImage () {
  memcpy(vidlastmem+0x400,textmainptr,0x400);
  if (SW_HIRES)
    memcpy(vidlastmem+0x2000,hiresmainptr,0x2000);
  if (SW_DHIRES)
    memcpy(vidlastmem,hiresauxptr,0x2000);
  else if (SW_80COL)
    memcpy(vidlastmem,textauxptr,0x400);
  int loop;
  for (loop = 0; loop < 256; loop++)
    *(memdirty+loop) &= ~2;
}

//===========================================================================
BOOL Update40ColCell (int x, int y, int xpixel, int ypixel, int offset) {
  BYTE ch = *(textmainptr+offset);
  if ((ch != *(vidlastmem+offset+0x400)) || redrawfull) {
    bitbltfunc(xpixel,ypixel,
               14,16,
               SRCOFFS_40COL+((ch & 0x0F) << 4),(ch & 0xF0)+charoffs);
    return 1;
  }
  else
    return 0;
}

//===========================================================================
BOOL Update80ColCell (int x, int y, int xpixel, int ypixel, int offset) {
  BYTE auxval  = *(textauxptr +offset);
  BYTE mainval = *(textmainptr+offset);
  if ((auxval  != *(vidlastmem+offset)) ||
      (mainval != *(vidlastmem+offset+0x400)) ||
      redrawfull) {
    bitbltfunc(xpixel,ypixel,
               7,16,
               SRCOFFS_80COL+((auxval & 15)<<3),((auxval>>4)<<4)+charoffs);
    bitbltfunc(xpixel+7,ypixel,
               7,16,
               SRCOFFS_80COL+((mainval & 15)<<3),((mainval>>4)<<4)+charoffs);
    return 1;
  }
  else
    return 0;
}

//===========================================================================
BOOL UpdateDHiResCell (int x, int y, int xpixel, int ypixel, int offset) {
  BOOL dirty   = 0;
  int  yoffset = 0;
  while (yoffset < 0x2000) {
    BYTE auxval  = *(hiresauxptr +offset+yoffset);
    BYTE mainval = *(hiresmainptr+offset+yoffset);
    BOOL draw    = (auxval  != *(vidlastmem+offset+yoffset)) ||
                   (mainval != *(vidlastmem+offset+yoffset+0x2000)) ||
                   redrawfull;
    if (offset & 1) {
      BYTE thirdval = *(hiresmainptr+offset+yoffset-1);
      draw |= (thirdval != *(vidlastmem+offset+yoffset+0x1FFF));
      if (draw) {
        int value1 = ((auxval & 0x3F) << 2) | ((thirdval & 0x60) >> 5);
        int value2 = ((mainval & 0x7F) << 1) | ((auxval & 0x40) >> 6);
        bitbltfunc(xpixel-2,ypixel+(yoffset >> 9),
                   8,2,
                   SRCOFFS_DHIRES,(value1 << 1));
        bitbltfunc(xpixel+6,ypixel+(yoffset >> 9),
                   8,2,
                   SRCOFFS_DHIRES,(value2 << 1));
      }
    }
    else {
      BYTE thirdval = *(hiresauxptr+offset+yoffset+1);
      draw |= (thirdval != *(vidlastmem+offset+yoffset+1));
      if (draw) {
        int value1 = (auxval & 0x7F) | ((mainval & 1) << 7);
        int value2 = ((mainval & 0x7E) >> 1) | ((thirdval & 3) << 6);
        bitbltfunc(xpixel,ypixel+(yoffset >> 9),
                   8,2,
                   SRCOFFS_DHIRES,(value1 << 1));
        bitbltfunc(xpixel+8,ypixel+(yoffset >> 9),
                   8,2,
                   SRCOFFS_DHIRES,(value2 << 1));
      }
    }
    if (draw)
      dirty = 1;
    yoffset += 0x400;
  }
  return dirty;
}

//===========================================================================
BOOL UpdateLoResCell (int x, int y, int xpixel, int ypixel, int offset) {
  BYTE val = *(textmainptr+offset);
  if ((val != *(vidlastmem+offset+0x400)) || redrawfull) {
    bitbltfunc(xpixel,ypixel,
               14,8,
               SRCOFFS_LORES,(int)((val & 0xF) << 4));
    bitbltfunc(xpixel,ypixel+8,
               14,8,
               SRCOFFS_LORES,(int)(val & 0xF0));
    return 1;
  }
  else
    return 0;
}

//===========================================================================
BOOL UpdateHiResCell (int x, int y, int xpixel, int ypixel, int offset) {
  BOOL dirty   = 0;
  int  yoffset = 0;
  BYTE byteval1;
  BYTE byteval2;
  BYTE byteval3;
  while (yoffset < 0x2000) {
    byteval1 = (x > 0) ? *(hiresmainptr+offset+yoffset-1) : 0;
    byteval2 = *(hiresmainptr+offset+yoffset);
    byteval3 = (x < 39) ? *(hiresmainptr+offset+yoffset+1) : 0;
    if ((byteval2 != *(vidlastmem+offset+yoffset+0x2000)) ||
        ((x >  0) && ((byteval1 & 64) != (*(vidlastmem+offset+yoffset+0x1FFF) & 64))) ||
        ((x < 39) && ((byteval3 & 1 ) != (*(vidlastmem+offset+yoffset+0x2001) & 1 ))) ||
        redrawfull) {
#define COLOFFS (((((x > 0) && (byteval1 & 64)) << 1) | \
                  ((x < 39) && (byteval3 & 1))) << 5)
      bitbltfunc(xpixel,ypixel+(yoffset >> 9),
                 14,2,
                 SRCOFFS_HIRES+COLOFFS+((x & 1) << 4),(((int)byteval2) << 1));
#undef COLOFFS
      dirty = 1;
    }
    yoffset += 0x400;
  }
  return dirty;
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BOOL VideoApparentlyDirty () {
  if (SW_MIXED || redrawfull)
    return 1;
  DWORD address = (SW_HIRES && !SW_TEXT) ? (0x20 << displaypage2)
                                         : (0x4 << displaypage2);
  DWORD length  = (SW_HIRES && !SW_TEXT) ? 0x20 : 0x4;
  while (length--)
    if (*(memdirty+(address++)) & 2)
      return 1;
  return 0;
}

//===========================================================================
void VideoBenchmark () {
  Sleep(500);

  // PREPARE TWO DIFFERENT FRAME BUFFERS, EACH OF WHICH HAVE HALF OF THE
  // BYTES SET TO 0x14 AND THE OTHER HALF SET TO 0xAA
  {
    int     loop;
    LPDWORD mem32 = (LPDWORD)mem;
    for (loop = 4096; loop < 6144; loop++)
      *(mem32+loop) = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0x14141414
                                                          : 0xAAAAAAAA;
    for (loop = 6144; loop < 8192; loop++)
      *(mem32+loop) = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0xAAAAAAAA
                                                          : 0x14141414;
  }

  // SEE HOW MANY TEXT FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
  // GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
  // SIMULATE THE ACTIVITY OF AN AVERAGE GAME
  DWORD totaltextfps = 0;
  {
    vidmode      = VF_TEXT;
    modeswitches = 0;
    FillMemory(mem+0x400,0x400,0x14);
    VideoRedrawScreen();

    DWORD milliseconds = GetTickCount();
    while (GetTickCount() == milliseconds) ;
    milliseconds = GetTickCount();
    DWORD cycle = 0;
    do {
      if (cycle & 1)
        FillMemory(mem+0x400,0x400,0x14);
      else
        CopyMemory(mem+0x400,mem+((cycle & 2) ? 0x4000 : 0x6000),0x400);
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
    vidmode      = VF_HIRES;
    modeswitches = 0;
    FillMemory(mem+0x2000,0x2000,0x14);
    VideoRedrawScreen();

    DWORD milliseconds = GetTickCount();
    while (GetTickCount() == milliseconds) ;
    milliseconds = GetTickCount();
    DWORD cycle = 0;
    do {
      if (cycle & 1)
        FillMemory(mem+0x2000,0x2000,0x14);
      else
        CopyMemory(mem+0x2000,mem+((cycle & 2) ? 0x4000 : 0x6000),0x2000);
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
    while (GetTickCount() == milliseconds) ;
    milliseconds = GetTickCount();
    DWORD cycle = 0;
    do {
      CpuExecute(100000);
      totalmhz10++;
    } while (GetTickCount() - milliseconds < 1000);
  }

  // IF THE PROGRAM COUNTER IS NOT IN THE EXPECTED RANGE AT THE END OF THE
  // CPU BENCHMARK, REPORT AN ERROR AND OPTIONALLY TRACK IT DOWN
  if ((regs.pc < 0x300) || (regs.pc > 0x400))
    if (MessageBox(framewindow,
                   TEXT("The emulator has detected a problem while running ")
                   TEXT("the CPU benchmark.  Would you like to gather more ")
                   TEXT("information?"),
                   TEXT("Benchmarks"),
                   MB_ICONQUESTION | MB_YESNO) == IDYES) {
      BOOL error  = 0;
      WORD lastpc = 0x300;
      int  loop   = 0;
      while ((loop < 10000) && !error) {
        CpuSetupBenchmark();
        CpuExecute(loop);
        if ((regs.pc < 0x300) || (regs.pc > 0x400))
          error = 1;
        else {
          lastpc = regs.pc;
          ++loop;
        }
      }
      if (error) {
        TCHAR outstr[256];
        wsprintf(outstr,
                 TEXT("The emulator experienced an error %u clock cycles ")
                 TEXT("into the CPU benchmark.  Prior to the error, the ")
                 TEXT("program counter was at $%04X.  After the error, it ")
                 TEXT("had jumped to $%04X."),
                 (unsigned)loop,
                 (unsigned)lastpc,
                 (unsigned)regs.pc);
        MessageBox(framewindow,
                   outstr,
                   TEXT("Benchmarks"),
                   MB_ICONINFORMATION);
      }
      else
        MessageBox(framewindow,
                   TEXT("The emulator was unable to locate the exact ")
                   TEXT("point of the error.  This probably means that ")
                   TEXT("the problem is external to the emulator, ")
                   TEXT("happening asynchronously, such as a problem in ")
                   TEXT("a timer interrupt handler."),
                   TEXT("Benchmarks"),
                   MB_ICONINFORMATION);
    }

  // DO A REALISTIC TEST OF HOW MANY FRAMES PER SECOND WE CAN PRODUCE
  // WITH FULL EMULATION OF THE CPU, JOYSTICK, AND DISK HAPPENING AT
  // THE SAME TIME
  DWORD realisticfps = 0;
  {
    FillMemory(mem+0x2000,0x2000,0xAA);
    VideoRedrawScreen();
    DWORD milliseconds = GetTickCount();
    while (GetTickCount() == milliseconds) ;
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
          VideoUpdateVbl(executedcycles,0);
        }
      }
      if (cycle & 1)
        FillMemory(mem+0x2000,0x2000,0xAA);
      else
        CopyMemory(mem+0x2000,mem+((cycle & 2) ? 0x4000 : 0x6000),0x2000);
      VideoRefreshScreen();
      if (cycle++ >= 3)
        cycle = 0;
      realisticfps++;
    } while (GetTickCount() - milliseconds < 1000);
  }

  // DISPLAY THE RESULTS
  VideoDisplayLogo();
  {
    TCHAR outstr[256];
    wsprintf(outstr,
             TEXT("Pixel Format:\t%x\n")
             TEXT("Pure Video FPS:\t%u hires, %u text\n")
             TEXT("Pure CPU MHz:\t%u.%u%s\n\n")
             TEXT("EXPECTED AVERAGE VIDEO GAME\n")
             TEXT("PERFORMANCE: %u FPS"),
             (unsigned)pixelformat,
             (unsigned)totalhiresfps,
             (unsigned)totaltextfps,
             (unsigned)(totalmhz10/10),
             (unsigned)(totalmhz10 % 10),
             (LPCTSTR)(apple2e ? TEXT("") : TEXT(" (6502)")),
             (unsigned)realisticfps);
    MessageBox(framewindow,
               outstr,
               TEXT("Benchmarks"),
               MB_ICONINFORMATION);
  }

}
            
//===========================================================================
BYTE __stdcall VideoCheckMode (WORD, BYTE address, BYTE, BYTE) {
  if (address == 0x7F)
    return MemReturnRandomData(SW_DHIRES != 0);
  else {
    BOOL result = 0;
    switch (address) {
      case 0x1A: result = SW_TEXT;    break;
      case 0x1B: result = SW_MIXED;   break;
      case 0x1D: result = SW_HIRES;   break;
      case 0x1E: result = charoffs;   break;
      case 0x1F: result = SW_80COL;   break;
      case 0x7F: result = SW_DHIRES;  break;
    }
    return KeybGetKeycode() | (result ? 0x80 : 0);
  }
}

//===========================================================================
void VideoCheckPage (BOOL force) {
  if ((displaypage2 != (SW_PAGE2 != 0)) &&
      (force || (emulmsec-lastpageflip > 500))) {
    displaypage2 = (SW_PAGE2 != 0);
    VideoRefreshScreen();
    hasrefreshed = 1;
    lastpageflip = emulmsec;
  }
}

//===========================================================================
BYTE __stdcall VideoCheckVbl (WORD, BYTE, BYTE, BYTE) {
  return MemReturnRandomData(vblcounter < 22);
}

//===========================================================================
void VideoDestroy () {
  VideoReleaseFrameDC();

  VirtualFree(framebufferinfo,0,MEM_RELEASE);
  VirtualFree(sourcebits     ,0,MEM_RELEASE);
  VirtualFree(vidlastmem     ,0,MEM_RELEASE);
  framebufferinfo = NULL;
  sourcebits      = NULL;
  vidlastmem      = NULL;

  if (!usingdib) {
    VirtualFree(framebufferbits  ,0,MEM_RELEASE);
    VirtualFree(framebufferdibits,0,MEM_RELEASE);
    framebufferbits   = NULL;
    framebufferdibits = NULL;
  }

  DeleteDC(devicedc);
  DeleteObject(devicebitmap);
  DeleteObject(videofont);
  devicedc     = (HDC)0;
  devicebitmap = (HBITMAP)0;
  videofont    = (HFONT)0;

  if (logoptr)
    VideoDestroyLogo();

  if (fastinst) {
    FreeLibrary(fastinst);
    fastinst   = (HINSTANCE)0;
    bitbltfunc = NULL;
  }
}

//===========================================================================
void VideoDestroyLogo () {
  if (logoptr) {
    UnmapViewOfFile(logoview);
    CloseHandle(logomap);
    CloseHandle(logofile);
    logofile = INVALID_HANDLE_VALUE;
    logomap  = INVALID_HANDLE_VALUE;
    logoptr  = NULL;
    logoview = NULL;
  }
}

//===========================================================================
void VideoDisplayLogo () {
  if (!framedc)
    framedc = FrameGetDC();

  // DRAW THE LOGO, USING SETDIBITSTODEVICE() IF IT IS AVAILABLE OR
  // BITBLT() IF IT IS NOT
  if (logoptr) {
    if (!SetDIBitsToDevice(framedc,
                           0,
                           0,
                           logoptr->bmiHeader.biWidth,
                           logoptr->bmiHeader.biHeight,
                           0,
                           0,
                           0,
                           logoptr->bmiHeader.biHeight,
                           (LPVOID)(((LPBYTE)logoptr)
                                      +sizeof(BITMAPINFOHEADER)
                                      +((bitsperpixel <= 4) ? 16 : 256)*sizeof(RGBQUAD)),
                           logoptr,
                           (pixelformat == 108) ? DIB_PAL_INDICES : DIB_RGB_COLORS)) {
      LPBITMAPINFO info = (LPBITMAPINFO)VirtualAlloc(NULL,
                                                     sizeof(BITMAPINFOHEADER)
                                                       +256*sizeof(RGBQUAD),
                                                     MEM_COMMIT,
                                                     PAGE_READWRITE);
      if (info) {
        ZeroMemory(info,sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD));
        info->bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
        info->bmiHeader.biWidth    = 560;
        info->bmiHeader.biHeight   = 384;
        info->bmiHeader.biPlanes   = 1;
        info->bmiHeader.biBitCount = ((bitsperpixel <= 4) ? 4 : 8);
        info->bmiHeader.biClrUsed  = ((bitsperpixel <= 4) ? 16 : 256);
        CopyMemory(info->bmiColors,logoptr->bmiColors,
                   ((bitsperpixel <= 4) ? 16*sizeof(RGBQUAD) : 256*sizeof(RGBQUAD)));
        SetDIBits(devicedc,devicebitmap,0,384,
                  (LPVOID)(((LPBYTE)logoptr)
                             +sizeof(BITMAPINFOHEADER)
                             +((bitsperpixel <= 4) ? 16 : 256)*sizeof(RGBQUAD)),
                  info,
                  DIB_RGB_COLORS);
        BitBlt(framedc,0,0,560,384,devicedc,0,0,SRCCOPY);
        VirtualFree(info,0,MEM_RELEASE);
      }
    }
  }

  // DRAW THE VERSION NUMBER
  {
    HFONT font    = CreateFont(-20,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,
                               OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
                               VARIABLE_PITCH | 4 | FF_SWISS,
                               TEXT("Arial"));
    HFONT oldfont = (HFONT)SelectObject(framedc,font);
    SetTextAlign(framedc,TA_RIGHT | TA_TOP);
    SetBkMode(framedc,TRANSPARENT);
#define  DRAWVERSION(x,y,c)  SetTextColor(framedc,c);                \
                             TextOut(framedc,                        \
                                     540+x,358+y,                    \
                                     TEXT("Version ") VERSIONSTRING, \
                                     strlen(TEXT("Version ") VERSIONSTRING));
    if (bitsperpixel <= 4) {
      DRAWVERSION( 2, 2,0x000000);
      DRAWVERSION( 1, 1,0x000000);
      DRAWVERSION( 0, 0,0x000080);
    }
    else if (GetDeviceCaps(framedc,RASTERCAPS) & RC_PALETTE) {
      int offset = GetDeviceCaps(framedc,NUMCOLORS)/2;
      DRAWVERSION( 1, 1,PALETTEINDEX(  2+offset));
      DRAWVERSION(-1,-1,PALETTEINDEX(122+offset));
      DRAWVERSION( 0, 0,PALETTEINDEX( 32+offset));
    }
    else {
      DRAWVERSION( 1, 1,0x6A2136);
      DRAWVERSION(-1,-1,0xE76BBD);
      DRAWVERSION( 0, 0,0xD63963);
    }
#undef  DRAWVERSION
    SetTextAlign(framedc,TA_RIGHT | TA_TOP);
    SelectObject(framedc,oldfont);
    DeleteObject(font);
  }

  FrameReleaseDC(framedc);
  framedc = (HDC)0;
}

//===========================================================================
void VideoDisplayMode (BOOL flashon) {
  if (!framedc)
    framedc = FrameGetDC();
  LPTSTR text = TEXT("        ");
  if (mode == MODE_PAUSED) {
    SetBkColor(framedc,0x000000);
    SetTextColor(framedc,0x00FFFFF);
    if (flashon)
      text = TEXT(" PAUSED ");
  }
  else {
    SetBkColor(framedc,0xFFFFFF);
    SetTextColor(framedc,0x800000);
    text = TEXT("STEPPING");
  }
  SelectObject(framedc,videofont);
  SetTextAlign(framedc,TA_LEFT | TA_TOP);
  RECT rect = {492,0,560,16};
  ExtTextOut(framedc,495,0,ETO_CLIPPED | ETO_OPAQUE,&rect,text,8,NULL);
}

//===========================================================================
BOOL VideoHasRefreshed () {
  BOOL result = hasrefreshed;
  hasrefreshed = 0;
  return result;
}

//===========================================================================
void VideoInitialize () {

  // CREATE A FONT FOR DRAWING TEXT ABOVE THE SCREEN
  videofont = CreateFont(16,0,0,0,FW_MEDIUM,0,0,0,ANSI_CHARSET,
                         OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                         DEFAULT_QUALITY,FIXED_PITCH | 4 | FF_MODERN,
                         TEXT("Courier New"));

  // CREATE A BUFFER FOR AN IMAGE OF THE LAST DRAWN MEMORY
  vidlastmem = (LPBYTE)VirtualAlloc(NULL,0x10000,MEM_COMMIT,PAGE_READWRITE);
  ZeroMemory(vidlastmem,0x10000);

  // DETERMINE THE NUMBER OF BITS PER PIXEL USED BY THE CURRENT DEVICE
  if (videocompatible) {
    HWND window  = GetDesktopWindow();
    HDC  dc      = GetDC(window);
    bitsperpixel = GetDeviceCaps(dc,PLANES) * GetDeviceCaps(dc,BITSPIXEL);
    pixelformat  = (GetDeviceCaps(dc,PLANES) << 8) | GetDeviceCaps(dc,BITSPIXEL);
    ReleaseDC(window,dc);
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
  pixelbits = (pixelformat & 0xFF)*(pixelformat >> 8);
  srcpixelbits = (srcpixelformat & 0xFF)*(srcpixelformat >> 8);

  // LOAD THE LOGO
  VideoLoadLogo();

  // CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
  framebufferinfo = (LPBITMAPINFO)VirtualAlloc(NULL,
                                               sizeof(BITMAPINFOHEADER)
                                                 +256*sizeof(RGBQUAD),
                                               MEM_COMMIT,
                                               PAGE_READWRITE);
  ZeroMemory(framebufferinfo,sizeof(BITMAPINFOHEADER)+256*sizeof(RGBQUAD));
  framebufferinfo->bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
  framebufferinfo->bmiHeader.biWidth    = 560;
  framebufferinfo->bmiHeader.biHeight   = 384;
  framebufferinfo->bmiHeader.biPlanes   = srcpixelformat >> 8;
  framebufferinfo->bmiHeader.biBitCount = srcpixelformat & 0xFF;
  framebufferinfo->bmiHeader.biClrUsed  = 256;

  // CREATE AN IDENTITY PALETTE AND FILL IN THE CORRESPONDING COLORS IN
  // THE BITMAPINFO STRUCTURE
  CreateIdentityPalette(logoptr ? logoptr->bmiColors : NULL,
                        framebufferinfo->bmiColors);

  // CREATE A BIT BUFFER FOR THE SOURCE IMAGES
  sourcebits = (LPBYTE)VirtualAlloc(NULL,SRCOFFS_TOTAL*64*srcpixelbits+4,
                                    MEM_COMMIT,PAGE_READWRITE);
  ZeroMemory(sourcebits,SRCOFFS_TOTAL*64*srcpixelbits+4);

  // DETERMINE WHETHER TO USE THE CREATEDIBSECTION() OR SETBITS() METHOD
  // OF BITMAP DATA UPDATING
  HINSTANCE gdiinst = (HINSTANCE)0;
  if ((pixelformat == 0x108) || (pixelformat == 0x110) ||
      (pixelformat == 0x118) || (pixelformat == 0x120)) {
    gdiinst = LoadLibrary(TEXT("GDI32"));
    if (gdiinst) {
      createdibsection = (createdibtype)GetProcAddress(gdiinst,
                                                       TEXT("CreateDIBSection"));
      usingdib         = (createdibsection != NULL);
    }
  }

  // CREATE THE DEVICE DEPENDENT BITMAP AND DEVICE CONTEXT
  {
    HWND window  = GetDesktopWindow();
    HDC  dc      = GetDC(window);
    if (usingdib) {
      framebufferbits = NULL;
      devicebitmap    = createdibsection(dc,framebufferinfo,
                                         DIB_RGB_COLORS,
                                         (LPVOID *)&framebufferbits,
                                         0,0);
      usingdib        = (devicebitmap && framebufferinfo);
    }
    devicedc = CreateCompatibleDC(dc);
    if (!usingdib)
      devicebitmap = CreateCompatibleBitmap(dc,560,384);
    if (gdiinst)
      FreeLibrary(gdiinst);
    ReleaseDC(window,dc);
    SelectPalette(devicedc,palette,0);
    RealizePalette(devicedc);
    SelectObject(devicedc,devicebitmap);
  }

  // IF WE ARE NOT USING CREATEDIBSECTION() THEN CREATE BIT BUFFERS FOR THE
  // FRAME BUFFER AND DIB FRAME BUFFER
  if (!usingdib) {
    framebufferbits   = (LPBYTE)VirtualAlloc(NULL,70*384*pixelbits+4,
                                             MEM_COMMIT,PAGE_READWRITE);
    framebufferdibits = (LPBYTE)VirtualAlloc(NULL,70*384*pixelbits+4,
                                             MEM_COMMIT,PAGE_READWRITE);
    ZeroMemory(framebufferbits,70*384*pixelbits+4);
  }

  // CREATE OFFSET TABLES FOR EACH SCAN LINE IN THE SOURCE IMAGES AND
  // FRAME BUFFER
  if ((srcpixelformat <= 0x108) || (srcpixelformat >= 0x200)) {
    BOOL type401b = (srcpixelformat == 0x401) && ((GetVersion() & 0xFF) < 4);
    int  loop     = 0;
    while (loop < 512) {
      if (loop < 384)
        frameoffsettable[loop] = framebufferbits
                                   +((bitsperpixel == 4) ? (type401b ? 70 : 280)
                                                         : 560)
                                      *(usingdib ? (383-loop) : loop);
      sourceoffsettable[loop] = sourcebits+((SRCOFFS_TOTAL*loop)
                                              >> ((bitsperpixel == 4) ? 1 : 0));
      loop++;
    }
  }
  else {
    int bytespixel = srcpixelbits >> 3;
    int loop       = 0;
    while (loop < 512) {
      if (loop < 384)
        frameoffsettable[loop] = framebufferbits
                                   +(560*bytespixel)*(usingdib ? (383-loop) : loop);
      sourceoffsettable[loop] = sourcebits+SRCOFFS_TOTAL*bytespixel*loop;
      loop++;
    }
  }

  // DETERMINE WHICH BITBLT FUNCTION TO USE AND INITIALIZE IT
  {
    if (fastinst) {
      FreeLibrary(fastinst);
      fastinst = (HINSTANCE)0;
    }
    bitbltfunc = NULL;
    BOOL win95 = ((GetVersion() & 0xFF) >= 4);
#ifdef _X86_
    if ((bitsperpixel != 4) || !i386) {
      TCHAR filename[MAX_PATH];
      _tcscpy(filename,progdir);
      _tcscat(filename,TEXT("fastblt.dll"));
      fastinst = LoadLibrary(filename);
    }
    if (fastinst) {
      TCHAR funcname[16];
      wsprintf(funcname,TEXT("FastBlt%x"),(unsigned)srcpixelformat);
      if ((srcpixelformat == 0x401) && !win95)
        _tcscat(funcname,TEXT("b"));
      else if (usingdib)
        _tcscat(funcname,TEXT("d"));
      bitbltfunc = (bitblttype)GetProcAddress(fastinst,funcname);
      fastbltinittype initfunc = (fastbltinittype)GetProcAddress(fastinst,
                                                                 TEXT("FastBltInit"));
      if (bitbltfunc && initfunc)
        initfunc(sourcebits,framebufferbits,sourceoffsettable,frameoffsettable);
      else {
        FreeLibrary(fastinst);
        fastinst   = (HINSTANCE)0;
        bitbltfunc = NULL;
      }
    }
#endif
    if (!bitbltfunc)
      switch (srcpixelformat) {
        case 0x104: bitbltfunc = BitBlt104;                          break;
        case 0x108: bitbltfunc = usingdib ? BitBlt108d : BitBlt108;  break;
        case 0x110: bitbltfunc = usingdib ? BitBlt110d : BitBlt110;  break;
        case 0x118: bitbltfunc = usingdib ? BitBlt118d : BitBlt118;  break;
        case 0x120: bitbltfunc = usingdib ? BitBlt120d : BitBlt120;  break;
        case 0x401: bitbltfunc = (!win95) ? BitBlt401b : BitBlt401;  break;
      }
  }

  // LOAD THE SOURCE IMAGES FROM DISK, OR DRAW THEM AND TRANSFER THEM
  // INTO THE SOURCE BIT BUFFER
  InitializeSourceImages();

  // RESET THE VIDEO MODE SWITCHES AND THE CHARACTER SET OFFSET
  VideoResetState();

}

//===========================================================================
void VideoLoadLogo () {
  if (logoptr)
    return;
  TCHAR filename[MAX_PATH];
  _tcscpy(filename,progdir);
  _tcscat(filename,TEXT("AppleWin.lgo"));
  logofile = CreateFile(filename,
                        GENERIC_READ,
                        FILE_SHARE_READ,
                        (LPSECURITY_ATTRIBUTES)NULL,
                        OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                        NULL);
  logomap  = CreateFileMapping(logofile,
                               (LPSECURITY_ATTRIBUTES)NULL,
                               PAGE_READONLY,
                               0,0,NULL);
  logoview = (LPBYTE)MapViewOfFile(logomap,
                                   FILE_MAP_READ,
                                   0,0,0);
  if (logoview)
    logoptr = (bitsperpixel <= 4) ? (LPBITMAPINFO)(logoview+0x35000+sizeof(BITMAPFILEHEADER))
                                  : (LPBITMAPINFO)(logoview+0x200+sizeof(BITMAPFILEHEADER));
  else
    logoptr = NULL;
}

//===========================================================================
void VideoRealizePalette (HDC dc) {
  if (!dc) {
    if (!framedc)
      framedc = FrameGetDC();
    dc = framedc;
  }
  SelectPalette(dc,palette,0);
  RealizePalette(dc);
}

//===========================================================================
void VideoRedrawScreen () {
  redrawfull = 1;
  VideoRefreshScreen();
}

//===========================================================================
void VideoRefreshScreen () {
  if (!framedc)
    framedc = FrameGetDC();

  // IF THE MODE HAS BEEN SWITCHED MORE THAN TWICE IN THE LAST FRAME, THE
  // PROGRAM IS PROBABLY TRYING TO DO A FLASHING EFFECT, SO JUST FLASH THE
  // SCREEN WHITE AND RETURN
  if (modeswitches > 2) {
    modeswitches = 0;
    SelectObject(framedc,GetStockObject(WHITE_BRUSH));
    SelectObject(framedc,GetStockObject(WHITE_PEN));
    Rectangle(framedc,0,0,560,384);
    return;
  }
  modeswitches = 0;

  // CHECK EACH CELL FOR CHANGED BYTES.  REDRAW PIXELS FOR THE CHANGED BYTES
  // IN THE FRAME BUFFER.  MARK CELLS IN WHICH REDRAWING HAS TAKEN PLACE AS
  // DIRTY.
  hiresauxptr  = MemGetAuxPtr (0x2000 << displaypage2);
  hiresmainptr = MemGetMainPtr(0x2000 << displaypage2);
  textauxptr   = MemGetAuxPtr (0x400  << displaypage2);
  textmainptr  = MemGetMainPtr(0x400  << displaypage2);
  ZeroMemory(celldirty,40*32);
  {
    updatetype update1  = NULL;
    updatetype update2  = NULL;
    update1 = SW_TEXT ? SW_80COL ? Update80ColCell
                                 : Update40ColCell
                      : SW_HIRES ? (SW_DHIRES && SW_80COL) ? UpdateDHiResCell
                                                           : UpdateHiResCell
                                 : UpdateLoResCell;
    update2 = SW_MIXED ? SW_80COL ? Update80ColCell
                                  : Update40ColCell
                       : update1;
    BOOL anydirty = 0;
    int  y        = 0;
    int  ypixel   = 0;
    while (y < 20) {
      int offset = ((y & 7) << 7) + ((y >> 3) * 40);
      int x      = 0;
      int xpixel = 0;
      while (x < 40) {
        anydirty |= celldirty[x][y] = update1(x,y,xpixel,ypixel,offset+x);
        ++x;
        xpixel += 14;
      }
      ++y;
      ypixel += 16;
    }
    while (y < 24) {
      int offset = ((y & 7) << 7) + ((y >> 3) * 40);
      int x      = 0;
      int xpixel = 0;
      while (x < 40) {
        anydirty |= celldirty[x][y] = update2(x,y,xpixel,ypixel,offset+x);
        ++x;
        xpixel += 14;
      }
      ++y;
      ypixel += 16;
    }
    if (!anydirty) {
      SetLastDrawnImage();
      return;
    }
  }

  // CONVERT THE FRAME BUFFER BITS INTO A DEVICE DEPENDENT BITMAP
  if (!usingdib)
    switch (pixelformat) {

      case 0x104:
      case 0x401:
        SetBitmapBits(devicebitmap,280*384,framebufferbits);
        break;

      case 0x108:
        SetBitmapBits(devicebitmap,560*384,framebufferbits);
        break;

      case 0x110:
        SetBitmapBits(devicebitmap,560*384*2,framebufferbits);
        break;

      case 0x118:
        SetBitmapBits(devicebitmap,560*384*3,framebufferbits);
        break;

      case 0x120:
        SetBitmapBits(devicebitmap,560*384*4,framebufferbits);
        break;

      default:
        ConvertToBottomUp8();
        SetDIBits(devicedc,devicebitmap,0,384,
                  framebufferdibits,framebufferinfo,
                  DIB_RGB_COLORS);
        break;

    }

  // COPY DIRTY CELLS FROM THE DEVICE DEPENDENT BITMAP ONTO THE SCREEN
  // IN LONG HORIZONTAL RECTANGLES
  BOOL remainingdirty = 0;
  {
    int y      = 0;
    int ypixel = 0;
    while (y < 24) {
      int start  = -1;
      int startx = 0;
      int x      = 0;
      int xpixel = 0;
      while (x < 40) {
        if ((x == 39) && celldirty[x][y])
          if (start >= 0) {
            xpixel += 14;
            celldirty[x][y] = 0;
          }
          else
            remainingdirty = 1;
        if ((start >= 0) && !celldirty[x][y]) {
          if ((x - startx > 1) || ((x == 39) && (xpixel == 560))) {
            int height = 1;
            while ((y+height < 24)
                     && celldirty[startx][y+height]
                     && celldirty[x-1][y+height]
                     && celldirty[(startx+x-1) >> 1][y+height])
              height++;
            BitBlt(framedc,start,ypixel,xpixel-start,height << 4,
                   devicedc,start,ypixel,SRCCOPY);
            while (height--) {
              int loop = startx;
              while (loop < x+(xpixel == 560))
                celldirty[loop++][y+height] = 0;
            }
            start = -1;
          }
          else
            remainingdirty = 1;
          start = -1;
        }
        else if ((start == -1) && celldirty[x][y] && (x < 39)) {
          start  = xpixel;
          startx = x;
        }
        x++;
        xpixel += 14;
      }
      y++;
      ypixel += 16;
    }
  }

  // COPY ANY REMAINING DIRTY CELLS FROM THE DEVICE DEPENDENT BITMAP
  // ONTO THE SCREEN IN VERTICAL RECTANGLES
  if (remainingdirty) {
    int x      = 0;
    int xpixel = 0;
    while (x < 40) {
      int start  = -1;
      int y      = 0;
      int ypixel = 0;
      while (y < 24) {
        if ((y == 23) && celldirty[x][y]) {
          if (start == -1)
            start = ypixel;
          ypixel += 16;
          celldirty[x][y] = 0;
        }
        if ((start >= 0) && !celldirty[x][y]) {
          BitBlt(framedc,xpixel,start,14,ypixel-start,
                 devicedc,xpixel,start,SRCCOPY);
          start = -1;
        }
        else if ((start == -1) && celldirty[x][y])
          start = ypixel;
        y++;
        ypixel += 16;
      }
      x++;
      xpixel += 14;
    }
  }

  GdiFlush();
  SetLastDrawnImage();
  redrawfull = 0;

  if ((mode == MODE_PAUSED) || (mode == MODE_STEPPING))
    VideoDisplayMode(1);
}

//===========================================================================
void VideoReinitialize () {
  InitializeSourceImages();
}

//===========================================================================
void VideoReleaseFrameDC () {
  if (framedc) {
    FrameReleaseDC(framedc);
    framedc = (HDC)0;
  }
}

//===========================================================================
void VideoResetState () {
  charoffs     = 0;
  displaypage2 = 0;
  vidmode      = VF_TEXT;
  redrawfull   = 1;
}

//===========================================================================
BYTE __stdcall VideoSetMode (WORD, BYTE address, BYTE write, BYTE) {
  DWORD oldpage2 = SW_PAGE2;
  int   oldvalue = charoffs+(int)(vidmode & ~(VF_MASK2 | VF_PAGE2));
  switch (address) {
    case 0x00: vidmode &= ~VF_MASK2;   break;
    case 0x01: vidmode |=  VF_MASK2;   break;
    case 0x0C: vidmode &= ~VF_80COL;   break;
    case 0x0D: vidmode |=  VF_80COL;   break;
    case 0x0E: charoffs = 0;           break;
    case 0x0F: charoffs = 256;         break;
    case 0x50: vidmode &= ~VF_TEXT;    break;
    case 0x51: vidmode |=  VF_TEXT;    break;
    case 0x52: vidmode &= ~VF_MIXED;   break;
    case 0x53: vidmode |=  VF_MIXED;   break;
    case 0x54: vidmode &= ~VF_PAGE2;   break;
    case 0x55: vidmode |=  VF_PAGE2;   break;
    case 0x56: vidmode &= ~VF_HIRES;   break;
    case 0x57: vidmode |=  VF_HIRES;   break;
    case 0x5E: vidmode |=  VF_DHIRES;  break;
    case 0x5F: vidmode &= ~VF_DHIRES;  break;
  }
  if (SW_MASK2)
    vidmode &= ~VF_PAGE2;
  if (oldvalue != charoffs+(int)(vidmode & ~(VF_MASK2 | VF_PAGE2))) {
    if ((SW_80COL != 0) == (SW_DHIRES != 0))
      modeswitches++;
    graphicsmode = !SW_TEXT;
    redrawfull   = 1;
  }
  if (fullspeed && oldpage2 && !SW_PAGE2) {
    static DWORD lasttime = 0;
    DWORD currtime = GetTickCount();
    if (currtime-lasttime >= 20)
      lasttime = currtime;
    else
      oldpage2 = SW_PAGE2;
  }
  if (oldpage2 != SW_PAGE2) {
    static DWORD lastrefresh = 0;
    BOOL fastvideoslowcpu = 0;
    if ((cpuemtype == CPU_FASTPAGING) && (emulmsec-lastrefresh >= 20))
      fastvideoslowcpu = 1;
    if ((displaypage2 && !SW_PAGE2) || (!behind) || fastvideoslowcpu) {
      displaypage2 = (SW_PAGE2 != 0);
      if (!redrawfull) {
        VideoRefreshScreen();
        hasrefreshed = 1;
        lastrefresh  = emulmsec;
      }
    }
    else if ((!SW_PAGE2) && (!redrawfull) && (emulmsec-lastrefresh >= 20)) {
      displaypage2 = 0;
      VideoRefreshScreen();
      hasrefreshed = 1;
      lastrefresh  = emulmsec;
    }
    lastpageflip = emulmsec;
  }
  if (address == 0x50)
    return VideoCheckVbl(0,0,0,0);
  else
    return MemReturnRandomData(1);
}

//===========================================================================
void VideoTestCompatibility () {
  if (!mem)
    return;

  // PERFORM THE TEST ONLY ONCE EACH TIME THE EMULATOR IS RUN
  static BOOL firsttime = 1;
  if (!firsttime)
    return;
  firsttime = 0;

  // IF APPLEWIN HAS HAD TO DISABLE ITS FASTBLT MODULE BECAUSE THE USER
  // HAS A 386 AND IS USING 16-COLOR MODE, WARN THAT VIDEO OUTPUT WILL
  // BE VERY SLOW
  if (i386 && (bitsperpixel == 4))
    MessageBox(framewindow,
               TEXT("AppleWin has detected that you are using a 386 with ")
               TEXT("a 16-color display.  This will result in extremely ")
               TEXT("slow video.  If at all possible, you should switch ")
               TEXT("to 256-color mode using the Control Panel."),
               TITLE,
               MB_ICONEXCLAMATION);

  // DETERMINE THE NAME OF THIS VIDEO MODE
  TCHAR modename[64];
  wsprintf(modename,
           TEXT("Video Mode %ux%u %ubpp"),
           (unsigned)GetSystemMetrics(SM_CXSCREEN),
           (unsigned)GetSystemMetrics(SM_CYSCREEN),
           (unsigned)bitsperpixel);

  // IF WE HAVE ALREADY TESTED THIS VIDEO MODE AND FOUND IT COMPATIBLE,
  // JUST RETURN
  BOOL samemode = 1;
  if (!RegLoadValue(TEXT("Compatibility"),modename,0,&videocompatible)) {
    samemode        = 0;
    videocompatible = 1;
  }
  {
    TCHAR savedmodename[64] = TEXT("");
    RegLoadString(TEXT("Compatibility"),TEXT("Last Video Mode"),0,
                  savedmodename,63);
    if (_tcscmp(modename,savedmodename) || rebuiltsource)
      samemode = 0;
  }
  if (samemode && videocompatible)
    return;
  if (!samemode)
    videocompatible = 1;

  // ENTER HIRES GRAPHICS MODE AND DRAW TWO PIXELS ON THE SCREEN
  DWORD oldvidmode = vidmode;
  vidmode       = VF_HIRES;
  modeswitches  = 0;
  *(mem+0x2000) = 0x02;
  *(mem+0x3FF7) = 0x20;

  // EXAMINE THE SCREEN TO MAKE SURE THAT THE PIXELS WERE DRAWN CORRECTLY
  TCHAR interference[64];
  BOOL  success;
  do {
    VideoRedrawScreen();
    success         = 1;
    interference[0] = 0;
    int loop;
    for (loop = 1; loop <= 4; ++loop) {
      CheckPixel(loop,0,((loop == 2) || (loop == 3)) ? 0x00FF00 : 0,
                 &success,interference);
      CheckPixel(loop,1,((loop == 2) || (loop == 3)) ? 0x00FF00 : 0,
                 &success,interference);
      CheckPixel(loop,2,0,
                 &success,interference);
    }
    for (loop = 555; loop <= 558; ++loop) {
      CheckPixel(loop,381,0,
                 &success,interference);
      CheckPixel(loop,382,((loop == 556) || (loop == 557)) ? 0xFF00FF : 0,
                 &success,interference);
      CheckPixel(loop,383,((loop == 556) || (loop == 557)) ? 0xFF00FF : 0,
                 &success,interference);
    }
    if (interference[0]) {
      TCHAR buffer[256];
      wsprintf(buffer,
               TEXT("AppleWin needs to perform a routine test on your ")
               TEXT("video driver.  Please move %s so that it does not ")
               TEXT("obscure the emulator window, then click OK."),
               (LPCTSTR)interference);
      MessageBox(framewindow,
                 buffer,
                 TITLE,
                 MB_ICONEXCLAMATION);
    }
  } while (interference[0]);

  // RESTORE THE VIDEO MODE
  vidmode = oldvidmode;
  *(mem+0x2000) = 0;
  *(mem+0x3FF7) = 0;

  // IF THE RESULTS WERE UNEXPECTED, INFORM THE USER
  if (videocompatible && !success)
    MessageBox(framewindow,
               TEXT("AppleWin has detected a compatibility problem with your ")
               TEXT("video driver.  You may want to use a different video ")
               TEXT("mode, or obtain an updated driver from your vendor.\n\n")
               TEXT("In the meantime, AppleWin will attempt to work around ")
               TEXT("the problem by limiting its use of the driver.  This ")
               TEXT("may significantly reduce performance."),
               TITLE,
               MB_ICONEXCLAMATION);
  else if (success && !videocompatible)
    MessageBox(framewindow,
               TEXT("AppleWin had previously reported a compatibility ")
               TEXT("problem in your video driver.  The problem seems ")
               TEXT("to have been resolved."),
               TITLE,
               MB_ICONINFORMATION);

  // SAVE THE RESULTS
  videocompatible = success;
  RegSaveValue(TEXT("Compatibility"),modename,0,videocompatible);
  RegSaveString(TEXT("Compatibility"),TEXT("Last Video Mode"),0,modename);

  // IF WE DISCOVERED A COMPATIBILITY PROBLEM, THEN REINITIALIZE VIDEO
  if (!videocompatible) {
    VideoDestroy();
    VideoInitialize();
  }

}

//===========================================================================
void VideoUpdateVbl (DWORD cycles, BOOL nearrefresh) {
  if (vblcounter)
    vblcounter -= MIN(vblcounter,(cycles >> 6));
  else if (!nearrefresh)
    vblcounter = 250;
}
