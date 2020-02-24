#include <windows.h>
#include <cstdio>
#include <d3d9.h>
#include <vector>
#include <dbghelp.h>
#include "Hooks.h"
#define IM_VEC2_CLASS_EXTRA
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <complex> 

//#define INSTANT_RESET

extern "C" __declspec(dllexport) void dummyexport() {}

typedef struct {
    float x;
    float y;
    float z;
} vec3f;

typedef struct {
    vec3f pos;
    vec3f size;
} object_t;

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
auto g_game = (game_handler_t*)0x0069BCA0;

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

HANDLE std_out;
bool imgui_initialized = false;

C_Hook CalcKillCollisionHook;
auto CalcKillCollisionOrig = ((signed int(__thiscall*)(void*, vec3f*, vec3f*))0x00426C40);
C_Hook EndSceneHook;
auto EndSceneOrig = ((HRESULT(__stdcall*)(IDirect3DDevice9*))nullptr);
C_Hook CreateWindowHook;
auto CreateWindowOrig = ((HWND(__cdecl*)(HINSTANCE))0x00420C10);
C_Hook StubbedLogHook;
auto StubbedLogOrig = ((void(__cdecl*)(char*,...))0x0041E940);
C_Hook UIContinueHook;
auto UIContinueOrig = ((signed int(__thiscall*)(void*))0x00402870);

auto destroy_game_chains = ((void(__cdecl*)(void))0x0041C269);
auto create_game = ((signed int(__cdecl*)(void))0x0041BA6A);

auto g_lives = (char)0x0069D4BA;
auto g_bombs = (char)0x0069D4BB;

auto window = (HWND)NULL;

WNDPROC wndproc_orig = nullptr;

std::vector<object_t> enemies;

void* get_caller() {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    CONTEXT context;
    STACKFRAME64 stack;

    RtlCaptureContext(&context);
    memset(&stack, 0, sizeof(STACKFRAME64));
    stack.AddrPC.Offset = context.Eip;
    stack.AddrPC.Mode = AddrModeFlat;
    stack.AddrStack.Offset = context.Esp;
    stack.AddrStack.Mode = AddrModeFlat;
    stack.AddrFrame.Offset = context.Ebp;
    stack.AddrFrame.Mode = AddrModeFlat;

    StackWalk64(
        IMAGE_FILE_MACHINE_I386,
        process,
        thread,
        &stack,
        &context,
        NULL,
        SymFunctionTableAccess64,
        SymGetModuleBase64,
        NULL
    );

    return (void*)stack.AddrPC.Offset;
}

