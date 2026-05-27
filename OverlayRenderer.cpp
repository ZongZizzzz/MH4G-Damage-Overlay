#include "OverlayRenderer.h"
#include "ConfigManager.h"
#include "MemoryScanner.h"
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <string>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace OverlayRenderer {
    static HWND g_overlayHwnd = nullptr;
    static ID3D11Device* g_pd3dDevice = nullptr;
    static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    static IDXGISwapChain1* g_pSwapChain = nullptr;
    static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

    static IDCompositionDevice* g_pDComDevice = nullptr;
    static IDCompositionTarget* g_pDComTarget = nullptr;
    static IDCompositionVisual* g_pDComVisual = nullptr;

    static ImFont* g_DamageFont = nullptr;
    static int g_currentWidth = 800;
    static int g_currentHeight = 600;

    static int g_Diag_TopmostLock = 0;
    static int g_Diag_Dx11Valid = 0;
    static int g_Diag_ImGuiFrame = 0;
    static DWORD g_Diag_ActualExStyle = 0;
    static HRESULT g_Diag_DwmHResult = 0;

    LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
        if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    HRESULT CreateCompositionSwapChain(HWND hWnd, int w, int h) {
        IDXGIDevice* pDxgiDevice = nullptr; g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDxgiDevice);
        IDXGIAdapter* pDxgiAdapter = nullptr; pDxgiDevice->GetAdapter(&pDxgiAdapter);
        IDXGIFactory2* pDxgiFactory = nullptr; pDxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&pDxgiFactory);

        DXGI_SWAP_CHAIN_DESC1 sd1; ZeroMemory(&sd1, sizeof(sd1));
        sd1.Width = w; sd1.Height = h;
        sd1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd1.SampleDesc.Count = 1; sd1.SampleDesc.Quality = 0;
        sd1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd1.BufferCount = 2;
        sd1.Scaling = DXGI_SCALING_STRETCH; sd1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd1.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

        HRESULT hr = pDxgiFactory->CreateSwapChainForComposition(g_pd3dDevice, &sd1, nullptr, &g_pSwapChain);

        pDxgiFactory->Release(); pDxgiAdapter->Release(); pDxgiDevice->Release();
        return hr;
    }

    HRESULT ConnectCompositionVisualTree() {
        if (!g_pSwapChain) return E_POINTER;

        DCompositionCreateDevice(nullptr, __uuidof(IDCompositionDevice), (void**)&g_pDComDevice);
        if (!g_pDComDevice) return E_POINTER;

        g_pDComDevice->CreateTargetForHwnd(g_overlayHwnd, TRUE, &g_pDComTarget);
        g_pDComDevice->CreateVisual(&g_pDComVisual);

        g_pDComVisual->SetContent(g_pSwapChain);
        g_pDComTarget->SetRoot(g_pDComVisual);
        g_pDComDevice->Commit();
        return S_OK;
    }

    bool Init(HWND targetHwnd) {
        int sw = 800, sh = 600;
        if (targetHwnd && IsWindow(targetHwnd)) { RECT r; if (GetWindowRect(targetHwnd, &r)) { sw = r.right - r.left; sh = r.bottom - r.top; } }
        g_currentWidth = sw; g_currentHeight = sh;

        HINSTANCE hInst = GetModuleHandle(nullptr);
        DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
        WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, WndProc, 0L, 0L, hInst, nullptr, nullptr, nullptr, nullptr, "OverlayClass", nullptr };
        RegisterClassExA(&wc);

        g_overlayHwnd = CreateWindowExA(exStyle, wc.lpszClassName, "MH4G Damage Overlay", WS_POPUP, 0, 0, sw, sh, nullptr, nullptr, hInst, nullptr);
        g_Diag_ActualExStyle = GetWindowLongA(g_overlayHwnd, GWL_EXSTYLE);

        D3D_FEATURE_LEVEL featureLevel;
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
        if (!g_pd3dDevice) return false;

        if (FAILED(CreateCompositionSwapChain(g_overlayHwnd, sw, sh))) return false;
        if (FAILED(ConnectCompositionVisualTree())) return false;

        if (g_pSwapChain) {
            ID3D11Texture2D* pBB = nullptr; g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBB));
            if (pBB) { g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_mainRenderTargetView); pBB->Release(); }
        }

        ShowWindow(g_overlayHwnd, SW_SHOW); UpdateWindow(g_overlayHwnd);
        IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO();

        g_DamageFont = io.Fonts->AddFontFromFileTTF(ConfigManager::Get().fontPath.c_str(), ConfigManager::Get().fontSize, nullptr);
        if (!g_DamageFont) g_DamageFont = io.Fonts->AddFontDefault();

        ImGui_ImplWin32_Init(g_overlayHwnd); ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
        return true;
    }

    void UpdatePosition(HWND targetHwnd) {
        static ULONGLONG lastCheckTick = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastCheckTick < 16) return;
        lastCheckTick = now;

        static RECT lastRect = { 0, 0, 0, 0 };
        bool positionChanged = false;

        if (targetHwnd && IsWindow(targetHwnd)) {
            RECT rect; if (GetWindowRect(targetHwnd, &rect)) {
                if (rect.left != lastRect.left || rect.top != lastRect.top || rect.right != lastRect.right || rect.bottom != lastRect.bottom) {
                    int width = rect.right - rect.left; int height = rect.bottom - rect.top;
                    MoveWindow(g_overlayHwnd, rect.left, rect.top, width, height, TRUE);
                    positionChanged = true;

                    if (width != g_currentWidth || height != g_currentHeight) {
                        if (g_pd3dDeviceContext) g_pd3dDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
                        if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
                        if (g_pSwapChain) {
                            g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
                            ID3D11Texture2D* pNewBackBuffer = nullptr;
                            if (SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pNewBackBuffer))) && pNewBackBuffer) {
                                g_pd3dDevice->CreateRenderTargetView(pNewBackBuffer, nullptr, &g_mainRenderTargetView); pNewBackBuffer->Release();
                            }
                            g_pDComVisual->SetContent(g_pSwapChain);
                            g_pDComDevice->Commit();
                        }
                        g_currentWidth = width; g_currentHeight = height;
                    }
                    lastRect = rect;
                }
            }
        }

        static ULONGLONG lastTopmostTick = 0;
        if (now - lastTopmostTick > 500 || positionChanged) {
            SetWindowPos(g_overlayHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            g_Diag_TopmostLock = 1;
            lastTopmostTick = now;
        }
        g_Diag_Dx11Valid = (g_pd3dDeviceContext && g_mainRenderTargetView) ? 1 : 0;
    }

    void BeginFrame() { g_Diag_ImGuiFrame = 0; ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame(); g_Diag_ImGuiFrame = 1; }


    ImU32 OverlayColorToU32(const OverlayColor& color, float alphaScale = 1.0f) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a * alphaScale));
    }

    void AddDamageText(ImDrawList* dl, const ImVec2& pos, const char* text, float fontSize, float alpha) {
        const auto& cfg = ConfigManager::Get();

        if (cfg.damageShadowEnabled) {
            const ImU32 shadowColor = OverlayColorToU32(cfg.damageShadowColor, alpha);
            if (cfg.damageShadowOffsetX != 0 || cfg.damageShadowOffsetY != 0) {
                dl->AddText(g_DamageFont, fontSize, ImVec2(pos.x + static_cast<float>(cfg.damageShadowOffsetX), pos.y + static_cast<float>(cfg.damageShadowOffsetY)), shadowColor, text);
            }

            for (int radius = 1; radius <= cfg.damageShadowThickness; ++radius) {
                const float r = static_cast<float>(radius);
                dl->AddText(g_DamageFont, fontSize, ImVec2(pos.x - r, pos.y), shadowColor, text);
                dl->AddText(g_DamageFont, fontSize, ImVec2(pos.x + r, pos.y), shadowColor, text);
                dl->AddText(g_DamageFont, fontSize, ImVec2(pos.x, pos.y - r), shadowColor, text);
                dl->AddText(g_DamageFont, fontSize, ImVec2(pos.x, pos.y + r), shadowColor, text);
                dl->AddText(g_DamageFont, fontSize, ImVec2(pos.x - r, pos.y - r), shadowColor, text);
                dl->AddText(g_DamageFont, fontSize, ImVec2(pos.x - r, pos.y + r), shadowColor, text);
                dl->AddText(g_DamageFont, fontSize, ImVec2(pos.x + r, pos.y - r), shadowColor, text);
                dl->AddText(g_DamageFont, fontSize, ImVec2(pos.x + r, pos.y + r), shadowColor, text);
            }
        }

        dl->AddText(g_DamageFont, fontSize, pos, OverlayColorToU32(cfg.damageColor, alpha), text);
    }

    void Render(const std::vector<GameLogic::DisplayMonster>& monsters, std::vector<DamageTick>& ticks) {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f)); ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Canvas", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
        ImDrawList* dl = ImGui::GetForegroundDrawList();

        const auto& cfg = ConfigManager::Get();
        // ‰÷»æπ÷ŒÔ—™¡ø£® ‹≈‰÷√øÿ÷∆£©
        if (cfg.showMonsterHP) {
            for (size_t i = 0; i < monsters.size(); ++i) {
                if (monsters[i].current > 0 && monsters[i].max > 0) {
                    std::string hpStr = "MONSTER [" + std::to_string(i + 1) + "] HP: " + std::to_string(monsters[i].current) + " / " + std::to_string(monsters[i].max);
                    float yPos = (float)g_currentHeight - 40.0f - (static_cast<float>(i) * 35.0f);
                    dl->AddText(g_DamageFont, 28.0f, ImVec2(25.0f, yPos), ImGui::ColorConvertFloat4ToU32(ImVec4(0.1f, 0.9f, 0.2f, 1.0f)), hpStr.c_str());
                }
            }
        }
        // ‰÷»æ…À∫¶∆Æ◊÷£® ‹≈‰÷√øÿ÷∆£©
        if (cfg.showDamageNumbers) {
            float configSize = ConfigManager::Get().fontSize;
            int configFadeTime = ConfigManager::Get().fadeTime;

            for (const auto& tick : ticks) {
                float alpha = 1.0f;
                if (tick.lifetime < configFadeTime) {
                    alpha = static_cast<float>(tick.lifetime) / static_cast<float>(configFadeTime);
                }
                std::string damageText = std::to_string(tick.damage);
                AddDamageText(dl, tick.pos, damageText.c_str(), configSize, alpha);
            }
        }
        ImGui::End();
    }

    void EndFrame() {
        ImGui::Render();
        if (g_Diag_Dx11Valid) {
            const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            g_pSwapChain->Present(0, 0);
        }
    }

    void Cleanup() {
        ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
        if (g_pDComVisual) g_pDComVisual->Release();
        if (g_pDComTarget) g_pDComTarget->Release();
        if (g_pDComDevice) g_pDComDevice->Release();
        if (g_mainRenderTargetView) g_mainRenderTargetView->Release(); if (g_pSwapChain) g_pSwapChain->Release();
        if (g_pd3dDeviceContext) g_pd3dDeviceContext->Release(); if (g_pd3dDevice) g_pd3dDevice->Release();
    }

    HWND GetOverlayHwnd() { return g_overlayHwnd; }
    int GetCurrentWidth() { return g_currentWidth; }
    int GetCurrentHeight() { return g_currentHeight; }
    int GetDiagTopmostLock() { return g_Diag_TopmostLock; }
    int GetDiagDx11Valid() { return g_Diag_Dx11Valid; }
    int GetDiagImGuiFrame() { return g_Diag_ImGuiFrame; }
    DWORD GetDiagActualExStyle() { return g_Diag_ActualExStyle; }
    HRESULT GetDiagDwmHResult() { return g_Diag_DwmHResult; }
}