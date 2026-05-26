#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <cstdint>
#include "MemoryScanner.h"
#include "imgui/imgui.h"

struct DamageTick {
    int damage = 0;
    ImVec2 pos = ImVec2(0.0f, 0.0f);
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    int lifetime = 0;
    int maxLifetime = 0;
};

namespace GameLogic {
    struct DisplayMonster {
        uint16_t current = 0;
        uint16_t max = 0;
    };

    struct MockInput {
        uint16_t hp = 0;
        uint16_t max = 0;
    };

    // 新增：伤害逻辑层专属白盒诊断结构体
    struct Diagnostics {
        size_t trackedMapSize = 0;      // 逻辑层历史字典中常驻的怪兽数量
        size_t activeTicksCount = 0;     // 当前正在屏幕上做淡出动画的伤害数字数量
        char lastTriggerReason[64] = ""; // 上一次伤害触发/拒绝的底层状态机代码
    };

    void Update(const std::vector<MemoryScanner::TargetMonster>& activeMonsters, float windowW, float windowH, int layoutMode);
    void MockUpdate(const std::vector<MockInput>& mockMonsters, float windowW, float windowH, int layoutMode);

    const std::vector<DisplayMonster>& GetDisplayMonsters();
    std::vector<DamageTick>& GetDamageTicks();
    std::string GetDmgTriggerReason();
    Diagnostics GetDiagnostics(); // 新增：获取诊断数据接口
}