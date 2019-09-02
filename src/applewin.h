#pragma once

#define  BUILDNUMBER       4
#define  VERSIONMAJOR      1
#define  VERSIONMINOR      11

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
#define  TITLE             "Apple //e Emulator"

#define  MAX(a,b)          (((a) > (b)) ? (a) : (b))
#define  MIN(a,b)          (((a) < (b)) ? (a) : (b))

namespace detail {
    template <typename T, int N>
    char (&arr(const T(&)[N]))[N];
}

#define ARRSIZE(x)          (sizeof(detail::arr(x)))

typedef BYTE (* iofunction)(WORD, BYTE, BYTE, BYTE);

typedef struct _Image_ {
    int unused;
} *HIMAGE;

struct regsrec {
    BYTE a;   // accumulator
    BYTE x;   // index X
    BYTE y;   // index Y
    BYTE ps;  // processor status
    WORD pc;  // program counter
    WORD sp;  // stack pointer
};

extern BOOL       apple2e;
extern BOOL       autoboot;
extern BOOL       behind;
extern DWORD      cumulativecycles;
extern DWORD      cyclenum;
extern DWORD      emulmsec;
extern DWORD      extbench;
extern BOOL       fullspeed;
extern HINSTANCE  instance;
extern HWND       framewindow;
extern BOOL       graphicsmode;
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
extern char       progdir[MAX_PATH];
extern regsrec    regs;
extern BOOL       resettiming;
extern BOOL       restart;
extern DWORD      serialport;
extern DWORD      soundtype;
extern DWORD      speed;

void         CommDestroy();
void         CommReset();
void         CommSetSerialPort(HWND window, DWORD newserialport);
void         CommUpdate(DWORD totalcycles);
void         CpuDestroy();
DWORD        CpuExecute(DWORD cycles);
void         CpuInitialize();
void         CpuSetupBenchmark();
void         DebugBegin();
void         DebugContinueStepping();
void         DebugDestroy();
void         DebugDisplay(BOOL drawbackground);
void         DebugEnd();
void         DebugInitialize();
void         DebugProcessChar(char ch);
void         DebugProcessCommand(int keycode);
void         DiskBoot();
void         DiskDestroy();
void         DiskGetLightStatus(int * drive1, int * drive2);
const char * DiskGetName(int drive);
BOOL         DiskInitialize();
BOOL         DiskIsSpinning();
void         DiskSelect(int drive);
void         DiskUpdatePosition(DWORD cycles);
void         FrameCreateWindow();
HDC          FrameGetDC();
void         FrameRefreshStatus();
void         FrameRegisterClass();
void         FrameReleaseDC(HDC dc);
BOOL         ImageBoot(HIMAGE image);
void         ImageClose(HIMAGE image);
void         ImageDestroy();
void         ImageInitialize();
BOOL         ImageOpen(const char * imagefilename, HIMAGE * image, BOOL * writeprotected, BOOL createifnecessary);
void         ImageReadTrack(HIMAGE image, int track, int quartertrack, LPBYTE trackimage, int * nibbles);
void         ImageWriteTrack(HIMAGE image, int track, int quartertrack, LPBYTE trackimage, int nibbles);
void         JoyInitialize();
BOOL         JoyProcessKey(int virtkey, BOOL extended, BOOL down, BOOL autorep);
void         JoyReset();
void         JoySetButton(int number, BOOL down);
BOOL         JoySetEmulationType(HWND window, DWORD newtype);
void         JoySetPosition(int xvalue, int xrange, int yvalue, int yrange);
void         JoyUpdatePosition(DWORD cycles);
BOOL         JoyUsingMouse();
void         KeybGetCapsStatus(BOOL * status);
BYTE         KeybGetKeycode();
DWORD        KeybGetNumQueries();
void         KeybQueueKeypress(int virtkey, BOOL extended);
void         MemDestroy();
LPBYTE       MemGetAuxPtr(WORD offset);
LPBYTE       MemGetMainPtr(WORD offset);
void         MemInitialize();
void         MemReset();
void         MemResetPaging();
BYTE         MemReturnRandomData(BOOL highbit);
BOOL         RegLoadString(const char * section, const char * key, char * buffer, DWORD chars);
BOOL         RegLoadValue(const char * section, const char * key, DWORD * value);
void         RegSaveString(const char * section, const char * key, const char * value);
void         RegSaveValue(const char * section, const char * key, DWORD value);
DWORD        SpkrCyclesSinceSound();
void         SpkrDestroy();
void         SpkrInitialize();
BOOL         SpkrNeedsAccurateCycleCount();
BOOL         SpkrNeedsFineGrainTiming();
BOOL         SpkrSetEmulationType(HWND window, DWORD newtype);
void         SpkrUpdate(DWORD totalcycles);
int          StrCat(char * dst, const char * src, size_t dstChars);
char *       StrChr(char * str, char ch);
const char * StrChr(const char * str, char ch);
int          StrCmp(const char * str1, const char * str2);
int          StrCmpI(const char * str1, const char * str2);
int          StrCmpLen(const char * str1, const char * str2, size_t maxchars);
int          StrCmpLenI(const char * str1, const char * str2, size_t maxchars);
int          StrCopy(char * dst, const char * src, size_t dstChars);
int          StrLen(const char * str);
int          StrPrintf(char * dest, size_t dstChars, const char * format, ...);
char *       StrStr(char * str1, const char * str2);
const char * StrStr(const char * str1, const char * str2);
char *       StrTok(char * str, const char * delimiter, char ** context);
unsigned int StrToUnsigned(char * str, char ** endPtr, int base);
BOOL         VideoApparentlyDirty();
void         VideoBenchmark();
void         VideoCheckPage(BOOL force);
void         VideoDestroy();
void         VideoDestroyLogo();
void         VideoDisplayLogo();
void         VideoDisplayMode(BOOL flashon);
BOOL         VideoHasRefreshed();
void         VideoInitialize();
void         VideoLoadLogo();
void         VideoRealizePalette(HDC dc);
void         VideoRedrawScreen();
void         VideoRefreshScreen();
void         VideoReinitialize();
void         VideoReleaseFrameDC();
void         VideoResetState();
void         VideoTestCompatibility();
void         VideoUpdateVbl(DWORD cycles, BOOL nearrefresh);