signed int __fastcall CalcKillCollisionProc(void* a1, void* dummy, vec3f* a2, vec3f* a3) {
    // enemies are enumerated with a hook because i am lazy
    // this was like this at the start of the project, then i started handling it like how i handle everything else,
    // and that didn't work
    CalcKillCollisionHook.removeHook();
    auto ret = CalcKillCollisionOrig(a1, a2, a3);

    // bullets will call this function if close enough to call calc_graze_collision,
    // so check if this is actually being called from enemy calc
    if (get_caller() != (void*)0x00412753) {
        // lol no goto
        CalcKillCollisionHook.installHook();
        return ret;
    }

    object_t params;
    params.pos.x = a2->x;
    params.pos.y = a2->y;
    params.pos.z = a2->z;
    params.size.x = a3->x;
    params.size.y = a3->y;
    params.size.z = a3->z;
    enemies.push_back(params);

    CalcKillCollisionHook.installHook();
    return ret;
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK wndproc_custom(const HWND hwnd, const UINT u_msg, const WPARAM w_param, const LPARAM l_param) {
    ImGui_ImplWin32_WndProcHandler(hwnd, u_msg, w_param, l_param);
    return CallWindowProc(wndproc_orig, hwnd, u_msg, w_param, l_param);
}

void z_rotation(ImVec2& point, ImVec2& origin, float angle) {
    point.x -= origin.x;
    point.y -= origin.y;
    auto tempx = point.x * cosf(angle) - point.y * sinf(angle);
    auto tempy = point.x * sinf(angle) + point.y * cosf(angle);
    point.x = tempx;
    point.y = tempy;
    point.x += origin.x;
    point.y += origin.y;
}

HRESULT __stdcall EndSceneProc(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    EndSceneHook.removeHook();

    if (!imgui_initialized) {
        imgui_initialized = true;
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(window);
        ImGui_ImplDX9_Init(pDevice);
        ImGui::StyleColorsDark();
        wndproc_orig = reinterpret_cast<WNDPROC>(SetWindowLongPtr(window, GWLP_WNDPROC, LONG_PTR(wndproc_custom)));
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // cover viewport for testing
    {
        ImVec2 p1(32, 16);
        ImVec2 p2(416, 16);
        ImVec2 p3(416, 464);
        ImVec2 p4(32, 464);
        ImGui::GetBackgroundDrawList()->AddQuadFilled(p1, p2, p3, p4, 0xFF000000);
    }

    //ImGui::Begin("bullets");
    auto bullet_handler = (bullet_handler_t*)0x005A5FF8;
    size_t bullet_count = 0;
    for (auto bullet : bullet_handler->bullets) {
        if (bullet.type != 0)
            bullet_count++;
    }
    //ImGui::Text("%u", bullet_count);
    for (auto bullet : bullet_handler->bullets) {
        if (bullet.type != 0) {
            auto pos = bullet.pos;
            auto size = bullet.size;
            //ImGui::Text("(%.2f, %.2f) (%.2f, %.2f)", pos.x, pos.y, size.x, size.y);

            // transform to viewport
            pos.x += 32;
            pos.y += 16;
            ImVec2 p1(pos.x - size.x / 2.0, pos.y - size.y / 2.0);
            ImVec2 p2(pos.x + size.x / 2.0, pos.y - size.y / 2.0);
            ImVec2 p3(pos.x + size.x / 2.0, pos.y + size.y / 2.0);
            ImVec2 p4(pos.x - size.x / 2.0, pos.y + size.y / 2.0);
            ImGui::GetBackgroundDrawList()->AddQuadFilled(p1, p2, p3, p4, 0xFF00FF00);
        }
    }
    //ImGui::End();

    // draw lasers
    //ImGui::Begin("lasers");
    for (auto laser : bullet_handler->lasers) {
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
            p1.x += 32;
            p2.x += 32;
            p1.y += 16;
            p2.y += 16;
            ImGui::GetBackgroundDrawList()->AddLine(p1, p2, 0xFF00FF00, size.y);
        }
    }
    //ImGui::End();

    //ImGui::Begin("enemies");
    //ImGui::Text("%u", enemies.size());
    for (auto enemy : enemies) {
        auto pos = enemy.pos;
        auto size = enemy.size;
        //ImGui::Text("(%.2f, %.2f) (%.2f, %.2f)", pos.x, pos.y, size.x, size.y);

        // transform to viewport
        pos.x += 32;
        pos.y += 16;
        ImVec2 p1(pos.x - size.x / 2.0, pos.y - size.y / 2.0);
        ImVec2 p2(pos.x + size.x / 2.0, pos.y - size.y / 2.0);
        ImVec2 p3(pos.x + size.x / 2.0, pos.y + size.y / 2.0);
        ImVec2 p4(pos.x - size.x / 2.0, pos.y + size.y / 2.0);
        ImGui::GetBackgroundDrawList()->AddQuadFilled(p1, p2, p3, p4, 0xFF0000FF);
    }
    enemies.clear();
    //ImGui::End();

    //ImGui::Begin("misc");
    auto player_handler = (player_handler_t*)0x006CA628;
    //ImGui::Text("player: (%.2f, %.2f)", player_handler->field_458.x, player_handler->field_458.y);
    {
        auto pos = player_handler->field_458;
        vec3f size{5, 5}; // i haven't verified this myself, but touhouwiki says it's 5x5

        // transform to viewport
        pos.x += 32;
        pos.y += 16;
        ImVec2 p1(pos.x - size.x / 2.0, pos.y - size.y / 2.0);
        ImVec2 p2(pos.x + size.x / 2.0, pos.y - size.y / 2.0);
        ImVec2 p3(pos.x + size.x / 2.0, pos.y + size.y / 2.0);
        ImVec2 p4(pos.x - size.x / 2.0, pos.y + size.y / 2.0);
        ImGui::GetBackgroundDrawList()->AddQuadFilled(p1, p2, p3, p4, 0xFFFF0000);
    }
    //ImGui::End();

    //ImGui::Begin("items");
    auto item_handler = (item_handler_t*)0x0069E268;
    size_t items = 0;
    for (auto item : item_handler->item) {
        if (item.active)
            items++;
    }
    //ImGui::Text("%u", items);
    for (auto item : item_handler->item) {
        if (item.active) {
            auto pos = item.pos;
            //ImGui::Text("(%.2f, %.2f)", pos.x, pos.y);

            // transform to viewport
            pos.x += 32;
            pos.y += 16;
            ImVec2 p1(pos.x - 8.0, pos.y - 8.0);
            ImVec2 p2(pos.x + 8.0, pos.y - 8.0);
            ImVec2 p3(pos.x + 8.0, pos.y + 8.0);
            ImVec2 p4(pos.x - 8.0, pos.y + 8.0);
            ImGui::GetBackgroundDrawList()->AddQuadFilled(p1, p2, p3, p4, 0xFF00FFFF);
        }
    }
    //ImGui::End();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    // we don't want the bot to learn to rely on dying, so we'll keep resetting lives to zero
#ifdef INSTANT_RESET
    g_game->lives = 0;
#endif

    HRESULT ret = EndSceneOrig(pDevice);
    EndSceneHook.installHook();
    return ret;
}

HWND __cdecl CreateWindowProc(HINSTANCE hInst) {
    CreateWindowHook.removeHook();
    HWND ret = CreateWindowOrig(hInst);
    window = *(HWND*)0x006C6BD4;
    if (EndSceneOrig == nullptr) {
        printf("hwnd %p\n", window);
        IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
        IDirect3DDevice9* dev = nullptr;
        D3DPRESENT_PARAMETERS d3dpp{ 0 };
        d3dpp.Windowed = true;
        d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        HRESULT ret = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &dev);
        if (FAILED(ret)) {
            printf("%p %x\n", window, ret);
            d3d->Release();
            return window;
        }
        void** vtbl = *reinterpret_cast<void***>(dev);
        EndSceneOrig = ((HRESULT(__stdcall*)(IDirect3DDevice9*))vtbl[42]);
        dev->Release();
        d3d->Release();
        EndSceneHook.setSubs((void*)EndSceneOrig, (void*)EndSceneProc);
        EndSceneHook.installHook();
    }
    CreateWindowHook.installHook();
    return ret;
}

