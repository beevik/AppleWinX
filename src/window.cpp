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
static SDL_Window *   window;

static SDL_Rect screenrect { 0, 0, 560, 384 };

//===========================================================================
void WindowDestroy() {
    SDL_FreeSurface(screen);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
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

    if (SDL_CreateWindowAndRenderer(screenrect.w * 2, screenrect.h * 2, 0, &window, &renderer) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        return FALSE;
    }
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
        return FALSE;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(renderer, screenrect.w, screenrect.h);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

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

    uint32_t * pixels = (uint32_t *)screen->pixels;
    for (int32_t i = 0, count = screenrect.w * screenrect.h; i < count; i += 2) {
        pixels[i]   = 0xffff0000;
        pixels[i+1] = 0xff000000;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(texture, nullptr, pixels, screenrect.w * sizeof(uint32_t));

    return TRUE;
}

//===========================================================================
void WindowUpdate() {
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
