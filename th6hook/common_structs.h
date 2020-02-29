#pragma once

typedef struct {
    float x;
    float y;
    float z;
} vec3f;

typedef struct {
    vec3f pos;
    vec3f size;
} object_t;