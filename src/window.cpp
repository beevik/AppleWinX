/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

static SDL_Renderer * s_renderer;
static SDL_Texture *  s_texture;
static SDL_Surface *  s_screen;
static bool           s_screenDirty;
static SDL_Window *   s_window;

static SDL_Rect s_screenRect { 0, 0, 560, 384 };

//===========================================================================
static inline bool IsCtrlOnly(uint16_t mod) {
    return (mod & KMOD_CTRL) != 0 && (mod & (KMOD_ALT | KMOD_SHIFT)) == 0;
}

//===========================================================================
static inline bool IsUnmodified(uint16_t mod) {
    return (mod & (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT)) == 0;
}

//===========================================================================
static void ProcessEventKeyDown(const SDL_KeyboardEvent & key)  {
    switch (key.keysym.scancode) {
        case SDL_SCANCODE_CAPSLOCK:
            KeybQueueKeypressSdl(key.keysym);
            return;

        case SDL_SCANCODE_F11:
        case SDL_SCANCODE_PAUSE:
            if (IsUnmodified(key.keysym.mod)) {
                EEmulatorMode mode = EmulatorGetMode();
                switch (mode) {
                    case EMULATOR_MODE_RUNNING:  EmulatorSetMode(EMULATOR_MODE_PAUSED);   break;
                    case EMULATOR_MODE_PAUSED:   EmulatorSetMode(EMULATOR_MODE_RUNNING);  break;
                    case EMULATOR_MODE_STEPPING: DebugProcessCommand(VK_ESCAPE);  break;
                }
                if ((mode != EMULATOR_MODE_LOGO) && (mode != EMULATOR_MODE_DEBUG))
                    VideoRedrawScreen();
                return;
            }
            break;

        case SDL_SCANCODE_HOME:
            TimerUpdateFullSpeedSetting(FULL_SPEED_SETTING_KEYDOWN, true);
            break;

        case SDL_SCANCODE_1:
            if (IsCtrlOnly(key.keysym.mod)) {
                SDL_SetWindowFullscreen(s_window, 0);
                SDL_SetWindowSize(s_window, s_screenRect.w, s_screenRect.h);
                return;
            }
            break;

        case SDL_SCANCODE_2:
            if (IsCtrlOnly(key.keysym.mod)) {
                SDL_SetWindowFullscreen(s_window, 0);
                SDL_SetWindowSize(s_window, 2 * s_screenRect.w, 2 * s_screenRect.h);
                return;
            }
            break;

        case SDL_SCANCODE_3:
            if (IsCtrlOnly(key.keysym.mod)) {
                SDL_SetWindowFullscreen(s_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                return;
            }
            break;
    }

    EEmulatorMode mode = EmulatorGetMode();
    if (mode == EMULATOR_MODE_RUNNING || mode == EMULATOR_MODE_STEPPING) {
        KeybQueueKeypressSdl(key.keysym);
    }
    else if (mode == EMULATOR_MODE_DEBUG) {
        // DebugProcessCommandSdl(key.keysym);
    }
}

//===========================================================================
static void ProcessEventKeyUp(const SDL_KeyboardEvent & key)  {
    switch (key.keysym.scancode) {
        case SDL_SCANCODE_HOME:
            TimerUpdateFullSpeedSetting(FULL_SPEED_SETTING_KEYDOWN, false);
            break;
    }
}

//===========================================================================
void WindowDestroy() {
    if (s_screen)
        SDL_FreeSurface(s_screen);
    if (s_texture)
        SDL_DestroyTexture(s_texture);
    if (s_renderer)
        SDL_DestroyRenderer(s_renderer);
    if (s_window)
        SDL_DestroyWindow(s_window);
    s_renderer = nullptr;
    s_texture  = nullptr;
    s_screen   = nullptr;
    s_window   = nullptr;
}

//===========================================================================
BOOL WindowInitialize() {
#ifdef _DEBUG
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_DEBUG);
#endif

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to initialize SDL: %s", SDL_GetError());
        return FALSE;
    }

    int result = SDL_CreateWindowAndRenderer(
        s_screenRect.w * 2,
        s_screenRect.h * 2,
        SDL_WINDOW_RESIZABLE,
        &s_window,
        &s_renderer
    );
    if (result != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        WindowDestroy();
        return FALSE;
    }
    SDL_RenderSetLogicalSize(s_renderer, s_screenRect.w, s_screenRect.h);
    SDL_RenderSetIntegerScale(s_renderer, SDL_FALSE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");
    SDL_SetWindowFullscreen(s_window, 0);

    s_texture = SDL_CreateTexture(
        s_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        s_screenRect.w,
        s_screenRect.h
    );
    if (!s_texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture from surface: %s", SDL_GetError());
        WindowDestroy();
        return FALSE;
    }
    SDL_SetTextureBlendMode(s_texture, SDL_BLENDMODE_NONE);

    s_screen = SDL_CreateRGBSurface(
        0,
        s_screenRect.w,
        s_screenRect.h,
        32,
        0x00ff0000,
        0x0000ff00,
        0x000000ff,
        0xff000000
    );
    if (!s_screen) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create screen surface: %s", SDL_GetError());
        WindowDestroy();
        return FALSE;
    }

    s_screenDirty = true;
    return TRUE;
}

//===========================================================================
uint32_t * WindowLockPixels() {
    return (uint32_t *)s_screen->pixels;
}

//===========================================================================
void WindowUnlockPixels() {
    s_screenDirty = true;
}

//===========================================================================
void WindowUpdate() {
    if (s_screenDirty) {
        SDL_UpdateTexture(
            s_texture,
            &s_screenRect,
            s_screen->pixels,
            s_screenRect.w * sizeof(uint32_t)
        );

        SDL_SetRenderDrawColor(s_renderer, 0x00, 0x00, 0x00, 0xff);
        SDL_RenderClear(s_renderer);
        SDL_RenderCopy(s_renderer, s_texture, nullptr, nullptr);
        SDL_RenderPresent(s_renderer);
        s_screenDirty = false;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                EmulatorSetMode(EMULATOR_MODE_SHUTDOWN);
                return;
            case SDL_KEYDOWN:
                ProcessEventKeyDown(event.key);
                break;
            case SDL_KEYUP:
                ProcessEventKeyUp(event.key);
                break;
        }
    }
}
