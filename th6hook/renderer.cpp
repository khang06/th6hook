#include <Windows.h>
#include "renderer.h"

Renderer::Renderer() {
    render = new char[WIDTH * HEIGHT * 4];
    downscaled = new char[SCALE_WIDTH * SCALE_HEIGHT * 4];
    contours = new int[HEIGHT * 2];
#ifdef USE_SDL2
    scratch_sdl = new uint32_t[WIDTH * HEIGHT];

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("%s\n", SDL_GetError());
        MessageBox(NULL, L"fuck!!!!!", L"sdl init failed", 0);
        exit(1);
    }

    window = SDL_CreateWindow("debugging stuff", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 384, 448, SDL_WINDOW_SHOWN); 
    if (window == nullptr) {
        printf("%s\n", SDL_GetError());
        MessageBox(NULL, L"fuck!!!!!", L"sdl window init failed", 0);
        exit(1);
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == nullptr) {
        printf("%s\n", SDL_GetError());
        MessageBox(NULL, L"fuck!!!!!", L"sdl renderer init failed", 0);
        exit(1);
    }
#endif
}

void Renderer::DrawPixel(int layer, int x, int y, char color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
        return;
    render[(layer * WIDTH * HEIGHT) + (y * WIDTH) + x] = color;
}

void Renderer::DrawRect(int layer, ImVec2 pos, ImVec2 size) {
    // this could be done with memset but that didn't go well
    // being off by a few pixels doesn't matter because this will be 4x downscaled anyways
    for (int y = pos.y - (size.y / 2); y <= pos.y + (size.y / 2); y++)
        for (int x = pos.x - (size.x / 2); x <= pos.x + (size.x / 2); x++)
            DrawPixel(layer, x, y, 0xFF);
}

void Renderer::BresenhamUpdateContours(ImVec2 p1, ImVec2 p2) {
    int x0 = p1.x;
    int y0 = p1.y;
    int x1 = p2.x;
    int y1 = p2.y;
    // http://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;

    for (;;) {
        if (y0 >= 0 && y0 < HEIGHT) {
            if (x0 < contours[y0 * 2] || contours[y0 * 2] == -1)
                contours[y0 * 2] = x0;
            if (x0 > contours[y0 * 2 + 1] || contours[y0 * 2 + 1] == -1)
                contours[y0 * 2 + 1] = x0;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 < dy) { err += dx; y0 += sy; }
    }
}

void Renderer::DrawLine(int layer, ImVec2 point1, ImVec2 point2, float size) {
    // generate points
    float perpendicular = atan(-((point2.x - point1.x) / (point2.y - point1.x)));
    
    ImVec2 p1(cosf(perpendicular) * (-size / 2) + point1.x, sinf(perpendicular) * (-size / 2) + point1.y);
    ImVec2 p2(cosf(perpendicular) * (size / 2) + point1.x, sinf(perpendicular) * (size / 2) + point1.y);
    ImVec2 p3(cosf(perpendicular) * (size / 2) + point2.x, sinf(perpendicular) * (size / 2) + point2.y);
    ImVec2 p4(cosf(perpendicular) * (-size / 2) + point2.x, sinf(perpendicular) * (-size / 2) + point2.y);

    // clear contours
    memset(contours, -1, HEIGHT * 2 * 4);

    // get min and max x values of each y line
    BresenhamUpdateContours(p1, p2);
    BresenhamUpdateContours(p2, p3);
    BresenhamUpdateContours(p3, p4);
    BresenhamUpdateContours(p4, p1);

    // fill between contours
    for (int y = 0; y < HEIGHT; y++) {
        if (contours[y * 2] != -1 && contours[y * 2 + 1] != -1) {
            for (int x = contours[y * 2]; x <= contours[y * 2 + 1]; x++)
                DrawPixel(layer, x, y, 0xFF);
        }
    }
}

void Renderer::Present() {
    // downscale
    // http://tech-algorithm.com/articles/bilinear-image-scaling/
    constexpr float x_ratio = 4;
    constexpr float y_ratio = 4;
    for (int layer = 0; layer < 4; layer++) {
        int offset = 0;
        for (int i = 0; i < SCALE_HEIGHT; i++) {
            for (int j = 0; j < SCALE_WIDTH; j++) {
                int x = x_ratio * j;
                int y = y_ratio * i;
                float x_diff = (x_ratio * j) - x;
                float y_diff = (y_ratio * i) - y;
                int index = (layer * SCALE_WIDTH * SCALE_HEIGHT) + y * WIDTH + x;

                char A = render[index];
                char B = render[index + 1];
                char C = render[index + WIDTH];
                char D = render[index + WIDTH + 1];

                downscaled[(layer * SCALE_WIDTH * SCALE_HEIGHT) + offset++] = A * (1 - x_diff) * (1 - y_diff) +
                                                                              B * (x_diff) * (1 - y_diff) +
                                                                              C * (y_diff) * (1 - x_diff) +
                                                                              D * (x_diff * y_diff);
            }
        }
    }
#ifdef USE_SDL2
    //const uint8_t* rust_render = renderer_get_render();
    for (int i = 0; i < SCALE_WIDTH * SCALE_HEIGHT; i++)
        scratch_sdl[i] = downscaled[i] << 24 | downscaled[i] << 16 | downscaled[i] << 8 | 0xFF;

    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(scratch_sdl, SCALE_WIDTH, SCALE_HEIGHT, 32, SCALE_WIDTH * 4, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
    if (surface == nullptr) {
        printf("%s\n", SDL_GetError());
        MessageBox(NULL, L"fuck!!!!!", L"sdl surface init failed", 0);
        exit(1);
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == nullptr) {
        printf("%s\n", SDL_GetError());
        MessageBox(NULL, L"fuck!!!!!", L"sdl texture init failed", 0);
        exit(1);
    }

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);

#endif
    memset(render, 0, WIDTH * HEIGHT * 4);
}