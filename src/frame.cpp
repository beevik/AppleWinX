/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

constexpr int VIEWPORTCX  = 560;
constexpr int VIEWPORTCY  = 384;
constexpr int BUTTONCX    = 45;
constexpr int BUTTONCY    = 45;
constexpr int BUTTONS     = 8;

constexpr int BTN_HELP    = 0;
constexpr int BTN_RUN     = 1;
constexpr int BTN_DRIVE1  = 2;
constexpr int BTN_DRIVE2  = 3;
constexpr int BTN_TOFILE  = 4;
constexpr int BTN_TODISK  = 5;
constexpr int BTN_DEBUG   = 6;
constexpr int BTN_SETUP   = 7;

static const char computerchoices[] = "Apple ][+\0"
                                      "Apple //e\0";
static const char joystickchoices[] = "Disabled\0"
                                      "PC Joystick\0"
                                      "Keyboard (standard)\0"
                                      "Keyboard (centering)\0"
                                      "Mouse\0";
static const char serialchoices[] =   "None\0"
                                      "COM1\0"
                                      "COM2\0"
                                      "COM3\0"
                                      "COM4\0";
static const char soundchoices[] =    "Disabled\0"
                                      "PC Speaker (direct)\0"
                                      "PC Speaker (translated)\0"
                                      "Sound Card\0";
static const char videochoices[] =    "Color\0"
                                      "Monochrome\0";

static HBITMAP capsbitmap[2];
static HBITMAP diskbitmap[3];
static HBITMAP buttonbitmap[BUTTONS];

HWND framewindow = (HWND)0;

static HBRUSH  btnfacebrush    = (HBRUSH)0;
static HPEN    btnfacepen      = (HPEN)0;
static HPEN    btnhighlightpen = (HPEN)0;
static HPEN    btnshadowpen    = (HPEN)0;
static int     buttonactive    = -1;
static int     buttondown      = -1;
static HRGN    clipregion      = (HRGN)0;
static BOOL    helpquit        = 0;
static HFONT   smallfont       = (HFONT)0;
static BOOL    usingcursor     = 0;

static void    DrawStatusArea(HDC passdc, BOOL drawbackground);
static void    EnableTrackbar(HWND window, BOOL enable);
static void    FillComboBox(HWND window, int controlid, const char * choices, int currentchoice);
static HBITMAP LoadButtonBitmap(HINSTANCE instance, const char * bitmapname);
static void    ProcessButtonClick(int button);
static void    ResetMachineState();
static void    SetUsingCursor(BOOL);

