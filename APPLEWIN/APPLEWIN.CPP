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

BOOL      apple2e           = 1;
BOOL      autoboot          = 0;
BOOL      behind            = 0;
DWORD     calibrating       = 0;
DWORD     clockgran         = 0;
DWORD     cumulativecycles  = 0;
DWORD     cyclegran         = 0;
DWORD     cyclenum          = 0;
DWORD     emulmsec          = 0;
DWORD     finegraindelay    = 1;
BOOL      fullspeed         = 0;
BOOL      i386              = 0;
HINSTANCE instance          = (HINSTANCE)0;
DWORD     lastfastpaging    = 0;
DWORD     lasttrimimages    = 0;
int       mode              = MODE_LOGO;
DWORD     needsprecision    = 0;
BOOL      optenhancedisk    = 1;
BOOL      optmonochrome     = 0;
BOOL      optpopuplabels    = 1;
TCHAR     progdir[MAX_PATH] = TEXT("");
BOOL      resettiming       = 0;
BOOL      restart           = 0;
DWORD     speed             = 10;
BOOL      win31             = 0;

//===========================================================================
void CheckFastPaging () {
  if ((pages >= 10) && CpuSupportsFastPaging()) {
    lastfastpaging = cumulativecycles;
    if (cpuemtype == CPU_COMPILING) {
      lasttrimimages = cumulativecycles;
      MemSetFastPaging(1);
    }
  }
}

//===========================================================================
void CheckCpuType () {
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  i386 = (sysinfo.dwProcessorType == PROCESSOR_INTEL_386);
}

//===========================================================================
void CheckWindowsVersion () {
  win31 = ((LOBYTE(LOWORD(GetVersion())) == 3) &&
           (HIBYTE(LOWORD(GetVersion())) <= 0x20));
}

