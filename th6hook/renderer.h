#pragma once

#include <cstdint>
#include "imgui.h" // ImVec2

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

class Renderer {
public:
    void DrawRect(int layer, ImVec2 pos, ImVec2 size);
    void DrawLine(int layer, ImVec2 p1, ImVec2 p2, float angle, ImVec2 size);
private:
    uint32_t render[384][448][4] = {0};
    uint32_t downscaled[96][112][4] = {0};
};