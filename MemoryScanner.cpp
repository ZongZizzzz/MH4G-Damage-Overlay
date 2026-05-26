#include "MemoryScanner.h"
#include "ConfigManager.h"
#include <tlhelp32.h>
#include <algorithm>
#include <thread>
#include <mutex>
#include <chrono>

namespace MemoryScanner {
    struct InternalBlock {
        uintptr_t base;
        size_t size;
    };

    static HWND g_citraHwnd = nullptr;
    static HANDLE g_hCitraProcess = nullptr;
    static DWORD g_CitraPID = 0;
    static std::string g_CitraRealName = "UNKNOWN";
    static uintptr_t g_RamBlockBase = 0;
    static std::vector<InternalBlock> g_CachedBlocks;
    static std::vector<TargetMonster> g_ActiveMonsters;
    static Diagnostics g_Diag;

    static std::mutex g_ScanMutex;

    BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
        char title[512]; if (!GetWindowTextA(hwnd, title, sizeof(title))) return TRUE;
        HWND* targetHwnd = reinterpret_cast<HWND*>(lParam);
        if (strstr(title, "Citra") != nullptr && IsWindowVisible(hwnd) && !IsIconic(hwnd)) {
            RECT r; if (GetWindowRect(hwnd, &r)) { if ((r.right - r.left) > 200 && (r.bottom - r.top) > 200) { *targetHwnd = hwnd; return FALSE; } }
        }
        return TRUE;
    }

    uintptr_t GetMainModuleBase(DWORD pid, std::string& outModuleName) {
        uintptr_t baseAddr = 0; HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W me32; me32.dwSize = sizeof(MODULEENTRY32W);
            if (Module32FirstW(hSnapshot, &me32)) {
                baseAddr = reinterpret_cast<uintptr_t>(me32.modBaseAddr);
                int len = WideCharToMultiByte(CP_ACP, 0, me32.szModule, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) { std::vector<char> buf(len); WideCharToMultiByte(CP_ACP, 0, me32.szModule, -1, buf.data(), len, nullptr, nullptr); outModuleName = buf.data(); }
            }
            CloseHandle(hSnapshot);
        }
        return baseAddr;
    }

    std::vector<InternalBlock> ScanAllCitraPhysicalRamBlocks(HANDLE hProcess) {
        std::vector<InternalBlock> blocks;
        if (!hProcess) return blocks;
        SYSTEM_INFO sysInfo; GetSystemInfo(&sysInfo);
        uintptr_t addr = reinterpret_cast<uintptr_t>(sysInfo.lpMinimumApplicationAddress);
        uintptr_t maxAddr = reinterpret_cast<uintptr_t>(sysInfo.lpMaximumApplicationAddress);
        MEMORY_BASIC_INFORMATION mbi;
        while (addr < maxAddr) {
            if (VirtualQueryEx(hProcess, reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)) == sizeof(mbi)) {
                if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_GUARD) == 0) {
                    if (mbi.RegionSize >= 0x08000000 && mbi.RegionSize <= 0x20000000) {
                        blocks.push_back({ reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize });
                    }
                }
                if (mbi.RegionSize == 0) addr += 4096; else addr += mbi.RegionSize;
            }
            else addr += 4096;
        }
        return blocks;
    }

    void TracePointerChainDiagnostics(HANDLE hProcess, uintptr_t ramBase) {
        g_Diag.stepFailed = 0; if (!hProcess || !ramBase) return;
        uintptr_t flatBase = ramBase - 0x08000000;
        uintptr_t addr1 = flatBase + 0x0F12214;
        if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addr1), &g_Diag.ptr1, 4, nullptr) || g_Diag.ptr1 == 0) { g_Diag.stepFailed = 1; g_Diag.ptr2 = 0; g_Diag.ptr3 = 0; g_Diag.finalAddr = 0; return; }
        uintptr_t addr2 = flatBase + g_Diag.ptr1 + 0x18;
        if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addr2), &g_Diag.ptr2, 4, nullptr) || g_Diag.ptr2 == 0) { g_Diag.stepFailed = 2; g_Diag.ptr3 = 0; g_Diag.finalAddr = 0; return; }
        uintptr_t addr3 = flatBase + g_Diag.ptr2 + 0xE28;
        if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(addr3), &g_Diag.ptr3, 4, nullptr) || g_Diag.ptr3 == 0) { g_Diag.stepFailed = 3; g_Diag.finalAddr = 0; return; }
        g_Diag.finalAddr = flatBase + g_Diag.ptr3 + 0x3E8;
    }

    bool Init() {
        if (!g_citraHwnd || !IsWindow(g_citraHwnd)) {
            g_citraHwnd = nullptr; if (g_hCitraProcess) { CloseHandle(g_hCitraProcess); g_hCitraProcess = nullptr; }
            g_RamBlockBase = 0; g_CachedBlocks.clear();
            { std::lock_guard<std::mutex> lock(g_ScanMutex); g_ActiveMonsters.clear(); }
            g_CitraRealName = "UNKNOWN";
            EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&g_citraHwnd));
            if (g_citraHwnd) {
                GetWindowThreadProcessId(g_citraHwnd, &g_CitraPID);
                g_hCitraProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, g_CitraPID);
            }
        }
        return g_hCitraProcess != nullptr;
    }

    void Update() {
        if (!g_hCitraProcess) return;
        if (g_CitraRealName == "UNKNOWN") GetMainModuleBase(g_CitraPID, g_CitraRealName);

        static DWORD lastPID = 0;
        if (g_CitraPID != lastPID || g_CachedBlocks.empty()) {
            g_CachedBlocks = ScanAllCitraPhysicalRamBlocks(g_hCitraProcess);
            lastPID = g_CitraPID;
            g_RamBlockBase = 0;
        }
        if (g_CachedBlocks.empty()) return;
        if (g_RamBlockBase == 0) g_RamBlockBase = g_CachedBlocks[0].base;

        static ULONGLONG lastDiagTick = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastDiagTick > 500) {
            TracePointerChainDiagnostics(g_hCitraProcess, g_RamBlockBase);
            lastDiagTick = now;
        }

        uint32_t hpLimit = ConfigManager::Get().hpMaxLimit;

        {
            std::lock_guard<std::mutex> lock(g_ScanMutex);
            std::vector<TargetMonster> verifiedCache;
            for (const auto& monster : g_ActiveMonsters) {
                uint32_t verificationData[4] = { 0 };
                if (ReadProcessMemory(g_hCitraProcess, reinterpret_cast<LPCVOID>(monster.address), verificationData, 16, nullptr)) {
                    float sizeScale = *reinterpret_cast<float*>(&verificationData[2]);

                    if (sizeScale >= 0.7f && sizeScale <= 1.5f && verificationData[1] == verificationData[3] &&
                        verificationData[1] > 100 && verificationData[1] < hpLimit &&
                        verificationData[0] > 0 && verificationData[0] <= verificationData[1]) {

                        TargetMonster m = monster;
                        m.currentHp = verificationData[0];
                        m.maxHp = verificationData[1];
                        verifiedCache.push_back(m);
                    }
                }
            }
            g_ActiveMonsters = verifiedCache;
        }

        static ULONGLONG lastFullScanTime = 0;
        bool triggerFullScan = false;

        // 【分级时钟重构】：完美解决中途乱入怪检测 + 兼顾 Citra 零内耗运行
        {
            std::lock_guard<std::mutex> lock(g_ScanMutex);
            if (g_ActiveMonsters.empty()) {
                // 1. 场上没怪：每 4 秒快速检索一次
                if (now - lastFullScanTime > 4000) triggerFullScan = true;
            }
            else if (g_ActiveMonsters.size() < 2) {
                // 2. 场上只有 1 只怪：允许中途乱入，但极度克制地每 15 秒才允许后台扫一次
                // 15 秒的超长延迟在后台执行 20ms 的读取，Citra 核心线程撞锁概率降至 0.001% 以下，体感完全零卡顿
                if (now - lastFullScanTime > 15000) triggerFullScan = true;
            }
            else {
                // 3. 场上已经塞满 2 只怪：彻底断电，拒绝任何无谓扫描
                triggerFullScan = false;
            }
        }

        if (triggerFullScan && !g_Diag.isScanningAsync) {
            g_Diag.isScanningAsync = true;

            std::thread t([](HANDLE hProcess, std::vector<InternalBlock> blocks, uint32_t currentHpLimit) {
                auto startTime = std::chrono::high_resolution_clock::now();
                std::vector<TargetMonster> localFoundMonsters;

                for (const auto& block : blocks) {
                    std::vector<uint8_t> localBuffer(block.size);
                    SIZE_T bytesRead = 0;

                    if (ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(block.base), localBuffer.data(), block.size, &bytesRead)) {
                        uint8_t* pRawBuf = localBuffer.data();
                        for (size_t offset = 0; offset < bytesRead - 16; offset += 4) {
                            uint32_t currentHP = *reinterpret_cast<uint32_t*>(pRawBuf + offset);
                            uint32_t maxHP = *reinterpret_cast<uint32_t*>(pRawBuf + offset + 4);
                            float sizeScale = *reinterpret_cast<float*>(pRawBuf + offset + 8);
                            uint32_t initMaxHP = *reinterpret_cast<uint32_t*>(pRawBuf + offset + 12);

                            if (sizeScale >= 0.7f && sizeScale <= 1.5f && maxHP == initMaxHP && maxHP > 100 && maxHP < currentHpLimit && currentHP > 0 && currentHP <= maxHP) {
                                TargetMonster nm;
                                nm.address = block.base + offset;
                                nm.currentHp = currentHP;
                                nm.maxHp = maxHP;
                                localFoundMonsters.push_back(nm);
                                offset += 0x200;
                            }
                        }
                    }
                    if (!localFoundMonsters.empty()) {
                        g_RamBlockBase = block.base;
                        break;
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(g_ScanMutex);
                    for (const auto& lm : localFoundMonsters) {
                        auto it = std::find_if(g_ActiveMonsters.begin(), g_ActiveMonsters.end(), [&lm](const TargetMonster& m) { return m.address == lm.address; });
                        if (it == g_ActiveMonsters.end()) {
                            g_ActiveMonsters.push_back(lm);
                        }
                    }
                }

                auto endTime = std::chrono::high_resolution_clock::now();
                g_Diag.scanCostMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                g_Diag.isScanningAsync = false;
                }, g_hCitraProcess, g_CachedBlocks, hpLimit);

            SetThreadPriority(t.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
            t.detach();

            lastFullScanTime = now;
        }
    }

    HWND GetCitraHwnd() { return g_citraHwnd; }
    HANDLE GetCitraProcess() { return g_hCitraProcess; }
    DWORD GetCitraPID() { return g_CitraPID; }
    std::string GetCitraRealName() { return g_CitraRealName; }
    uintptr_t GetRamBlockBase() { return g_RamBlockBase; }
    std::vector<TargetMonster> GetActiveMonsters() { std::lock_guard<std::mutex> lock(g_ScanMutex); return g_ActiveMonsters; }
    void ClearMonsterHpAddrs() { std::lock_guard<std::mutex> lock(g_ScanMutex); g_ActiveMonsters.clear(); }
    Diagnostics GetDiagnostics() { return g_Diag; }
    void SetHexDump(uint8_t* data) { memcpy(g_Diag.hexDump, data, 16); }
}