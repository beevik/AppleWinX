/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/


extern BOOL graphicsmode;

BOOL VideoApparentlyDirty();
void VideoBenchmark();
void VideoCheckPage(BOOL force);
void VideoDestroy();
void VideoDestroyLogo();
void VideoDisplayLogo();
void VideoDisplayMode(BOOL flashon);
BOOL VideoHasRefreshed();
void VideoInitialize();
void VideoLoadLogo();
void VideoRealizePalette(HDC dc);
void VideoRedrawScreen();
void VideoRefreshScreen();
void VideoReinitialize();
void VideoReleaseFrameDC();
void VideoResetState();
void VideoUpdateVbl(DWORD cycles, BOOL nearrefresh);

BYTE VideoCheckMode(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE VideoCheckVbl(WORD pc, BYTE address, BYTE write, BYTE value);
BYTE VideoSetMode(WORD pc, BYTE address, BYTE write, BYTE value);
