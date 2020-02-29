//#define INSTANT_RESET

#include <windows.h>
#include <cstdio>
#include <d3d9.h>
#include <vector>
#include "Hooks.h"
#include "imgui.h" // still required because some stuff uses ImVec2
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "common_structs.h"
#include "helper.h"
#include "touhou6.h"

extern "C" __declspec(dllexport) void dummyexport() {}

HANDLE std_out;
bool imgui_initialized = false;

C_Hook hook_calc_kill_collision;
C_Hook hook_EndScene;
C_Hook hook_create_window;
C_Hook hook_stubbed_log;
C_Hook hook_ui_continue;

auto orig_EndScene = ((HRESULT(__stdcall*)(IDirect3DDevice9*))nullptr);

auto window = (HWND)NULL;

WNDPROC wndproc_orig = nullptr;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK wndproc_custom(const HWND hwnd, const UINT u_msg, const WPARAM w_param, const LPARAM l_param) {
    ImGui_ImplWin32_WndProcHandler(hwnd, u_msg, w_param, l_param);
    return CallWindowProc(wndproc_orig, hwnd, u_msg, w_param, l_param);
}

HRESULT __stdcall custom_EndScene(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    hook_EndScene.removeHook();

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

    {
        // 384x448
        ImVec2 p1(32, 16);
        ImVec2 p2(416, 16);
        ImVec2 p3(416, 464);
        ImVec2 p4(32, 464);
        ImGui::GetBackgroundDrawList()->AddQuadFilled(p1, p2, p3, p4, 0xFF000000);
    }

    ImGui::Begin("bullets");
    size_t bullet_count = 0;
    for (auto bullet : g_bullet_handler->bullets) {
        if (bullet.type != 0)
            bullet_count++;
    }
    ImGui::Text("%u", bullet_count);
    for (auto bullet : g_bullet_handler->bullets) {
        if (bullet.type != 0) {
            auto pos = bullet.pos;
            auto size = bullet.size;
            ImGui::Text("(%.2f, %.2f) (%.2f, %.2f)", pos.x, pos.y, size.x, size.y);

            // only need to transform to viewport if using imgui
            pos.x += 32;
            pos.y += 16;
            ImVec2 p1(pos.x - size.x / 2.0, pos.y - size.y / 2.0);
            ImVec2 p2(pos.x + size.x / 2.0, pos.y - size.y / 2.0);
            ImVec2 p3(pos.x + size.x / 2.0, pos.y + size.y / 2.0);
            ImVec2 p4(pos.x - size.x / 2.0, pos.y + size.y / 2.0);
            ImGui::GetBackgroundDrawList()->AddQuadFilled(p1, p2, p3, p4, 0xFF00FF00);
        }
    }
    ImGui::End();

    ImGui::Begin("lasers");
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
            p1.x += 32;
            p2.x += 32;
            p1.y += 16;
            p2.y += 16;
            ImGui::GetBackgroundDrawList()->AddLine(p1, p2, 0xFF00FF00, size.y);
        }
    }
    ImGui::End();

    ImGui::Begin("enemies");
    for (auto enemy : g_enemy_handler->enemies) {
        if (enemy.type < 0) {
            // original code seems to check 4, 2, and 1 in separate parts
            if (enemy.flags & 7 == 7 && !(enemy.flags & 0x800)) {
                auto pos = enemy.pos;
                auto size = enemy.size;
                ImGui::Text("(%.2f, %.2f) (%.2f, %.2f)", pos.x, pos.y, size.x, size.y);

                // transform to viewport
                pos.x += 32;
                pos.y += 16;
                ImVec2 p1(pos.x - size.x / 2.0, pos.y - size.y / 2.0);
                ImVec2 p2(pos.x + size.x / 2.0, pos.y - size.y / 2.0);
                ImVec2 p3(pos.x + size.x / 2.0, pos.y + size.y / 2.0);
                ImVec2 p4(pos.x - size.x / 2.0, pos.y + size.y / 2.0);
                ImGui::GetBackgroundDrawList()->AddQuadFilled(p1, p2, p3, p4, 0xFF0000FF);
            }
        }
    }
    ImGui::End();

    ImGui::Begin("misc");
    ImGui::Text("player: (%.2f, %.2f)", g_player_handler->field_458.x, g_player_handler->field_458.y);
    {
        auto pos = g_player_handler->field_458;
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
    ImGui::End();

    ImGui::Begin("items");
    size_t items = 0;
    for (auto item : g_item_handler->item) {
        if (item.active)
            items++;
    }
    ImGui::Text("%u", items);
    for (auto item : g_item_handler->item) {
        if (item.active) {
            auto pos = item.pos;
            ImGui::Text("(%.2f, %.2f)", pos.x, pos.y);

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
    ImGui::End();

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    // i don't want the bot to learn to rely on dying, this keeps resetting lives to zero
#ifdef INSTANT_RESET
    g_game->lives = 0;
#endif

    HRESULT ret = orig_EndScene(pDevice);
    hook_EndScene.installHook();
    return ret;
}

HWND __cdecl custom_create_window(HINSTANCE hInst) {
    hook_create_window.removeHook();
    HWND ret = create_window(hInst);
    window = *(HWND*)0x006C6BD4;
    if (orig_EndScene == nullptr) {
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
        orig_EndScene = ((HRESULT(__stdcall*)(IDirect3DDevice9*))vtbl[42]);
        dev->Release();
        d3d->Release();
        hook_EndScene.setSubs((void*)orig_EndScene, (void*)custom_EndScene);
        hook_EndScene.installHook();
    }
    hook_create_window.installHook();
    return ret;
}

void custom_stubbed_log(char* a1, ...) {
    // not even going to bother with unhooking and rehooking
    // original function does nothing
    va_list list;
    va_start(list, a1);
    vprintf(a1, list);
    va_end(list);
}

signed int custom_ui_continue(void* a1) {
    g_game->stage--; // game increments stage, but we want to stay on the same one

    destroy_game_chains();
    create_game();
    return 0;
}

// why do i even have to do this???
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
        // console
        AllocConsole();
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        SetConsoleTitle(L"th6hook");
        printf("Hello World!\n");
        std_out = GetStdHandle(STD_OUTPUT_HANDLE);

        // patch functions
        MAKE_HOOK(create_window);
        MAKE_HOOK(stubbed_log);
#ifdef INSTANT_RESET
        MAKE_HOOK(ui_continue);
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