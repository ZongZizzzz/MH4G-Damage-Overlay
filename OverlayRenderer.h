#pragma once
#include <windows.h>
#include <vector>
#include "GameLogic.h"

namespace OverlayRenderer {
    bool Init(HWND targetHwnd);
    void UpdatePosition(HWND targetHwnd);
    void BeginFrame();
    void Render(const std::vector<GameLogic::DisplayMonster>& monsters, std::vector<DamageTick>& ticks);
    void EndFrame();
    void Cleanup();

    HWND GetOverlayHwnd();
    int GetCurrentWidth();
    int GetCurrentHeight();
    int GetDiagTopmostLock();
    int GetDiagDx11Valid();
    int GetDiagImGuiFrame();

    DWORD GetDiagActualExStyle();
    HRESULT GetDiagDwmHResult();
}