//===========================================================================
static BOOL CALLBACK ConfigDlgProc(
    HWND   window,
    UINT   message,
    WPARAM wparam,
    LPARAM lparam
) {
    static BOOL afterclose = 0;
    switch (message) {
        case WM_COMMAND:
            switch (LOWORD(wparam)) {

                case IDOK:
                {
                    BOOL  newcomptype = (BOOL)SendDlgItemMessage(window, 101, CB_GETCURSEL, 0, 0);
                    BOOL  newvidtype = (BOOL)SendDlgItemMessage(window, 105, CB_GETCURSEL, 0, 0);
                    DWORD newjoytype = (DWORD)SendDlgItemMessage(window, 102, CB_GETCURSEL, 0, 0);
                    DWORD newsoundtype = (DWORD)SendDlgItemMessage(window, 103, CB_GETCURSEL, 0, 0);
                    DWORD newserialport = (DWORD)SendDlgItemMessage(window, 104, CB_GETCURSEL, 0, 0);
                    if (newcomptype != apple2e)
                        if (MessageBox(window,
                            "You have changed the emulated computer "
                            "type.  This change will not take effect "
                            "until the next time you restart the "
                            "emulator.\n\n"
                            "Would you like to restart the emulator now?",
                            "Configuration",
                            MB_ICONQUESTION | MB_YESNO) == IDYES)
                            afterclose = 2;
                    if (!JoySetEmulationType(window, newjoytype)) {
                        afterclose = 0;
                        return 0;
                    }
                    if (!SpkrSetEmulationType(window, newsoundtype)) {
                        afterclose = 0;
                        return 0;
                    }
                    if (optmonochrome != newvidtype) {
                        optmonochrome = newvidtype;
                        VideoReinitialize();
                        VideoRedrawScreen();
                    }
                    CommSetSerialPort(window, newserialport);
                    if (IsDlgButtonChecked(window, 106))
                        speed = SPEED_NORMAL;
                    else
                        speed = (DWORD)SendDlgItemMessage(window, 108, TBM_GETPOS, 0, 0);
#define SAVE(a,b) RegSaveValue("Configuration",a,b);
                    SAVE("Computer Emulation", newcomptype);
                    SAVE("Joystick Emulation", joytype);
                    SAVE("Sound Emulation", soundtype);
                    SAVE("Serial Port", serialport);
                    SAVE("Custom Speed", IsDlgButtonChecked(window, 107));
                    SAVE("Emulation Speed", speed);
                    SAVE("Monochrome Video", optmonochrome);
#undef SAVE
                }
                EndDialog(window, 1);
                if (afterclose)
                    PostMessage(framewindow, WM_USER + afterclose, 0, 0);
                break;

                case IDCANCEL:
                    EndDialog(window, 0);
                    break;

                case 106:
                    SendDlgItemMessage(window, 108, TBM_SETPOS, 1, SPEED_NORMAL);
                    EnableTrackbar(window, 0);
                    break;

                case 107:
                    SetFocus(GetDlgItem(window, 108));
                    EnableTrackbar(window, 1);
                    break;

                case 108:
                    CheckRadioButton(window, 106, 107, 107);
                    EnableTrackbar(window, 1);
                    break;

                case 111:
                    afterclose = 1;
                    PostMessage(window, WM_COMMAND, IDOK, (LPARAM)GetDlgItem(window, IDOK));
                    break;

                case 112:
                    if (MessageBox(window,
                        "The emulator has been set to recalibrate "
                        "itself the next time it is started.\n\n"
                        "Would you like to restart the emulator now?",
                        "Configuration",
                        MB_ICONQUESTION | MB_YESNO) == IDYES) {
                        afterclose = 2;
                        PostMessage(window, WM_COMMAND, IDOK, (LPARAM)GetDlgItem(window, IDOK));
                    }
                    break;

            }
            break;

        case WM_HSCROLL:
            CheckRadioButton(window, 106, 107, 107);
            break;

        case WM_INITDIALOG:
            FillComboBox(window, 101, computerchoices, apple2e);
            FillComboBox(window, 105, videochoices, optmonochrome);
            FillComboBox(window, 102, joystickchoices, joytype);
            FillComboBox(window, 103, soundchoices, soundtype);
            FillComboBox(window, 104, serialchoices, serialport);
            SendDlgItemMessage(window, 108, TBM_SETRANGE, 1, MAKELONG(0, 40));
            SendDlgItemMessage(window, 108, TBM_SETPAGESIZE, 0, 5);
            SendDlgItemMessage(window, 108, TBM_SETTICFREQ, 10, 0);
            SendDlgItemMessage(window, 108, TBM_SETPOS, 1, speed);
            {
                BOOL custom = 1;
                if (speed == 10) {
                    custom = 0;
                    RegLoadValue("Configuration", "Custom Speed", (DWORD *)&custom);
                }
                CheckRadioButton(window, 106, 107, 106 + custom);
                SetFocus(GetDlgItem(window, custom ? 108 : 106));
                EnableTrackbar(window, custom);
            }
            afterclose = 0;
            break;

        case WM_LBUTTONDOWN:
        {
            POINT pt;
            pt.x = LOWORD(lparam);
            pt.y = HIWORD(lparam);
            ClientToScreen(window, &pt);
            RECT rect;
            GetWindowRect(GetDlgItem(window, 108), &rect);
            if ((pt.x >= rect.left) && (pt.x <= rect.right) &&
                (pt.y >= rect.top) && (pt.y <= rect.bottom)) {
                CheckRadioButton(window, 106, 107, 107);
                EnableTrackbar(window, 1);
                SetFocus(GetDlgItem(window, 108));
                ScreenToClient(GetDlgItem(window, 108), &pt);
                PostMessage(GetDlgItem(window, 108), WM_LBUTTONDOWN,
                    wparam, MAKELONG(pt.x, pt.y));
            }
        }
        break;
    }
    return 0;
}

//===========================================================================
static void CreateGdiObjects() {
    ZeroMemory(buttonbitmap, BUTTONS * sizeof(HBITMAP));
    buttonbitmap[BTN_HELP] = LoadButtonBitmap(instance, "HELP_BUTTON");
    buttonbitmap[BTN_RUN] = LoadButtonBitmap(instance, "RUN_BUTTON");
    buttonbitmap[BTN_DRIVE1] = LoadButtonBitmap(instance, "DRIVE1_BUTTON");
    buttonbitmap[BTN_DRIVE2] = LoadButtonBitmap(instance, "DRIVE2_BUTTON");
    buttonbitmap[BTN_TOFILE] = LoadButtonBitmap(instance, "TOFILE_BUTTON");
    buttonbitmap[BTN_TODISK] = LoadButtonBitmap(instance, "TODISK_BUTTON");
    buttonbitmap[BTN_DEBUG] = LoadButtonBitmap(instance, "DEBUG_BUTTON");
    buttonbitmap[BTN_SETUP] = LoadButtonBitmap(instance, "SETUP_BUTTON");
    capsbitmap[0] = LoadButtonBitmap(instance, "CAPSOFF_BITMAP");
    capsbitmap[1] = LoadButtonBitmap(instance, "CAPSON_BITMAP");
    diskbitmap[0] = LoadButtonBitmap(instance, "DISKOFF_BITMAP");
    diskbitmap[1] = LoadButtonBitmap(instance, "DISKREAD_BITMAP");
    diskbitmap[2] = LoadButtonBitmap(instance, "DISKWRITE_BITMAP");
    btnfacebrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    btnfacepen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNFACE));
    btnhighlightpen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
    btnshadowpen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
    smallfont = CreateFont(11, 6, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS,
        "Small Fonts");
}

