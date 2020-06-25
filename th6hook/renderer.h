#pragma once

//#define USE_SDL2

#include <cstdint>
#include "imgui.h" // ImVec2
#include <cmath>
#ifdef USE_SDL2
#include <SDL2/SDL.h>
#endif

/*
    this generates the input image for the agent
    instead of being a 3-channel RGB image, this will have 4 layers
    
    0 - bullets/lasers
    1 - enemies
    2 - player
    3 - items

    this is to ensure the agent won't have to deal with overlaps

    touhou 6 uses a 384x448 viewport, which will be the resolution of the initial render
    then it'll be downscaled bilinearly by 4x (96x112) to use less memory
*/

#define WIDTH 384
#define HEIGHT 448
#define SCALE_WIDTH 96
#define SCALE_HEIGHT 112

class Renderer {
public:
    Renderer();

    void DrawPixel(int layer, int x, int y, char color);
    void DrawRect(int layer, ImVec2 pos, ImVec2 size);
    void DrawLine(int layer, ImVec2 p1, ImVec2 p2, float size);

    void Present();

    char* downscaled = nullptr;
private:
    void BresenhamUpdateContours(ImVec2 p1, ImVec2 p2);
#ifdef USE_SDL2
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    uint32_t* scratch_sdl = nullptr; // 1bpp -> rgba for sdl
#endif

    //uint32_t render[4][448][384] = {0};
    //uint32_t downscaled[4][112][96] = {0};
    char* render = nullptr;
    int* contours = nullptr;
};