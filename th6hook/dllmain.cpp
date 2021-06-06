#define INSTANT_RESET
//#define HANG_ON_SOCKET_FAIL
//#define USE_CONSOLE
#define CUSTOM_CALC_CHAIN
//#define STANDALONE

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <d3d9.h>
#include <vector>
#include <timeapi.h>
#include "Hooks.h"
#include "imgui.h" // still required because some stuff uses ImVec2
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "common_structs.h"
#include "helper.h"
#include "touhou6.h"
#include "renderer.h"

extern "C" __declspec(dllexport) void dummyexport() {}

HANDLE std_out;

C_Hook hook_calc_kill_collision;
C_Hook hook_create_window;
C_Hook hook_stubbed_log;
C_Hook hook_ui_continue;
C_Hook hook_create_game;
C_Hook hook_get_input;
C_Hook hook_check_if_already_running;
C_Hook hook_game_malloc;

bool custom_calc_chain_added = false;
Renderer* renderer = nullptr;
SOCKET sock = INVALID_SOCKET;
bool game_started = false; // going in-game
bool initialized = false; // game startup
bool game_done = false; // true on death
int last_score = 0;
__int16 input_to_send = 0;

int port = 0;
int target_stage = 0;

unsigned frame = 0;

int __cdecl custom_chain_calc(void* a1) {
    // only process every other frame
    if (frame % 2)
        return 1;

    // render code
    for (auto bullet : g_bullet_handler->bullets) {
        if (bullet.type != 0) {
            auto pos = bullet.pos;
            auto size = bullet.size;

            renderer->DrawRect(0, { pos.x, pos.y }, { size.x * 2, size.y * 2 });
        }
    }

    for (auto laser : g_bullet_handler->lasers) {
        if (laser.active) {
            vec3f pos{ (laser.field_234 - laser.field_230) / 2.0f + laser.field_230 + laser.fire_point.x,
                       laser.fire_point.y,
                       0
            };
            vec3f size{ laser.field_234 - laser.field_230,
                        laser.field_23C / 2.0f,
                        0
            };
            ImVec2 p1(pos.x - (size.x / 2.0f), pos.y);
            ImVec2 p2(pos.x + (size.x / 2.0f), pos.y);
            ImVec2 fire(laser.fire_point.x, laser.fire_point.y);
            z_rotation(p1, fire, laser.angle);
            z_rotation(p2, fire, laser.angle);
            renderer->DrawLine(0, p1, p2, size.y);
        }
    }

    for (auto enemy : g_enemy_handler->enemies) {
        if (enemy.type < 0) {
            auto pos = enemy.pos;
            auto size = enemy.size;

            renderer->DrawRect(1, { pos.x, pos.y }, { size.x, size.y });
        }
    }

    // player "hitbox" (a lot larger than it is internally but easier to handle
    renderer->DrawRect(2, { g_player_handler->field_458.x, g_player_handler->field_458.y }, { 15, 15 });

    for (auto item : g_item_handler->item) {
        if (item.active)
            renderer->DrawRect(3, { item.pos.x, item.pos.y }, { 16, 16 });
    }

    renderer->Present();

    // actual logic
    if (game_started && !game_done) {
        // send observation to the server
        int wsa_res = send(sock, renderer->downscaled, SCALE_WIDTH * SCALE_HEIGHT * 4, 0);
        if (wsa_res == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(wsa_res);
            WSACleanup();
#ifdef HANG_ON_SOCKET_FAIL
            while (1)
                ;
#endif
            exit(1);
        }
        if (g_player_handler->death_state == 2) {
            printf("done\n");
            game_done = true;
        }
        // add one to reward to reward staying alive
        //int score_diff = (g_game->score - last_score) / 10 + 1;
        int score_diff = 1;
        //int score_diff = (g_game->score - last_score) / 100 + 1;
        if (score_diff < 0) {
            printf("wtf??? negative score diff\n");
            while (1)
                ;
        }
        // discourage dying
        // this penalty may seem way too large but the reward gets very high
        if (game_done)
            score_diff -= 1000;
        send(sock, (const char*)&score_diff, sizeof(int), 0);
        last_score = g_game->score;

        send(sock, (const char*)&game_done, sizeof(bool), 0);

        // wait for an input from the server
        char input[2];
        recv(sock, input, 2, 0);
        // encode received input into an __int16
        input_to_send = 0;
        switch (input[0]) {
        case 0:
            // do nothing
            break;
        case 1:
            input_to_send |= InputState::Up;
            break;
        case 2:
            input_to_send |= InputState::Down;
            break;
        case 3:
            input_to_send |= InputState::Left;
            break;
        case 4:
            input_to_send |= InputState::Right;
            break;
        default:
            printf("invalid input");
            break;
        }
        if (input[1] == 1)
            input_to_send |= InputState::Focus;

        // i don't want the bot to learn to rely on dying, so this keeps resetting lives to zero
#ifdef INSTANT_RESET
        g_game->lives = 0;
#endif
    }
    return 1;
}