//===========================================================================
static void DeleteGdiObjects() {
    int loop;
    for (loop = 0; loop < BUTTONS; loop++)
        DeleteObject(buttonbitmap[loop]);
    for (loop = 0; loop < 2; loop++)
        DeleteObject(capsbitmap[loop]);
    for (loop = 0; loop < 3; loop++)
        DeleteObject(diskbitmap[loop]);
    DeleteObject(btnfacebrush);
    DeleteObject(btnfacepen);
    DeleteObject(btnhighlightpen);
    DeleteObject(btnshadowpen);
    DeleteObject(smallfont);
}

//===========================================================================
static void Draw3dRect(HDC dc, int x1, int y1, int x2, int y2, BOOL out) {
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    SelectObject(dc, out ? btnshadowpen : btnhighlightpen);
    POINT pt[3];
    pt[0].x = x1;    pt[0].y = y2 - 1;
    pt[1].x = x2 - 1;  pt[1].y = y2 - 1;
    pt[2].x = x2 - 1;  pt[2].y = y1;
    Polyline(dc, (LPPOINT)& pt, 3);
    SelectObject(dc, (out == 1) ? btnhighlightpen : btnshadowpen);
    pt[1].x = x1;    pt[1].y = y1;
    pt[2].x = x2;    pt[2].y = y1;
    Polyline(dc, (LPPOINT)& pt, 3);
}

//===========================================================================
static void DrawBitmapRect(HDC dc, int x, int y, LPRECT rect, HBITMAP bitmap) {
    HDC    dcmem = CreateCompatibleDC(dc);
    POINT  ptsize, ptorg;
    SelectObject(dcmem, bitmap);
    ptsize.x = rect->right + 1 - rect->left;
    ptsize.y = rect->bottom + 1 - rect->top;
    ptorg.x = rect->left;
    ptorg.y = rect->top;
    BitBlt(dc, x, y, ptsize.x, ptsize.y, dcmem, ptorg.x, ptorg.y, SRCCOPY);
    DeleteDC(dcmem);
}

//===========================================================================
static void DrawButton(HDC passdc, int number) {
    VideoReleaseFrameDC();
    HDC dc = (passdc ? passdc : GetDC(framewindow));
    int x = VIEWPORTCX + (VIEWPORTX << 1);
    int y = number * BUTTONCY + 1;
    SelectObject(dc, GetStockObject(BLACK_PEN));
    MoveToEx(dc, x, y, NULL);
    LineTo(dc, x, y + BUTTONCY - 1);
    LineTo(dc, x + BUTTONCX - 1, y + BUTTONCY - 1);
    if (number == buttondown) {
        int loop = 0;
        while (loop++ < 3)
            Draw3dRect(dc, x + loop, y + loop - 1, x + BUTTONCX, y + BUTTONCY - 1, 0);
        RECT rect = { 0,0,39,39 };
        DrawBitmapRect(dc, x + 4, y + 3, &rect, buttonbitmap[number]);
    }
    else {
        Draw3dRect(dc, x + 1, y, x + BUTTONCX, y + BUTTONCY - 1, 1);
        Draw3dRect(dc, x + 2, y + 1, x + BUTTONCX - 1, y + BUTTONCY - 2, 1);
        RECT rect = { 1,1,40,40 };
        DrawBitmapRect(dc, x + 3, y + 2, &rect, buttonbitmap[number]);
    }
    if ((number == BTN_DRIVE1) || (number == BTN_DRIVE2)) {
        RECT rect;
        int  offset = (number == buttondown) << 1;
        rect.left = x + offset + 3;
        rect.top = y + offset + 30;
        rect.right = x + offset + 42;
        rect.bottom = y + offset + 42;
        SelectObject(dc, smallfont);
        SetTextColor(dc, 0);
        SetTextAlign(dc, TA_CENTER | TA_TOP);
        SetBkMode(dc, TRANSPARENT);
        ExtTextOut(dc, x + offset + 22, rect.top, ETO_CLIPPED, &rect,
            DiskGetName(number - BTN_DRIVE1),
            MIN(8, StrLen(DiskGetName(number - BTN_DRIVE1))),
            NULL);
    }
    if (!passdc)
        ReleaseDC(framewindow, dc);
}