//===========================================================================
void ContinueExecution () {
  static BOOL finegrainlast   = 1;
  static BOOL finegraintiming = 1;
  static BOOL normaldelays    = 0;
  static BOOL pageflipping    = 0;

  // RUN THE CPU, DISK, AND JOYSTICK TIMERS FOR ONE CLOCK TICK'S
  // WORTH OF CYCLES
  BOOL skippedfinegrain = 0;
  BOOL ranfinegrain     = 0;
  {
    int   loop        = 1+(cyclegran >= 20000);
    DWORD cyclestorun = cyclegran >> (cyclegran >= 20000);
    while (loop--) {
      cyclenum = 0;
      if (((cumulativecycles-needsprecision) > 1500000) && !finegraintiming)
        do {
          DWORD executedcycles = CpuExecute(2000);
          cyclenum += executedcycles;
          CheckFastPaging();
          DiskUpdatePosition(executedcycles);
          JoyUpdatePosition(executedcycles);
          VideoUpdateVbl(executedcycles,
                         (cyclestorun-cyclenum) <= 2000);
        } while (cyclenum < cyclestorun);
      else {
        DWORD cyclesneeded = 0;
        do {
          DWORD startcycles = cyclenum;
          if (SpkrNeedsAccurateCycleCount()) {
            int loop2 = 3;
            while (loop2--) {
              cyclesneeded += 33;
              if (cyclenum < cyclesneeded)
                cyclenum += CpuExecute(cyclesneeded-cyclenum);
            }
          }
          else {
            cyclesneeded += 100;
            if (cyclenum < cyclesneeded)
              cyclenum += CpuExecute(cyclesneeded-cyclenum);
          }
          CheckFastPaging();
          DiskUpdatePosition(cyclenum-startcycles);
          JoyUpdatePosition(cyclenum-startcycles);
          VideoUpdateVbl(cyclenum-startcycles,
                         (cyclestorun-cyclesneeded) < 1000);
          if (finegraintiming && finegrainlast)
            if ((calibrating == 0) && (SpkrCyclesSinceSound() > 50000))
              skippedfinegrain = 1;
            else {
              ranfinegrain = 1;
              DWORD loop = finegraindelay;
              while (loop--) ;
            }
        } while (cyclesneeded < cyclestorun);
      }
      cumulativecycles += cyclenum;
      SpkrUpdate(cyclenum);
      CommUpdate(cyclenum);
    }
  }
  emulmsec += clockgran;
  if (cpuemtype == CPU_FASTPAGING)
    if ((!pages) && (cumulativecycles-lastfastpaging > 500000))
      MemSetFastPaging(0);
    else if (cumulativecycles-lasttrimimages > 500000) {
      MemTrimImages();
      lasttrimimages = cumulativecycles;
    }
  pages = 0;

  // DETERMINE WHETHER THE SCREEN WAS UPDATED, THE DISK WAS SPINNING,
  // OR THE KEYBOARD I/O PORTS WERE BEING EXCESSIVELY QUERIED THIS
  // CLOCKTICK
  VideoCheckPage(0);
  BOOL diskspinning  = DiskIsSpinning();
  BOOL screenupdated = VideoHasRefreshed();
  BOOL systemidle    = (KeybGetNumQueries() > (clockgran << 2))
                         && (calibrating == 0) && (!ranfinegrain);
  fullspeed = ((speed == SPEED_MAX) || (GetKeyState(VK_SCROLL) < 0)) && (calibrating == 0);
  if (screenupdated)
    pageflipping = 3;

  // IF A TWENTIETH OF A SECOND HAS ELAPSED AND THE SCREEN HAS NOT BEEN
  // UPDATED BUT IT APPEARS TO NEED UPDATING, THEN REFRESH IT
  if (mode != MODE_LOGO) {
    static BOOL  anyupdates     = 0;
    static DWORD lastcycles     = 0;
    static BOOL  lastupdates[2] = {0,0};
    anyupdates |= screenupdated;
    if ((cumulativecycles-lastcycles) >=
          (DWORD)(50000 << (behind || fullspeed || (calibrating > 0 ? 1 : 0) || ranfinegrain))) {
      lastcycles = cumulativecycles;
      if ((!anyupdates) && (!lastupdates[0]) && (!lastupdates[1]) &&
          VideoApparentlyDirty()) {
        VideoCheckPage(1);
        static DWORD lasttime = 0;
        DWORD currtime = GetTickCount();
        if ((!fullspeed) ||
            (currtime-lasttime >= (DWORD)((graphicsmode || !systemidle) ? 100 : 25))) {
          VideoRefreshScreen();
          lasttime = currtime;
        }
        screenupdated = 1;
      }
      lastupdates[1] = lastupdates[0];
      lastupdates[0] = anyupdates;
      anyupdates     = 0;
      if (pageflipping)
        pageflipping--;
    }
  }

  // IF WE ARE CURRENTLY CALIBRATING THE SYSTEM THEN ADJUST FINE
  // GRAIN TIMING
  if (calibrating > 0) {
    static DWORD lastnormaldelays = 1;
    static DWORD lastcycles       = 0;
    static DWORD lastdelay        = 1;
    if (cumulativecycles-lastcycles >= 100000) {
      lastcycles = cumulativecycles;
      if (finegraintiming && finegrainlast)
        if (normaldelays+lastnormaldelays <= (DWORD)(clockgran <= 25))
          if (calibrating < 16) {
            calibrating++;
            finegraindelay = lastdelay;
            normaldelays   = 1;
            resettiming    = 1;
          }
          else
            calibrating = 0;
        else {
          lastdelay = finegraindelay;
		  finegraindelay += (1 << (16 - calibrating));
        }
      lastnormaldelays = normaldelays;
      normaldelays     = 0;
    }
  }

  // IF WE ARE NOT CALIBRATING THE SYSTEM, TURN FINE GRAIN TIMING
  // ON OR OFF BASED ON WHETHER THE DISK IS SPINNING, THE SCREEN IS
  // BEING UPDATED, OR THE SYSTEM IS IDLE
  finegrainlast   = finegraintiming;
  finegraintiming = (calibrating > 0) ||
                    ((!systemidle) && (!diskspinning) && (!fullspeed) &&
                     (!pageflipping) && (!screenupdated) && 
                     SpkrNeedsFineGrainTiming() && !VideoApparentlyDirty());

  // COMPARE THE EMULATOR'S CLOCK TO THE REAL TIME CLOCK
  {
    static DWORD milliseconds = 0;
    static DWORD microseconds = 0;
    DWORD currtime = GetTickCount();
    if ((!fullspeed) &&
        ((!optenhancedisk) || (!diskspinning)) &&
        (!resettiming)) {
      if ((speed == SPEED_NORMAL) || (calibrating > 0))
        milliseconds += clockgran;
      else {
        DWORD delta = (DWORD)((clockgran*2000.0)/pow(2.0,speed/10.0));
        milliseconds += (delta/1000);
        microseconds += (delta % 1000);
        if (microseconds >= 1000) {
          microseconds -= 1000;
          ++milliseconds;
        }
      }

      // DETERMINE WHETHER WE ARE AHEAD OF OR BEHIND REAL TIME
      if (currtime > milliseconds+1000) {
        behind       = 0;
        milliseconds = currtime;
      }
      else if ((currtime >= milliseconds+100) && (calibrating == 0))
        behind = 1;
      else if (milliseconds > currtime) {
        static DWORD ahead     = 0;
        static DWORD lastreset = 0;
        ahead += milliseconds-currtime;
        if (currtime-lastreset > 500) {
          if (ahead >= 200)
            behind = 0;
          ahead     = 0;
          lastreset = currtime;
        }
      }

      // IF WE ARE AHEAD OF REAL TIME, WAIT FOR REAL TIME TO CATCH UP
      if ((milliseconds-currtime > 0) &&
          (milliseconds-currtime < 200)) {
        ++normaldelays;
        if ((skippedfinegrain || !finegraintiming) && !behind)
          Sleep(1);
        do {
          if (systemidle && !behind)
            Sleep(1);
          currtime = GetTickCount();
        } while ((milliseconds-currtime > 0) &&
                 (milliseconds-currtime < 200));
      }
      else if (currtime-milliseconds > 250)
        milliseconds += 100;

    }
    else {
      behind       = fullspeed;
      milliseconds = currtime;
      resettiming  = 0;
    }
  }

}

