/* gmb_gfx_vulkan_selfcheck.c — backend Vulkan: init + present + benchmark.
 * Confronta il present path Vulkan (MoltenVK) con quello SDL sullo stesso
 * framebuffer sintetico. Richiede un display reale (finestra + Metal layer).
 * Compilato solo quando Vulkan è disponibile (GMB_GFX_VULKAN_FOUND). */
#include "gmb_gfx_vulkan.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define W 640
#define H 400
#define FRAMES 120

#define NFRAMES 8
static uint32_t frames[NFRAMES][W * H];

/* Precalcolo: il benchmark deve misurare il present, non il fill (a -O0 il
 * fill di 256k pixel dominerebbe entrambe le misure). */
static void build_frames(void)
{
    for (int f = 0; f < NFRAMES; f++)
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
            {
                int r = (x + f * 3) & 0xFF;
                int g = (y + f * 2) & 0xFF;
                int b = (x + y + f * 5) & 0xFF;
                frames[f][y * W + x] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
}

int main(void)
{
    setenv("SDL_VIDEODRIVER", "cocoa", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("gmb_gfx_vulkan_selfcheck: SDL_Init fallita: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("gmb_gfx vulkan selfcheck", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_SHOWN);
    if (!win)
    {
        printf("gmb_gfx_vulkan_selfcheck: finestra fallita: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* ── Vulkan ── */
    if (gmb_gfx_vk_start(win, W, H) != 0)
    {
        printf("gmb_gfx_vulkan_selfcheck: start Vulkan fallita\n");
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    build_frames();
    gmb_gfx_vk_present(frames[0], W, H); /* primo frame (crea pipeline lato driver) */
    Uint64 t0 = SDL_GetPerformanceCounter();
    for (int f = 1; f <= FRAMES; f++)
    {
        gmb_gfx_vk_present(frames[f % NFRAMES], W, H);
        SDL_PumpEvents();
    }
    Uint64 t1 = SDL_GetPerformanceCounter();
    double vk_ms = (double)(t1 - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency() / FRAMES;
    printf("VK device=%s frames=%d avg=%.3f ms\n", gmb_gfx_vk_device_name(), FRAMES, vk_ms);
    gmb_gfx_vk_stop();

    /* ── SDL (stesso framebuffer) ── */
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                               SDL_TEXTUREACCESS_STREAMING, W, H) : NULL;
    if (ren && tex)
    {
        SDL_UpdateTexture(tex, NULL, frames[0], W * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        t0 = SDL_GetPerformanceCounter();
        for (int f = 1; f <= FRAMES; f++)
        {
            SDL_UpdateTexture(tex, NULL, frames[f % NFRAMES], W * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
            SDL_PumpEvents();
        }
        t1 = SDL_GetPerformanceCounter();
        double sdl_ms = (double)(t1 - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency() / FRAMES;
        printf("SDL frames=%d avg=%.3f ms\n", FRAMES, sdl_ms);
        printf("gmb_gfx_vulkan_selfcheck: OK (Vulkan %.3f ms vs SDL %.3f ms)\n", vk_ms, sdl_ms);
        SDL_DestroyTexture(tex);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 0;
    }

    printf("gmb_gfx_vulkan_selfcheck: renderer SDL non disponibile\n");
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 1;
}