BYTE CommCommand(WORD, BYTE, BYTE, BYTE);
BYTE CommControl(WORD, BYTE, BYTE, BYTE);
BYTE CommDipSw(WORD, BYTE, BYTE, BYTE);
BYTE CommReceive(WORD, BYTE, BYTE, BYTE);
BYTE CommStatus(WORD, BYTE, BYTE, BYTE);
BYTE CommTransmit(WORD, BYTE, BYTE, BYTE);
BYTE DiskControlMotor(WORD, BYTE, BYTE, BYTE);
BYTE DiskControlStepper(WORD, BYTE, BYTE, BYTE);
BYTE DiskEnable(WORD, BYTE, BYTE, BYTE);
BYTE DiskReadWrite(WORD, BYTE, BYTE, BYTE);
BYTE DiskSetLatchValue(WORD, BYTE, BYTE, BYTE);
BYTE DiskSetReadMode(WORD, BYTE, BYTE, BYTE);
BYTE DiskSetWriteMode(WORD, BYTE, BYTE, BYTE);
BYTE KeybReadData(WORD, BYTE, BYTE, BYTE);
BYTE KeybReadFlag(WORD, BYTE, BYTE, BYTE);
BYTE JoyReadButton(WORD, BYTE, BYTE, BYTE);
BYTE JoyReadPosition(WORD, BYTE, BYTE, BYTE);
BYTE JoyResetPosition(WORD, BYTE, BYTE, BYTE);
BYTE MemCheckPaging(WORD, BYTE, BYTE, BYTE);
BYTE MemSetPaging(WORD, BYTE, BYTE, BYTE);
BYTE SpkrToggle(WORD, BYTE, BYTE, BYTE);
BYTE VideoCheckMode(WORD, BYTE, BYTE, BYTE);
BYTE VideoCheckVbl(WORD, BYTE, BYTE, BYTE);
BYTE VideoSetMode(WORD, BYTE, BYTE, BYTE);