//===========================================================================
static void DrawCrosshairs(int x, int y) {
    static int lastx = 0;
    static int lasty = 0;
    VideoReleaseFrameDC();
    HDC dc = GetDC(framewindow);
#define LINE(x1,y1,x2,y2) MoveToEx(dc,x1,y1,NULL); LineTo(dc,x2,y2);

    // ERASE THE OLD CROSSHAIRS
    if (lastx && lasty) {
        int loop = 5;
        while (loop--) {
            switch (loop) {
                case 0: SelectObject(dc, GetStockObject(BLACK_PEN));  break;
                case 1: // fall through
                case 2: SelectObject(dc, btnshadowpen);               break;
                case 3: // fall through
                case 4: SelectObject(dc, btnfacepen);                 break;
            }
            LINE(lastx - 2, VIEWPORTY - loop - 1, lastx + 3, VIEWPORTY - loop - 1);
            LINE(VIEWPORTX - loop - 1, lasty - 2, VIEWPORTX - loop - 1, lasty + 3);
            if ((loop == 1) || (loop == 2))
                SelectObject(dc, btnhighlightpen);
            LINE(lastx - 2, VIEWPORTY + VIEWPORTCY + loop, lastx + 3, VIEWPORTY + VIEWPORTCY + loop);
            LINE(VIEWPORTX + VIEWPORTCX + loop, lasty - 2, VIEWPORTX + VIEWPORTCX + loop, lasty + 3);
        }
    }

    // DRAW THE NEW CROSSHAIRS
    if (x && y) {
        int loop = 4;
        while (loop--) {
            if ((loop == 1) || (loop == 2))
                SelectObject(dc, GetStockObject(WHITE_PEN));
            else
                SelectObject(dc, GetStockObject(BLACK_PEN));
            LINE(x + loop - 2, VIEWPORTY - 5, x + loop - 2, VIEWPORTY);
            LINE(x + loop - 2, VIEWPORTY + VIEWPORTCY + 4, x + loop - 2, VIEWPORTY + VIEWPORTCY - 1);
            LINE(VIEWPORTX - 5, y + loop - 2, VIEWPORTX, y + loop - 2);
            LINE(VIEWPORTX + VIEWPORTCX + 4, y + loop - 2, VIEWPORTX + VIEWPORTCX - 1, y + loop - 2);
        }
    }

#undef LINE
    lastx = x;
    lasty = y;
    ReleaseDC(framewindow, dc);
}

//===========================================================================
static void DrawFrameWindow(BOOL paint) {
    VideoReleaseFrameDC();
    PAINTSTRUCT ps;
    HDC dc = (paint ? BeginPaint(framewindow, &ps) : GetDC(framewindow));

    // DRAW THE 3D BORDER AROUND THE EMULATED SCREEN
    Draw3dRect(dc,
        VIEWPORTX - 2, VIEWPORTY - 2,
        VIEWPORTX + VIEWPORTCX + 2, VIEWPORTY + VIEWPORTCY + 2,
        0);
    Draw3dRect(dc,
        VIEWPORTX - 3, VIEWPORTY - 3,
        VIEWPORTX + VIEWPORTCX + 3, VIEWPORTY + VIEWPORTCY + 3,
        0);
    SelectObject(dc, btnfacepen);
    Rectangle(dc,
        VIEWPORTX - 4, VIEWPORTY - 4,
        VIEWPORTX + VIEWPORTCX + 4, VIEWPORTY + VIEWPORTCY + 4);
    Rectangle(dc,
        VIEWPORTX - 5, VIEWPORTY - 5,
        VIEWPORTX + VIEWPORTCX + 5, VIEWPORTY + VIEWPORTCY + 5);

    // DRAW THE TOOLBAR BUTTONS
    int loop = BUTTONS;
    while (loop--)
        DrawButton(dc, loop);

    // DRAW THE STATUS AREA
    DrawStatusArea(dc, 1);

    if (paint)
        EndPaint(framewindow, &ps);
    else
        ReleaseDC(framewindow, dc);

    // DRAW THE CONTENTS OF THE EMULATED SCREEN
    if (mode == MODE_LOGO)
        VideoDisplayLogo();
    else if (mode == MODE_DEBUG)
        DebugDisplay(TRUE);
    else
        VideoRedrawScreen();
}

