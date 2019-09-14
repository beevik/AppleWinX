/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

static SDL_Renderer * renderer;
static SDL_Texture *  texture;
static SDL_Surface *  screen;
static bool           screendirty;
static SDL_Window *   window;

static SDL_Rect screenrect { 0, 0, 560, 384 };

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
        screenrect.w * 2,
        screenrect.h * 2,
        SDL_WINDOW_RESIZABLE,
        &window,
        &renderer
    );
    if (result != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        WindowDestroy();
        return FALSE;
    }
    SDL_RenderSetLogicalSize(renderer, screenrect.w, screenrect.h);
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "0");
    SDL_SetWindowFullscreen(window, 0);

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        screenrect.w,
        screenrect.h
    );
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture from surface: %s", SDL_GetError());
        WindowDestroy();
        return FALSE;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

    screen = SDL_CreateRGBSurface(
        0,
        screenrect.w,
        screenrect.h,
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

    screendirty = true;
    return TRUE;
}

//===========================================================================
uint32_t * WindowLockPixels() {
    return (uint32_t *)screen->pixels;
}

//===========================================================================
void WindowUnlockPixels() {
    screendirty = true;
}

//===========================================================================
void WindowUpdate() {
    if (screendirty) {
        SDL_UpdateTexture(
            texture,
            &screenrect,
            screen->pixels,
            screenrect.w * sizeof(uint32_t)
        );
        screendirty = false;
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            mode = MODE_SHUTDOWN;
            return;
        }
        else if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_1:
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        SDL_SetWindowFullscreen(window, 0);
                        SDL_SetWindowSize(window, screenrect.w, screenrect.h);
                    }
                    break;

                case SDL_SCANCODE_2:
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        SDL_SetWindowFullscreen(window, 0);
                        SDL_SetWindowSize(window, 2 * screenrect.w, 2 * screenrect.h);
                    }
                    break;

                case SDL_SCANCODE_3:
                    if (event.key.keysym.mod & KMOD_CTRL) {
                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                    break;

                case SDL_SCANCODE_ESCAPE:
                    mode = MODE_SHUTDOWN;
                    return;
            }
        }
    }
}
