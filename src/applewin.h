#pragma once

#define  BUILDNUMBER       4
#define  VERSIONSTRING     TEXT("1.10")

#define  CPU_COMPILING     0
#define  CPU_INTERPRETIVE  1
#define  CPU_FASTPAGING    2

#define  MODE_LOGO         0
#define  MODE_PAUSED       1
#define  MODE_RUNNING      2
#define  MODE_DEBUG        3
#define  MODE_STEPPING     4

#define  SPEED_NORMAL      10
#define  SPEED_MAX         40

#define  VIEWPORTX         5
#define  VIEWPORTY         5

#define  MAXIMAGES         16
#define  TITLE             TEXT("Apple //e Emulator")

#define  MAX(a,b)          (((a) > (b)) ? (a) : (b))
#define  MIN(a,b)          (((a) < (b)) ? (a) : (b))

#ifndef HFINDFILE
#ifndef WIN40
#define HFINDFILE   HANDLE
#endif  // ifndef WIN40
#endif  // ifndef HFINDFILE

typedef BYTE(__stdcall * iofunction)(WORD, BYTE, BYTE, BYTE);

typedef struct _IMAGE__ {
    int unused;
} *HIMAGE;

typedef struct _regsrec {
    BYTE a;   // accumulator
    BYTE x;   // index X
    BYTE y;   // index Y
    BYTE ps;  // processor status
    WORD pc;  // program counter
    WORD sp;  // stack pointer
} regsrec, * regsptr;

extern BOOL       apple2e;
extern BOOL       autoboot;
extern BOOL       behind;
extern DWORD      cpuemtype;
extern DWORD      cumulativecycles;
extern DWORD      cyclenum;
extern DWORD      emulmsec;
extern DWORD      extbench;
extern BOOL       fullspeed;
extern HINSTANCE  instance;
extern HWND       framewindow;
extern BOOL       graphicsmode;
extern BOOL       i386;
extern DWORD      image;
extern iofunction ioread[0x100];
extern iofunction iowrite[0x100];
extern DWORD      lastimage;
extern DWORD      joytype;
extern LPBYTE     mem;
extern LPBYTE     memaux;
extern LPBYTE     memdirty;
extern LPBYTE     memmain;
extern LPBYTE     memshadow[MAXIMAGES][0x100];
extern LPBYTE     memwrite[MAXIMAGES][0x100];
extern int        mode;
extern DWORD      needsprecision;
extern BOOL       optenhancedisk;
extern BOOL       optmonochrome;
extern DWORD      pages;
extern TCHAR      progdir[MAX_PATH];
extern regsrec    regs;
extern BOOL       resettiming;
extern BOOL       restart;
extern DWORD      serialport;
extern DWORD      soundtype;
extern DWORD      speed;

void    CommDestroy();
void    CommReset();
void    CommSetSerialPort(HWND, DWORD);
void    CommUpdate(DWORD);
void    CpuDestroy();
DWORD   CpuExecute(DWORD);
void    CpuGetCode(WORD, LPBYTE *, DWORD *);
void    CpuInitialize();
void    CpuReinitialize();
void    CpuResetCompilerData();
void    CpuSetupBenchmark();
BOOL    CpuSupportsFastPaging();
void    DebugBegin();
void    DebugContinueStepping();
void    DebugDestroy();
void    DebugDisplay(BOOL);
void    DebugEnd();
void    DebugInitialize();
void    DebugProcessChar(TCHAR);
void    DebugProcessCommand(int);
void    DiskBoot();
void    DiskDestroy();
void    DiskGetLightStatus(int *, int *);
LPCTSTR DiskGetName(int);
BOOL    DiskInitialize();
BOOL    DiskIsSpinning();
void    DiskSelect(int);
void    DiskUpdatePosition(DWORD);
void    FrameCreateWindow();
HDC     FrameGetDC();
void    FrameRefreshStatus();
void    FrameRegisterClass();
void    FrameReleaseDC(HDC);
void    KeybGetCapsStatus(BOOL *);
BYTE    KeybGetKeycode();
DWORD   KeybGetNumQueries();
void    KeybQueueKeypress(int, BOOL);
BOOL    ImageBoot(HIMAGE);
void    ImageClose(HIMAGE);
void    ImageDestroy();
void    ImageInitialize();
BOOL    ImageOpen(LPCTSTR, HIMAGE *, BOOL *, BOOL);
void    ImageReadTrack(HIMAGE, int, int, LPBYTE, int *);
void    ImageWriteTrack(HIMAGE, int, int, LPBYTE, int);
void    JoyInitialize();
BOOL    JoyProcessKey(int, BOOL, BOOL, BOOL);
void    JoyReset();
void    JoySetButton(int, BOOL);
BOOL    JoySetEmulationType(HWND, DWORD);
void    JoySetPosition(int, int, int, int);
void    JoyUpdatePosition(DWORD);
BOOL    JoyUsingMouse();
void    MemDestroy();
LPBYTE  MemGetAuxPtr(WORD);
LPBYTE  MemGetMainPtr(WORD);
void    MemInitialize();
void    MemReset();
void    MemResetPaging();
BYTE    MemReturnRandomData(BYTE highbit);
void    MemSetFastPaging(BOOL);
void    MemTrimImages();
BOOL    RegLoadString(LPCTSTR, LPCTSTR, BOOL, LPTSTR, DWORD);
BOOL    RegLoadValue(LPCTSTR, LPCTSTR, BOOL, DWORD *);
void    RegSaveString(LPCTSTR, LPCTSTR, BOOL, LPCTSTR);
void    RegSaveValue(LPCTSTR, LPCTSTR, BOOL, DWORD);
DWORD   SpkrCyclesSinceSound();
void    SpkrDestroy();
void    SpkrInitialize();
BOOL    SpkrNeedsAccurateCycleCount();
BOOL    SpkrNeedsFineGrainTiming();
BOOL    SpkrSetEmulationType(HWND, DWORD);
void    SpkrUpdate(DWORD);
BOOL    VideoApparentlyDirty();
void    VideoBenchmark();
void    VideoCheckPage(BOOL);
void    VideoDestroy();
void    VideoDestroyLogo();
void    VideoDisplayLogo();
void    VideoDisplayMode(BOOL);
BOOL    VideoHasRefreshed();
void    VideoInitialize();
void    VideoLoadLogo();
void    VideoRealizePalette(HDC);
void    VideoRedrawScreen();
void    VideoRefreshScreen();
void    VideoReinitialize();
void    VideoReleaseFrameDC();
void    VideoResetState();
void    VideoTestCompatibility();
void    VideoUpdateVbl(DWORD, BOOL);

BYTE __stdcall CommCommand(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall CommControl(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall CommDipSw(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall CommReceive(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall CommStatus(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall CommTransmit(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall DiskControlMotor(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall DiskControlStepper(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall DiskEnable(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall DiskReadWrite(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall DiskSetLatchValue(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall DiskSetReadMode(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall DiskSetWriteMode(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall KeybReadData(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall KeybReadFlag(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall JoyReadButton(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall JoyReadPosition(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall JoyResetPosition(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall MemCheckPaging(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall MemSetPaging(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall SpkrToggle(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall VideoCheckMode(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall VideoCheckVbl(WORD, BYTE, BYTE, BYTE);
BYTE __stdcall VideoSetMode(WORD, BYTE, BYTE, BYTE);