//===========================================================================
static void DrawStatusArea(HDC passdc, BOOL drawbackground) {
    VideoReleaseFrameDC();
    HDC dc = (passdc ? passdc : GetDC(framewindow));
    int x = VIEWPORTCX + (VIEWPORTX << 1);
    int y = 8 * BUTTONCY + 1;

    if (drawbackground) {
        SelectObject(dc, GetStockObject(NULL_PEN));
        SelectObject(dc, btnfacebrush);
        Rectangle(dc, x, y, x + BUTTONCX + 2, y + 35);
        Draw3dRect(dc, x + 1, y + 3, x + BUTTONCX, y + 31, 0);
        SelectObject(dc, smallfont);
        SetTextAlign(dc, TA_CENTER | TA_TOP);
        SetTextColor(dc, 0);
        SetBkMode(dc, TRANSPARENT);
        TextOut(dc, x + 7, y + 7, "1", 1);
        TextOut(dc, x + 25, y + 7, "2", 1);
    }

    {
        RECT rect = { 0,0,8,8 };
        int  drive1 = 0;
        int  drive2 = 0;
        DiskGetLightStatus(&drive1, &drive2);
        DrawBitmapRect(dc, x + 12, y + 8, &rect, diskbitmap[drive1]);
        DrawBitmapRect(dc, x + 30, y + 8, &rect, diskbitmap[drive2]);
    }

    if (apple2e) {
        RECT rect = { 0,0,30,8 };
        BOOL caps = 0;
        KeybGetCapsStatus(&caps);
        DrawBitmapRect(dc, x + 7, y + 19, &rect, capsbitmap[caps != 0]);
    }

    if (!passdc)
        ReleaseDC(framewindow, dc);
}

//===========================================================================
static void EnableTrackbar(HWND window, BOOL enable) {
    EnableWindow(GetDlgItem(window, 108), enable);
    int loop = 120;
    while (loop++ < 124)
        EnableWindow(GetDlgItem(window, loop), enable);
}

//===========================================================================
static void FillComboBox(HWND window, int controlid, const char * choices, int currentchoice) {
    HWND combowindow = GetDlgItem(window, controlid);
    SendMessage(combowindow, CB_RESETCONTENT, 0, 0);
    while (*choices) {
        SendMessage(combowindow, CB_ADDSTRING, 0, (LPARAM)(const char *)choices);
        choices += StrLen(choices) + 1;
    }
    SendMessage(combowindow, CB_SETCURSEL, currentchoice, 0);
}

