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
static SDL_Surface * screen;
static SDL_Window *  window;

//===========================================================================
void WindowDestroy() {
    SDL_FreeSurface(screen);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
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

    SDL_Rect rect { 0, 0, 560, 384 };
    if (SDL_CreateWindowAndRenderer(rect.w, rect.h, SDL_WINDOW_RESIZABLE, &window, &renderer) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        return FALSE;
    }

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        rect.w,
        rect.h
    );
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture from surface: %s", SDL_GetError());
        return FALSE;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    SDL_RenderSetLogicalSize(renderer, rect.w, rect.h);

    screen = SDL_CreateRGBSurface(
        0,
        rect.w,
        rect.h,
        32,
        0x00ff0000,
        0x0000ff00,
        0x000000ff,
        0xff000000
    );

    uint32_t * pixels = (uint32_t *)screen->pixels;
    for (int32_t i = 0, count = rect.w * rect.h; i < count; i += 2) {
        pixels[i]   = 0xffff0000;
        pixels[i+1] = 0xff000000;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(texture, nullptr, pixels, rect.w * sizeof(uint32_t));

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
                case SDL_SCANCODE_ESCAPE:
                    mode = MODE_SHUTDOWN;
                    return;
            }
        }
    }
}
