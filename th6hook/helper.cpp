#include <math.h>
#include "imgui.h"

void z_rotation(ImVec2& point, ImVec2& origin, float angle) {
    point.x -= origin.x;
    point.y -= origin.y;
    auto tempx = point.x * cosf(angle) - point.y * sinf(angle);
    auto tempy = point.x * sinf(angle) + point.y * cosf(angle);
    point.x = tempx + origin.x;
    point.y = tempy + origin.y;
}