//===========================================================================
void DetermineClockGranularity () {
  clockgran = 50;
  DWORD oldticks = GetTickCount();
  int   loop     = 40;
  while (loop-- && (clockgran >= 20)) {
    DWORD newticks;
    do {
      newticks = GetTickCount();
	} while (oldticks >= newticks);
    if (newticks-oldticks >= 10)
      clockgran = MIN(clockgran,newticks-oldticks);
    oldticks = newticks;
  }
  cyclegran = clockgran*1000;
}

//===========================================================================
LRESULT CALLBACK DlgProc (HWND   window,
                          UINT   message,
                          WPARAM wparam,
                          LPARAM lparam) {
  if (message == WM_CREATE) {
    RECT rect;
    GetWindowRect(window,&rect);
    SIZE size;
    size.cx = rect.right-rect.left;
    size.cy = rect.bottom-rect.top;
    rect.left   = (GetSystemMetrics(SM_CXSCREEN)-size.cx) >> 1;
    rect.top    = (GetSystemMetrics(SM_CYSCREEN)-size.cy) >> 1;
    rect.right  = rect.left+size.cx;
    rect.bottom = rect.top +size.cy;
    MoveWindow(window,
	       rect.left,
	       rect.top,
	       rect.right-rect.left,
	       rect.bottom-rect.top,
	       0);
    //Ctl3dSubclassDlgEx(window,0xFFFF);
    ShowWindow(window,SW_SHOW);
  }
  if (win31 && (message == WM_CTLCOLORSTATIC)) {
    SetBkColor((HDC)wparam,0xC0C0C0);
    return (LRESULT)GetStockObject(LTGRAY_BRUSH);
  }
  return DefWindowProc(window,message,wparam,lparam);
}

