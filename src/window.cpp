/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

static SDL_Renderer * renderer;
static SDL_Texture *  texture;
static SDL_Surface *  screen;
static bool           screenDirty;
static SDL_Window *   window;

static SDL_Rect screenRect { 0, 0, 560, 384 };

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
                switch (mode) {
                    case MODE_RUNNING:  SetMode(MODE_PAUSED);            break;
                    case MODE_PAUSED:   SetMode(MODE_RUNNING);           break;
                    case MODE_STEPPING: DebugProcessCommand(VK_ESCAPE);  break;
                }
                if ((mode != MODE_LOGO) && (mode != MODE_DEBUG))
                    VideoRedrawScreen();
                return;
            }
            break;

        case SDL_SCANCODE_1:
            if (IsCtrlOnly(key.keysym.mod)) {
                SDL_SetWindowFullscreen(window, 0);
                SDL_SetWindowSize(window, screenRect.w, screenRect.h);
                return;
            }
            break;

        case SDL_SCANCODE_2:
            if (IsCtrlOnly(key.keysym.mod)) {
                SDL_SetWindowFullscreen(window, 0);
                SDL_SetWindowSize(window, 2 * screenRect.w, 2 * screenRect.h);
                return;
            }
            break;

        case SDL_SCANCODE_3:
            if (IsCtrlOnly(key.keysym.mod)) {
                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                return;
            }
            break;
    }

    if (mode == MODE_RUNNING || mode == MODE_STEPPING) {
        KeybQueueKeypressSdl(key.keysym);
    }
    else if (mode == MODE_DEBUG) {
        // DebugProcessCommandSdl(key.keysym);
    }
}

//===========================================================================
void WindowDestroy() {
    if (screen)
        SDL_FreeSurface(screen);
    if (texture)
        SDL_DestroyTexture(texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    renderer = nullptr;
    texture  = nullptr;
    screen   = nullptr;
    window   = nullptr;
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
        screenRect.w * 2,
        screenRect.h * 2,
        SDL_WINDOW_RESIZABLE,
        &window,
        &renderer
    );
    if (result != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        WindowDestroy();
        return FALSE;
    }
    SDL_RenderSetLogicalSize(renderer, screenRect.w, screenRect.h);
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");
    SDL_SetWindowFullscreen(window, 0);

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        screenRect.w,
        screenRect.h
    );
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture from surface: %s", SDL_GetError());
        WindowDestroy();
        return FALSE;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

    screen = SDL_CreateRGBSurface(
        0,
        screenRect.w,
        screenRect.h,
        32,
        0x00ff0000,
        0x0000ff00,
        0x000000ff,
        0xff000000
    );
    if (!screen) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create screen surface: %s", SDL_GetError());
        WindowDestroy();
        return FALSE;
    }

    screenDirty = true;
    return TRUE;
}

//===========================================================================
uint32_t * WindowLockPixels() {
    return (uint32_t *)screen->pixels;
}

//===========================================================================
void WindowUnlockPixels() {
    screenDirty = true;
}

//===========================================================================
void WindowUpdate() {
    if (screenDirty) {
        SDL_UpdateTexture(
            texture,
            &screenRect,
            screen->pixels,
            screenRect.w * sizeof(uint32_t)
        );

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        screenDirty = false;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                SetMode(MODE_SHUTDOWN);
                return;
            case SDL_KEYDOWN:
                ProcessEventKeyDown(event.key);
                break;
        }
    }
}