HWND __cdecl custom_create_window(HINSTANCE hInst) {
    hook_create_window.removeHook();
    HWND ret = create_window(hInst);
    // first-time init stuff...
    if (!initialized) {
#ifdef CUSTOM_CALC_CHAIN
        // for anything that we want to run every in-game frame
        chain_t* chain = new chain_t;
        memset(chain, 0, sizeof(chain_t));
        chain->calc = custom_chain_calc;
        chain->flags |= 2; // i would document what this does but i forgot
        add_calc_chain(g_chain, chain, 1000);
        custom_calc_chain_added = true;
#endif
        
        // sdl2
        renderer = new Renderer;

        initialized = true;
    }
    hook_create_window.installHook();
    return ret;
}

void custom_stubbed_log(char* a1, ...) {
    // not even going to bother with unhooking and rehooking
    // original function does nothing
    va_list list;
    va_start(list, a1);
    //vprintf(a1, list);
    va_end(list);
}

signed int custom_ui_continue(void* a1) {
    // also won't bother with unhook and rehook
    destroy_game_chains();
    memset(g_game, 0, sizeof(game_handler_t));
    create_game();
    last_score = 0;
    game_done = false;
    return 0;
}

signed int custom_create_game() {
    hook_create_game.removeHook();
    if (!game_started) {
        printf("game started, overriding input\n");
        game_started = true;
    }
    // randomize stage if using less than 6 actors
    srand(timeGetTime());
    g_game->stage = rand() % 6;
    //g_game->stage = target_stage;
    g_game->is_marisa = false;
    g_game->weapon_type = true;
    g_game->difficulty = rand() % 4; // randomized from easy to lunatic
    // g_game->field_1C = 1; // dunno what this does, seems to be 0 for title screen demo
    auto ret = create_game();
    hook_create_game.installHook();
    return ret;
}

__int16 custom_get_input() {
    frame++;
    if (game_started) {
        return Shoot | SkipDialogue | input_to_send;
    } else {
        /*
        hook_get_input.removeHook();
        auto ret = get_input();
        hook_get_input.installHook();
        return ret;
        */
        // mash A button as fast as possible
        return frame % 2 == 0 ? Shoot : 0;
    }
}

void* custom_game_malloc(size_t size) {
    hook_game_malloc.removeHook();
    // this is in place because i had to debug a crash with pageheap
    // an off by one out of bounds read in the asset loader
    // so i just did this :)
    // TODO: would it be a good idea to only make this run on certain callers?
    auto ret = game_malloc(size + 1);
    hook_game_malloc.installHook();
    return ret;
}

signed int custom_check_if_already_running() {
    // not going to rehook
    return 0;
}

// why do i even have to make 3 macros for 1???
#define MAKE_HOOK(x) MAKE_HOOK_HIDDEN1(x).setSubs((void*)x, (void*)MAKE_HOOK_HIDDEN2(x)); \
                     MAKE_HOOK_HIDDEN1(x).installHook();
#define MAKE_HOOK_HIDDEN1(x) hook_##x
#define MAKE_HOOK_HIDDEN2(x) custom_##x

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
#ifdef USE_CONSOLE
        // console
        AllocConsole();
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        SetConsoleTitle(L"th6hook");
        printf("Hello World!\n");
        std_out = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
#ifdef STANDALONE
        target_stage = 6;