//===========================================================================
void EnterMessageLoop () {
  MSG message;
  while (GetMessage(&message,0,0,0)) {
    TranslateMessage(&message);
    DispatchMessage(&message);
    while ((mode == MODE_RUNNING) || (mode == MODE_STEPPING) || (calibrating > 0))
      if (PeekMessage(&message,0,0,0,PM_REMOVE)) {
        if (message.message == WM_QUIT)
          return;
        TranslateMessage(&message);
        DispatchMessage(&message);
      }
      else if (mode == MODE_STEPPING)
        DebugContinueStepping();
      else {
        ContinueExecution();
        if (fullspeed)
          ContinueExecution();
      }
  }
  while (PeekMessage(&message,0,0,0,PM_REMOVE)) ;
}

//===========================================================================
void GetProgramDirectory () {
  GetModuleFileName((HINSTANCE)0,progdir,MAX_PATH);
  progdir[MAX_PATH-1] = 0;
  int loop = _tcslen(progdir);
  while (loop--)
    if ((progdir[loop] == TEXT('\\')) ||
        (progdir[loop] == TEXT(':'))) {
      progdir[loop+1] = 0;
      break;
    }
}

//===========================================================================
BOOL LoadCalibrationData () {
#define LOAD(a,b,c) if (!RegLoadValue(a,b,1,c)) return 0;
  DWORD buildnumber = 0;
  DWORD runningonos = 0;
  LOAD(TEXT(""),TEXT("CurrentBuildNumber"),&buildnumber);
  LOAD(TEXT(""),TEXT("RunningOnOS"),&runningonos);
  if (buildnumber != BUILDNUMBER)
    return 0;
  if (runningonos != GetVersion())
    return 0;
  LOAD(TEXT("Calibration"),TEXT("Clock Granularity"),&clockgran);
  LOAD(TEXT("Calibration"),TEXT("Cycle Granularity"),&cyclegran);
  LOAD(TEXT("Calibration"),TEXT("Precision Timing") ,&finegraindelay);
#undef LOAD
  return (clockgran && cyclegran && finegraindelay);
}

//===========================================================================
void LoadConfiguration () {
#define LOAD(a,b) RegLoadValue(TEXT("Configuration"),a,0,b);
  LOAD(TEXT("Computer Emulation")  ,(DWORD *)&apple2e);
  LOAD(TEXT("Joystick Emulation")  ,&joytype);
  LOAD(TEXT("Sound Emulation")     ,&soundtype);
  LOAD(TEXT("Serial Port")         ,&serialport);
  LOAD(TEXT("Emulation Speed")     ,&speed);
  LOAD(TEXT("Enhance Disk Speed")  ,(DWORD *)&optenhancedisk);
  LOAD(TEXT("Monochrome Video")    ,(DWORD *)&optmonochrome);
#undef LOAD
}

