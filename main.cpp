#define RUN_MOCK_MODE 0 // 切换为真实运行模式

#include "ConfigManager.h"
#include "MemoryScanner.h"
#include "GameLogic.h"
#include "OverlayRenderer.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>

#pragma comment(lib, "winmm.lib")
#pragma comment(linker, "/subsystem:windows")       

struct FrameTelemetry {
    long long totalFrameTimeUs = 0;
};

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance); UNREFERENCED_PARAMETER(hPrevInstance); UNREFERENCED_PARAMETER(lpCmdLine); UNREFERENCED_PARAMETER(nCmdShow);
    SetProcessDPIAware(); srand(static_cast<unsigned int>(time(nullptr)));

    AllocConsole(); FILE* fpDummy = nullptr; freopen_s(&fpDummy, "CONOUT$", "w", stdout);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    timeBeginPeriod(1);
    ConfigManager::Load("config.ini");

    // 1. 首次尝试初始化扫描器，获取 Citra 进程与窗口句柄
    MemoryScanner::Init();

    // 2. 绑定真实的 Citra 窗口句柄初始化渲染器
    bool initSuccess = OverlayRenderer::Init(MemoryScanner::GetCitraHwnd());

    MSG msg; ZeroMemory(&msg, sizeof(msg));
    static FrameTelemetry telemetry;
    static ULONGLONG lastConsoleTick = 0;

    while (msg.message != WM_QUIT) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        if (PeekMessageA(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessageA(&msg);
            continue;
        }

        if (GetAsyncKeyState(VK_END) & 0x8000) break;

        // 3. 动态检查并维护驱动 Citra 进程连接状态（支持中途打开游戏或重启游戏）
        MemoryScanner::Init();

        // 4. 驱动扫描器状态机（内部包含生命周期验证、分级时钟限制以及异步扫描线程调度）
        MemoryScanner::Update();

        float currentW = static_cast<float>(OverlayRenderer::GetCurrentWidth());
        float currentH = static_cast<float>(OverlayRenderer::GetCurrentHeight());

        // 5. 管道完全打通：直接向逻辑层注入真实的扫描目标矢量
        GameLogic::Update(MemoryScanner::GetActiveMonsters(), currentW, currentH, 2);

        if (initSuccess) {
            // 6. 强绑定：让渲染分层窗口实时追踪最新的 Citra 窗口位置与大小
            OverlayRenderer::UpdatePosition(MemoryScanner::GetCitraHwnd());
            OverlayRenderer::BeginFrame();
            OverlayRenderer::Render(GameLogic::GetDisplayMonsters(), GameLogic::GetDamageTicks());
            OverlayRenderer::EndFrame();
        }

        // 控制台白盒 telemetry 监控
        if (GetTickCount64() - lastConsoleTick > 200) {
            COORD c = { 0, 0 }; SetConsoleCursorPosition(hConsole, c);
            auto scanDiag = MemoryScanner::GetDiagnostics();
            auto logicDiag = GameLogic::GetDiagnostics();

            std::cout << "==================================================\n";
            std::cout << "      DCOM PRODUCTION PIPELINE TELEMETRY          \n";
            std::cout << "==================================================\n";
            std::cout << "  Citra Process:         "
                << (MemoryScanner::GetCitraProcess() ? "🟢 CONNECTED" : "🔴 NOT FOUND") << "\n";
            std::cout << "  Citra PID:             " << MemoryScanner::GetCitraPID() << "\n";
            std::cout << "  Graphics Pipeline:     💎 DIRECTCOMPOSITION (100%)\n";
            std::cout << "--------------------------------------------------\n";
            std::cout << "  Async Scan State:      " << (scanDiag.isScanningAsync ? "RUNNING" : "IDLE") << "\n";
            std::cout << "  Last Scan Cost:        " << scanDiag.scanCostMs << " ms\n";
            std::cout << "  Tracked Monsters:      " << logicDiag.trackedMapSize << "\n";
            std::cout << "  Active Dmg Ticks:      " << logicDiag.activeTicksCount << "\n";
            std::cout << "  Logic FSM Code:        " << logicDiag.lastTriggerReason << "\n";
            std::cout << "--------------------------------------------------\n";
            std::cout << "  Single Frame Loop:     " << std::setw(6) << telemetry.totalFrameTimeUs << " us\n";
            std::cout << "==================================================\n";
            lastConsoleTick = GetTickCount64();
        }

        auto frameEnd = std::chrono::high_resolution_clock::now();
        telemetry.totalFrameTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart).count();

        if (telemetry.totalFrameTimeUs < 16666) {
            DWORD sleepMs = static_cast<DWORD>((16666 - telemetry.totalFrameTimeUs) / 1000);
            if (sleepMs > 0) Sleep(sleepMs);
        }
    }

    OverlayRenderer::Cleanup();
    timeEndPeriod(1);
    return 0;
}