#else
        // parse command line
        // https://stackoverflow.com/questions/1706551/parse-string-into-argv-argc/13281447#13281447
        int argc;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argc != 3) {
            printf("invalid command line args...\n");
            while (1)
                ;
        }
        // lol no error checking
        swscanf(argv[1], L"%d", &port);
        swscanf(argv[2], L"%d", &target_stage);
#endif

        // patch functions
        MAKE_HOOK(create_window);
        MAKE_HOOK(stubbed_log);
#ifdef INSTANT_RESET
        MAKE_HOOK(ui_continue);
#endif
        MAKE_HOOK(create_game);
#ifndef STANDALONE
        MAKE_HOOK(get_input);
#endif
        MAKE_HOOK(check_if_already_running);
        //MAKE_HOOK(game_malloc);

        BYTE seven_byte_nop[] = {
            0x90,
            0x90,
            0x90,
            0x90,
            0x90,
            0x90,
            0x90,
        };
        BYTE ret_zero[] = {
            0x31, 0xc0, // xor eax, eax
            0xC3 // ret
        };
        BYTE j_to_jmp[] = {
            0xEB
        };
        BYTE resizewin_asm[] = {
            0x68, 0x00, 0x00, 0x8F, 0x10 // push 100A0000
        };
        BYTE config_asm[] = {
            0x31, 0xc0, // xor eax, eax
            0x90, // nop
            0x90, // nop
            0x90, // nop
        };
        // patch the 60fps limiter because it breaks randomly
#ifndef STANDALONE
        QPatch limiter_patch((void*)0x0042098E, seven_byte_nop, 2);
        limiter_patch.patch();
#endif
        // kill replay system
        // would be nice to make resetting its own thing
        // too bad the replay system is really broken with it...
        QPatch replay_patch((void*)0x0042A240, ret_zero, sizeof(ret_zero));
        replay_patch.patch();
        QPatch replay_patch2((void*)0x0041C24C, seven_byte_nop, 5);
        replay_patch2.patch();
        // patch out the title screen demo
        QPatch demo_patch((void*)0x004359B1, j_to_jmp, sizeof(j_to_jmp));
        demo_patch.patch();
        QPatch demo_patch2((void*)0x00435950, j_to_jmp, sizeof(j_to_jmp));
        demo_patch2.patch();
        // patch the game to run when not focused
        QPatch focus_patch((void*)0x004206F0, j_to_jmp, sizeof(j_to_jmp));
        focus_patch.patch();
        // resizable window
        QPatch resizewin_patch((void*)0x00420D09, resizewin_asm, sizeof(resizewin_asm));
        resizewin_patch.patch();
        // patch out the game writing to the config file
        // running a bunch of game at the same time can result in a race condition
        QPatch config_patch((void*)0x00424A6D, config_asm, sizeof(config_asm));
        config_patch.patch();

#ifndef STANDALONE
        // connect to tcp server
        // copy pasted from https://docs.microsoft.com/en-us/windows/win32/winsock/complete-client-code lol
        WSADATA wsa_data;
        struct addrinfo hints;
        struct addrinfo* addrinfo_out;
        int wsa_res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (wsa_res != 0) {
            printf("WSAStartup failed with error: %d\n", wsa_res);
#ifdef HANG_ON_SOCKET_FAIL
            while (1)
                ;
#endif
            exit(1);
        }
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        char port_str[16]; // overkill
        snprintf(port_str, sizeof(port_str), "%d", port);
        wsa_res = getaddrinfo("127.0.0.1", port_str, &hints, &addrinfo_out);
        if (wsa_res != 0) {
            printf("getaddrinfo failed with error: %d\n", wsa_res);
            WSACleanup();
#ifdef HANG_ON_SOCKET_FAIL
            while (1)
                ;
#endif
            exit(1);
        }
        sock = socket(addrinfo_out->ai_family, addrinfo_out->ai_socktype,
            addrinfo_out->ai_protocol);
        if (sock == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
#ifdef HANG_ON_SOCKET_FAIL
            while (1)
                ;
#endif
            exit(1);
        }
        while (true) {
            wsa_res = connect(sock, addrinfo_out->ai_addr, (int)addrinfo_out->ai_addrlen);
            if (wsa_res == SOCKET_ERROR) {
                closesocket(wsa_res);
                printf("connect failed, retrying\n");
            } else {
                break;
            }
        }
#endif
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}