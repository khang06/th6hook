#pragma once
// touhou 6 v1.02h

#include "common_structs.h"

/* game-specific structs */
#pragma pack(push, 1)
typedef struct {
    char gap0[0x550];
    vec3f size;
    char gap55C[4];
    vec3f pos;
    char gap56C[0x52];
    short type;
    char gap5C0[4];
} bullet_t;
static_assert(sizeof(bullet_t) == 0x5C4, "bullet_t must be 0x5C4 bytes long");

typedef struct {
    char gap0[0x220];
    vec3f fire_point;
    float angle;
    float field_230;
    float field_234;
    float field_238;
    float field_23C;
    char gap240[0x18];
    int active;
    char gap25C[0x14];
} laser_t;
static_assert(sizeof(laser_t) == 0x270, "laser_t must be 0x270 bytes long");

typedef struct {
    char gap0[0x5600];
    bullet_t bullets[640];
    laser_t lasers[64];
    char gapF5C00[0x18];
} bullet_handler_t;
static_assert(sizeof(bullet_handler_t) == 0xF5C18, "bullet_handler_t must be 0xF5C18 bytes long");

typedef struct {
    char gap0[0x458];
    vec3f field_458;
    vec3f field_464;
    char gap470[0x9480];
} player_handler_t;
static_assert(sizeof(player_handler_t) == 0x98F0, "player_handler_t must be 0x98F0 bytes long");

typedef struct {
    char gap0[0x1819];
    char bombs;
    char lives;
    char gap181B[0x219];
    int stage;
    char gap1A38[0x48];
} game_handler_t;
static_assert(sizeof(game_handler_t) == 0x1A80, "game_handler_t must be 0x1A80 bytes long");

typedef struct {
    char gap0[0x110];
    vec3f pos;
    char gap11C[0x25];
    bool active;
    char gap142[2];
} item_t;
static_assert(sizeof(item_t) == 0x144, "item_t must be 0x144 bytes long");

typedef struct {
    item_t item[512];
    char gap28800[0x14c];
} item_handler_t;
static_assert(sizeof(item_handler_t) == 0x2894C, "item_handler_t must be 0x2894C bytes long");

typedef struct {
    char gap0[0xC6C];
    vec3f pos;
    vec3f size;
    char gapC84[0x1CC];
    char type;
    short flags; // lol misaligned
    char gapE53[0x75];
} enemy_t;
static_assert(sizeof(enemy_t) == 0xEC8, "enemy_t must be 0xEC8 bytes long");

typedef struct {
    char gap0[0xED0];
    enemy_t enemies[256];
    char gapED6D0[0xF1C];
} enemy_handler_t;
static_assert(sizeof(enemy_handler_t) == 0xEE5EC, "enemy_handler_t must be 0xEE5EC bytes long");

typedef struct {
    uint16_t priority;
    uint16_t flags;
    int(__cdecl* calc)(void*);
    int(__cdecl* init)(void*);
    int(__cdecl* destroy)(void*);
    void* back;
    void* next;
    int unknown;
    void* data;
} chain_t;
static_assert(sizeof(chain_t) == 0x20, "chain_t must be 0x20 bytes long");
#pragma pack(pop)

/* game-specific objects */
auto g_game = (game_handler_t*)0x0069BCA0;
auto g_bullet_handler = (bullet_handler_t*)0x005A5FF8;
auto g_player_handler = (player_handler_t*)0x006CA628;
auto g_item_handler = (item_handler_t*)0x0069E268;
auto g_enemy_handler = (enemy_handler_t*)0x004B79C8;
auto g_chain = (chain_t*)0x0069D918;

/* functions to call */
auto destroy_game_chains = ((void(__cdecl*)(void))0x0041C269);
auto create_game = ((signed int(__cdecl*)(void))0x0041BA6A);
auto add_calc_chain = ((void(__thiscall*)(void*, chain_t*, signed int))0x0041C860);

/* functions to hook */
auto calc_kill_collision = ((signed int(__thiscall*)(void*, vec3f*, vec3f*))0x00426C40);
auto create_window = ((HWND(__cdecl*)(HINSTANCE))0x00420C10);
auto stubbed_log = ((void(__cdecl*)(char*, ...))0x0041E940);
auto ui_continue = ((signed int(__thiscall*)(void*))0x00402870);