void StubbedLogProc(char* a1, ...) {
    // not even going to bother with unhooking and rehooking
    // original function does nothing
    va_list list;
    va_start(list, a1);
    vprintf(a1, list);
    va_end(list);
}

signed int UIContinueProc(void* a1) {
    g_game->stage--; // game increments stage, but we want to stay on the same one

    destroy_game_chains();
    create_game();
    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        // console
        AllocConsole();
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        SetConsoleTitle(L"th6hook");
        printf("Hello World!\n");
        std_out = GetStdHandle(STD_OUTPUT_HANDLE);

        // patch functions
        CalcKillCollisionHook.setSubs((void*)CalcKillCollisionOrig, (void*)CalcKillCollisionProc);
        CalcKillCollisionHook.installHook();
        CreateWindowHook.setSubs((void*)CreateWindowOrig, (void*)CreateWindowProc);
        CreateWindowHook.installHook();
        StubbedLogHook.setSubs((void*)StubbedLogOrig, (void*)StubbedLogProc);
        StubbedLogHook.installHook();
#ifdef INSTANT_RESET
        UIContinueHook.setSubs((void*)UIContinueOrig, (void*)UIContinueProc);
        UIContinueHook.installHook();
#endif

        // misc patches
        /*
        BYTE StageIncBytes[] = { 0x90 }; // nop
        QPatch StageIncPatch((void*)0x0041BE3D, StageIncBytes, 1);
        StageIncPatch.patch();
        */
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}