//===========================================================================
static LRESULT CALLBACK FrameWndProc(
    HWND   window,
    UINT   message,
    WPARAM wparam,
    LPARAM lparam
) {
    switch (message) {

        case WM_ACTIVATE:
            JoyReset();
            SetUsingCursor(0);
            break;

        case WM_CLOSE:
            VideoReleaseFrameDC();
            SetUsingCursor(0);
            KillTimer(window, 1);
            if (helpquit) {
                helpquit = 0;
                char filename[MAX_PATH];
                StrCopy(filename, progdir, ARRSIZE(filename));
                StrCat(filename, "applewin.hlp", ARRSIZE(filename));
                WinHelp(window, filename, HELP_QUIT, 0);
            }
            break;

        case WM_CHAR:
            if (mode == MODE_DEBUG)
                DebugProcessChar((char)wparam);
            break;

        case WM_CREATE:
            framewindow = window;
            CreateGdiObjects();
            PostMessage(window, WM_USER, 0, 0);
            break;

        case WM_DESTROY:
            SetMode(MODE_SHUTDOWN);
            DebugDestroy();
            if (!restart) {
                DiskDestroy();
                ImageDestroy();
            }
            CommDestroy();
            CpuDestroy();
            MemDestroy();
            SpkrDestroy();
            VideoDestroy();
            DeleteGdiObjects();
            PostQuitMessage(0);
            break;

        case WM_KEYDOWN:
            if ((wparam >= VK_F1) && (wparam <= VK_F8) && (buttondown == -1)) {
                SetUsingCursor(0);
                DrawButton((HDC)0, buttondown = (int)(wparam - VK_F1));
            }
            else if (wparam == VK_CAPITAL)
                KeybQueueKeypress((int)wparam, 0);
            else if (wparam == VK_PAUSE) {
                SetUsingCursor(0);
                switch (mode) {
                    case MODE_RUNNING:  SetMode(MODE_PAUSED);            break;
                    case MODE_PAUSED:   SetMode(MODE_RUNNING);           break;
                    case MODE_STEPPING: DebugProcessCommand(VK_ESCAPE);  break;
                }
                if ((mode != MODE_LOGO) && (mode != MODE_DEBUG))
                    VideoRedrawScreen();
                resettiming = 1;
            }
            else if ((wparam == VK_ESCAPE) && usingcursor)
                SetUsingCursor(0);
            else if (mode == MODE_RUNNING || mode == MODE_LOGO || (mode == MODE_STEPPING && wparam != VK_ESCAPE)) {
                BOOL autorep = ((lparam & 0x40000000) != 0);
                BOOL extended = ((lparam & 0x01000000) != 0);
                if (!JoyProcessKey((int)wparam, extended, 1, autorep) && mode != MODE_LOGO)
                    KeybQueueKeypress((int)wparam, extended);
            }
            else if ((mode == MODE_DEBUG) || (mode == MODE_STEPPING))
                DebugProcessCommand((int)wparam);
            if (wparam == VK_F10) {
                SetUsingCursor(0);
                return 0;
            }
            break;

        case WM_KEYUP:
            if ((wparam >= VK_F1) && (wparam <= VK_F8) && (buttondown == (int)wparam - VK_F1)) {
                buttondown = -1;
                DrawButton((HDC)0, (int)(wparam - VK_F1));
                ProcessButtonClick((int)(wparam - VK_F1));
            }
            else
                JoyProcessKey((int)wparam, ((lparam & 0x1000000) != 0), 0, 0);
            break;

        case WM_LBUTTONDOWN:
            if (buttondown == -1) {
                int x = LOWORD(lparam);
                int y = HIWORD(lparam);
                if ((x > VIEWPORTCX + (VIEWPORTX << 1)) && (y < BUTTONS * BUTTONCY)) {
                    DrawButton((HDC)0, buttonactive = buttondown = y / BUTTONCY);
                    SetCapture(window);
                }
                else if (usingcursor)
                    JoySetButton(0, 1);
                else if ((x < VIEWPORTCX + (VIEWPORTX << 1)) && JoyUsingMouse() &&
                    ((mode == MODE_RUNNING) || (mode == MODE_STEPPING)))
                    SetUsingCursor(1);
            }
            break;

        case WM_LBUTTONUP:
            if (buttonactive != -1) {
                ReleaseCapture();
                if (buttondown == buttonactive) {
                    buttondown = -1;
                    DrawButton((HDC)0, buttonactive);
                    ProcessButtonClick(buttonactive);
                }
                buttonactive = -1;
            }
            else if (usingcursor)
                JoySetButton(0, 0);
            break;

        case WM_MOUSEMOVE:
        {
            int x = LOWORD(lparam);
            int y = HIWORD(lparam);
            if (buttonactive != -1) {
                int newdown = (((x > VIEWPORTCX + (VIEWPORTX << 1)) &&
                    (x < VIEWPORTCX + (VIEWPORTX << 1) + BUTTONCX) &&
                    (y >= buttonactive * BUTTONCY) &&
                    (y < (buttonactive + 1) * BUTTONCY)) ? buttonactive : -1);
                if (newdown != buttondown) {
                    buttondown = newdown;
                    DrawButton((HDC)0, buttonactive);
                }
            }
            else if (usingcursor) {
                DrawCrosshairs(x, y);
                JoySetPosition(x - VIEWPORTX, VIEWPORTCX - 4,
                    y - VIEWPORTY, VIEWPORTCY - 4);
            }
        }
        break;

        case WM_PAINT:
            if (GetUpdateRect(window, NULL, 0))
                DrawFrameWindow(1);
            break;

        case WM_QUERYNEWPALETTE:
            return 1;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            if (usingcursor)
                JoySetButton(1, (message == WM_RBUTTONDOWN));
            break;

        case WM_SYSCOLORCHANGE:
            DeleteGdiObjects();
            CreateGdiObjects();
            break;

        case WM_SYSKEYDOWN:
            PostMessage(window, WM_KEYDOWN, wparam, lparam);
            if ((wparam == VK_F10) || (wparam == VK_MENU))
                return 0;
            break;

        case WM_SYSKEYUP:
            PostMessage(window, WM_KEYUP, wparam, lparam);
            break;

        case WM_TIMER:
            if (mode == MODE_PAUSED) {
                static DWORD counter = 0;
                if (counter++ > 1)
                    counter = 0;
                VideoDisplayMode(counter > 0);
            }
            break;

        case WM_USER:
            SpkrInitialize();
            SetTimer(window, 1, 333, NULL);
            if (autoboot) {
                autoboot = 0;
                ProcessButtonClick(BTN_RUN);
            }
            break;

        case WM_USER + 1:
            if (mode != MODE_LOGO)
                if (MessageBox(framewindow,
                    "Running the benchmarks will reset the state of "
                    "the emulated machine, causing you to lose any "
                    "unsaved work.\n\n"
                    "Are you sure you want to do this?",
                    "Benchmarks",
                    MB_ICONQUESTION | MB_YESNO) == IDNO)
                    break;
            UpdateWindow(window);
            ResetMachineState();
            SetMode(MODE_LOGO);
            {
                HCURSOR oldcursor = SetCursor(LoadCursor(0, IDC_WAIT));
                VideoBenchmark();
                ResetMachineState();
                SetCursor(oldcursor);
            }
            break;

        case WM_USER + 2:
            if (mode != MODE_LOGO)
                if (MessageBox(framewindow,
                    "Restarting the emulator will reset the state "
                    "of the emulated machine, causing you to lose any "
                    "unsaved work.\n\n"
                    "Are you sure you want to do this?",
                    "Configuration",
                    MB_ICONQUESTION | MB_YESNO) == IDNO)
                    break;
            restart = 1;
            PostMessage(window, WM_CLOSE, 0, 0);
            break;

    }
    return DefWindowProc(window, message, wparam, lparam);
}