//===========================================================================
void PerformCalibration () {

  // REGISTER THE WINDOW CLASS OF THE CALIBRATION DIALOG BOX
  WNDCLASS wndclass;
  ZeroMemory(&wndclass,sizeof(WNDCLASS));
  wndclass.lpfnWndProc   = DlgProc;
  wndclass.cbWndExtra    = DLGWINDOWEXTRA;
  wndclass.hInstance     = instance;
  wndclass.hIcon         = LoadIcon(instance,TEXT("APPLEWIN_ICON"));
  wndclass.hCursor       = LoadCursor(0,IDC_WAIT);
  wndclass.hbrBackground = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
  wndclass.lpszClassName = TEXT("APPLE2CALIBRATION");
  RegisterClass(&wndclass);

  // CREATE THE CALIBRATION DIALOG BOX
  HWND dlgwindow = CreateDialog(instance,
                                TEXT("CALIBRATION_DIALOG"),
                                (HWND)0,
                                NULL);

  // PROCESS MESSAGES UNTIL THE DIALOG BOX IS FULLY DRAWN
  MSG message;
  while (PeekMessage(&message,0,0,0,PM_REMOVE)) {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  // WAIT ONE SECOND FOR DISK ACTIVITY TO STOP
  Sleep(1000);

  // SET UP MEMORY FOR THE CALIBRATION
  FillMemory(mem,0x102,0x3E);
  *(mem+0x102) = 0x4C;
  *(mem+0x103) = 0x00;
  *(mem+0x104) = 0x00;
  regs.pc = 0;

  // PERFORM THE CALIBRATION
  calibrating    = 1;
  finegraindelay = 1;
  while (calibrating > 0)
    ContinueExecution();

  // CLOSE THE DIALOG BOX
  PostMessage(dlgwindow,WM_CLOSE,0,0);

}

//===========================================================================
void RegisterExtensions () {
  TCHAR command[MAX_PATH];
  GetModuleFileName((HMODULE)0,command,MAX_PATH);
  command[MAX_PATH-1] = 0;
  TCHAR icon[MAX_PATH];
  wsprintf(icon,TEXT("%s,1"),(LPCTSTR)command);
  _tcscat(command,TEXT(" %1"));
  RegSetValue(HKEY_CLASSES_ROOT,".bin",REG_SZ,"DiskImage",10);
  RegSetValue(HKEY_CLASSES_ROOT,".do" ,REG_SZ,"DiskImage",10);
  RegSetValue(HKEY_CLASSES_ROOT,".dsk",REG_SZ,"DiskImage",10);
  RegSetValue(HKEY_CLASSES_ROOT,".nib",REG_SZ,"DiskImage",10);
  RegSetValue(HKEY_CLASSES_ROOT,".po" ,REG_SZ,"DiskImage",10);
  RegSetValue(HKEY_CLASSES_ROOT,
              "DiskImage",
              REG_SZ,"Disk Image",21);
  RegSetValue(HKEY_CLASSES_ROOT,
              "DiskImage\\DefaultIcon",
              REG_SZ,icon,_tcslen(icon)+1);
  RegSetValue(HKEY_CLASSES_ROOT,
              "DiskImage\\shell\\open\\command",
              REG_SZ,command,_tcslen(command)+1);
}

//===========================================================================
void SaveCalibrationData () {
  RegSaveValue(TEXT(""),TEXT("CurrentBuildNumber"),1,BUILDNUMBER);
  RegSaveValue(TEXT(""),TEXT("RunningOnOS")       ,1,GetVersion());
  RegSaveValue(TEXT("Calibration"),TEXT("Clock Granularity"),1,clockgran);
  RegSaveValue(TEXT("Calibration"),TEXT("Cycle Granularity"),1,cyclegran);
  RegSaveValue(TEXT("Calibration"),TEXT("Precision Timing") ,1,finegraindelay);
}

//===========================================================================
int APIENTRY WinMain (HINSTANCE passinstance, HINSTANCE, LPSTR, int) {

  // DO ONE-TIME INITIALIZATION
  instance = passinstance;
  CheckCpuType();
  CheckWindowsVersion();
  GdiSetBatchLimit(512);
  GetProgramDirectory();
  RegisterExtensions();
  //Ctl3dRegister(instance);
  FrameRegisterClass();
  ImageInitialize();
  if (!DiskInitialize())
    return 1;

  do {

    // DO INITIALIZATION THAT MUST BE REPEATED FOR A RESTART
    restart = 0;
    mode    = MODE_LOGO;
    LoadConfiguration();
    DebugInitialize();
    JoyInitialize();
    MemInitialize();
    VideoInitialize();
    if (!LoadCalibrationData()) {
      DetermineClockGranularity();
      PerformCalibration();
      SaveCalibrationData();
      MemDestroy();
      MemInitialize();
    }
    FrameCreateWindow();

    // ENTER THE MAIN MESSAGE LOOP
    EnterMessageLoop();

  } while (restart);
  return 0;
}
