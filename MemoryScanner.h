#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace MemoryScanner {
    struct TargetMonster {
        uintptr_t address = 0;
        uint32_t currentHp = 0;
        uint32_t maxHp = 0;
    };

    struct Diagnostics {
        uint32_t ptr1 = 0;
        uint32_t ptr2 = 0;
        uint32_t ptr3 = 0;
        uintptr_t finalAddr = 0;
        int stepFailed = 0;
        uint8_t hexDump[16] = { 0 };
        long long scanCostMs = 0;
        bool isScanningAsync = false;
    };

    bool Init();
    void Update();

    HWND GetCitraHwnd();
    HANDLE GetCitraProcess();
    DWORD GetCitraPID();
    std::string GetCitraRealName();
    uintptr_t GetRamBlockBase();
    std::vector<TargetMonster> GetActiveMonsters();
    void ClearMonsterHpAddrs();
    Diagnostics GetDiagnostics();
    void SetHexDump(uint8_t* data);
}