//===========================================================================
static HBITMAP LoadButtonBitmap(HINSTANCE instance, const char * bitmapname) {
    HBITMAP bitmap = LoadBitmap(instance, bitmapname);
    if (!bitmap)
        return bitmap;
    BITMAP info;
    GetObject(bitmap, sizeof(BITMAP), &info);
    if (info.bmBitsPixel >= 8) {
        DWORD  bytespixel = info.bmBitsPixel >> 3;
        DWORD  bytestotal = info.bmHeight * info.bmWidthBytes;
        DWORD  pixelmask = 0xFFFFFFFF >> (32 - info.bmBitsPixel);
        LPBYTE data = new BYTE[bytestotal + 4];
        if (!data)
            return bitmap;
        if (pixelmask == 0xFFFFFFFF)
            pixelmask = 0xFFFFFF;
        if (GetBitmapBits(bitmap, bytestotal, data) == (LONG)bytestotal) {
            DWORD origval = *(LPDWORD)data & pixelmask;
            HWND  window = GetDesktopWindow();
            HDC   dc = GetDC(window);
            HDC   memdc = CreateCompatibleDC(dc);
            SelectObject(memdc, bitmap);
            SetPixelV(memdc, 0, 0, GetSysColor(COLOR_BTNFACE));
            DeleteDC(memdc);
            ReleaseDC(window, dc);
            GetBitmapBits(bitmap, bytestotal, data);
            DWORD newval = *(LPDWORD)data & pixelmask;
            DWORD offset = 0;
            DWORD lineoffset = 0;
            while (offset < bytestotal) {
                if ((*(LPDWORD)(data + offset) & pixelmask) == origval)
                    * (LPDWORD)(data + offset) = (*(LPDWORD)(data + offset) & ~pixelmask)
                    | newval;
                offset += bytespixel;
                lineoffset += bytespixel;
                if (lineoffset + bytespixel > (DWORD)info.bmWidthBytes) {
                    offset += info.bmWidthBytes - lineoffset;
                    lineoffset = 0;
                }
            }
            SetBitmapBits(bitmap, bytestotal, data);
        }
        delete[] data;
    }
    else {
        HWND window = GetDesktopWindow();
        HDC  dc = GetDC(window);
        HDC  memdc = CreateCompatibleDC(dc);
        SelectObject(memdc, bitmap);
        COLORREF origcol = GetPixel(memdc, 0, 0);
        COLORREF newcol = GetSysColor(COLOR_BTNFACE);
        int y = 0;
        do {
            int x = 0;
            do
                if (GetPixel(memdc, x, y) == origcol)
                    SetPixelV(memdc, x, y, newcol);
            while (++x < info.bmWidth);
        } while (++y < info.bmHeight);
        DeleteDC(memdc);
        ReleaseDC(window, dc);
    }
    return bitmap;
}

//===========================================================================
static void ProcessButtonClick(int button) {
    switch (button) {

        case BTN_HELP:
        {
            char filename[MAX_PATH];
            StrCopy(filename, progdir, ARRSIZE(filename));
            StrCat(filename, "applewin.hlp", ARRSIZE(filename));
            WinHelp(framewindow, filename, HELP_CONTENTS, 0);
            helpquit = 1;
        }
        break;

        case BTN_RUN:
            if (mode == MODE_LOGO)
                DiskBoot();
            else if (mode == MODE_RUNNING)
                ResetMachineState();
            if ((mode == MODE_DEBUG) || (mode == MODE_STEPPING))
                DebugEnd();
            SetMode(MODE_RUNNING);
            VideoRedrawScreen();
            resettiming = 1;
            break;

        case BTN_DRIVE1:
        case BTN_DRIVE2:
            DiskSelect(button - BTN_DRIVE1);
            DrawFrameWindow(0);
            break;

        case BTN_TOFILE:
            MessageBeep(0);
            MessageBox(framewindow,
                "The 'Transfer To File' button is not implemented "
                "in this release.",
                title,
                MB_ICONINFORMATION | MB_SETFOREGROUND);
            break;

        case BTN_TODISK:
            MessageBeep(0);
            MessageBox(framewindow,
                "The 'Transfer To Disk Image' button is not implemented "
                "in this release.",
                title,
                MB_ICONINFORMATION | MB_SETFOREGROUND);
            break;

        case BTN_DEBUG:
            if (mode == MODE_LOGO)
                ResetMachineState();
            if (mode == MODE_STEPPING)
                DebugProcessCommand(VK_ESCAPE);
            else if (mode == MODE_DEBUG)
                ProcessButtonClick(BTN_RUN);
            else {
                DebugBegin();
            }
            break;

        case BTN_SETUP:
        {
            InitCommonControls();
            DialogBox(
                instance,
                "CONFIGURATION_DIALOG",
                framewindow,
                (DLGPROC)ConfigDlgProc
            );
        }
        break;

    }
}

//===========================================================================
static void ResetMachineState() {
    MemReset();
    DiskBoot();
    VideoResetState();
    CommReset();
    JoyReset();
}

//===========================================================================
static void SetUsingCursor(BOOL newvalue) {
    if (newvalue == usingcursor)
        return;
    usingcursor = newvalue;
    if (usingcursor) {
        SetCapture(framewindow);
        RECT rect;
        rect.left = VIEWPORTX + 2;
        rect.top = VIEWPORTY + 2;
        rect.right = VIEWPORTX + VIEWPORTCX - 2;
        rect.bottom = VIEWPORTY + VIEWPORTCY - 2;
        ClientToScreen(framewindow, (LPPOINT)& rect.left);
        ClientToScreen(framewindow, (LPPOINT)& rect.right);
        ClipCursor(&rect);
        ShowCursor(0);
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(framewindow, &pt);
        DrawCrosshairs(pt.x, pt.y);
    }
    else {
        DrawCrosshairs(0, 0);
        ShowCursor(1);
        ClipCursor(NULL);
        ReleaseCapture();
    }
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void FrameCreateWindow() {
    int width = VIEWPORTCX + (VIEWPORTX << 1)
        + BUTTONCX
        + (GetSystemMetrics(SM_CXBORDER) << 1)
        + 16;
    int height = VIEWPORTCY + (VIEWPORTY << 1)
        + GetSystemMetrics(SM_CYBORDER)
        + GetSystemMetrics(SM_CYCAPTION)
        + 16;
    framewindow = CreateWindow(
        "APPLE2FRAME",
        apple2e ? title : "Apple ][+ Emulator",
        WS_OVERLAPPED
        | WS_BORDER
        | WS_CAPTION
        | WS_SYSMENU
        | WS_MINIMIZEBOX
        | WS_VISIBLE,
        (GetSystemMetrics(SM_CXSCREEN) - width) >> 1,
        (GetSystemMetrics(SM_CYSCREEN) - height) >> 1,
        width,
        height,
        HWND_DESKTOP,
        (HMENU)0,
        instance,
        NULL
    );
}

//===========================================================================
HDC FrameGetDC() {
    VideoReleaseFrameDC();
#ifdef CLIPVIEWPORT
    RECT rect;
    rect.left = VIEWPORTX + 1;
    rect.top = VIEWPORTY + 1;
    rect.right = VIEWPORTX + VIEWPORTCX;
    rect.bottom = VIEWPORTY + VIEWPORTCY;
    InvalidateRect(framewindow, &rect, 0);
    ClientToScreen(framewindow, (LPPOINT)& rect.left);
    ClientToScreen(framewindow, (LPPOINT)& rect.right);
    clipregion = CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);
    HDC dc = GetDCEx(framewindow, clipregion, DCX_INTERSECTRGN | DCX_VALIDATE);
#else
    HDC dc = GetDC(framewindow);
#endif
    SetViewportOrgEx(dc, VIEWPORTX, VIEWPORTY, NULL);
    return dc;
}

//===========================================================================
void FrameRefreshStatus() {
    DrawStatusArea((HDC)0, 0);
}

//===========================================================================
void FrameRegisterClass() {
    WNDCLASSEX wndclass;
    ZeroMemory(&wndclass, sizeof(WNDCLASSEX));
    wndclass.cbSize        = sizeof(WNDCLASSEX);
    wndclass.style         = CS_OWNDC | CS_BYTEALIGNCLIENT;
    wndclass.lpfnWndProc   = FrameWndProc;
    wndclass.hInstance     = instance;
    wndclass.hIcon         = LoadIcon(instance, "APPLEWIN_ICON");
    wndclass.hCursor       = LoadCursor(0, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndclass.lpszClassName = "APPLE2FRAME";
    wndclass.hIconSm       = (HICON)LoadImageA(instance, "APPLEWIN_ICON", 1, 16, 16, 0);
    RegisterClassExA(&wndclass);
}

//===========================================================================
void FrameReleaseDC(HDC dc) {
    SetViewportOrgEx(dc, 0, 0, NULL);
    ReleaseDC(framewindow, dc);
#ifdef CLIPVIEWPORT
    DeleteObject(clipregion);
#endif
}
