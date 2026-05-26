#include "GameLogic.h"
#include "ConfigManager.h"
#include <cstdlib>
#include <map>
#include <algorithm>
#include <cstring>

namespace GameLogic {
    struct TrackedMonster {
        uint16_t currentHP = 0;
        uint16_t maxHP = 0;
        uint16_t lastHP = 0;
        bool     isFirstLock = true;
    };

    static std::map<uintptr_t, TrackedMonster> g_RealMonsters;
    static std::map<int, TrackedMonster>       g_MockMonsters;
    static std::vector<DisplayMonster>         g_DisplayList;
    static std::vector<DamageTick>             g_DamageTicks;
    static std::string                         g_DmgTriggerReason = "WAIT_DATA_INPUT";
    static Diagnostics                         g_Diag;

    static ULONGLONG g_lastSpawnTime = 0;
    static int       g_overlapCount = 0;
    static float     g_cumulativeXOffset = 0.0f;

    void SpawnDamageNumber(int damage, float windowW, float windowH, int layoutMode) {
        if (damage <= 0) return;
        DamageTick tick; tick.damage = damage;
        const auto& cfg = ConfigManager::Get();

        ULONGLONG now = GetTickCount64();
        if (now - g_lastSpawnTime < 1000) {
            g_overlapCount++;
            if (g_overlapCount >= cfg.overlapMax) {
                g_overlapCount = 0;
                g_cumulativeXOffset = 0.0f;
            }
            else {
                float randDir = static_cast<float>((rand() % 3) - 1);
                g_cumulativeXOffset += (randDir * cfg.xStaggerStep);
            }
        }
        else {
            g_overlapCount = 0;
            g_cumulativeXOffset = 0.0f;
        }
        g_lastSpawnTime = now;

        float centerX = windowW * 0.38f; float centerY = windowH / 2.0f;
        if (layoutMode == 0) { centerX = windowW / 2.0f; centerY = (windowW / (5.0f / 3.0f) / 2.0f) * 0.95f; }
        else if (layoutMode == 1) { centerX = (windowW * 0.5f) / 2.0f; centerY = windowH / 2.0f; }
        else if (layoutMode == 2) { centerX = windowW * 0.38f; centerY = windowH / 2.0f; }
        else if (layoutMode == 3) { centerX = windowW / 2.0f; centerY = windowH / 2.0f; }

        float yStaggerOffset = static_cast<float>(g_overlapCount) * cfg.fontSize;
        tick.pos = ImVec2(centerX + g_cumulativeXOffset, centerY - 20.0f - yStaggerOffset);
        tick.lifetime = cfg.lifetime;
        tick.maxLifetime = tick.lifetime;

        g_DamageTicks.push_back(tick);
    }

    void Update(const std::vector<MemoryScanner::TargetMonster>& activeMonsters, float windowW, float windowH, int layoutMode) {
        g_DisplayList.clear();

        if (activeMonsters.empty()) {
            g_DmgTriggerReason = "BLOCKED_BY_SCANNER_EMPTY";
        }

        for (auto it = g_RealMonsters.begin(); it != g_RealMonsters.end(); ) {
            auto srcIt = std::find_if(activeMonsters.begin(), activeMonsters.end(), [it](const MemoryScanner::TargetMonster& m) { return m.address == it->first; });
            if (srcIt == activeMonsters.end()) {
                it = g_RealMonsters.erase(it);
            }
            else { ++it; }
        }

        for (const auto& m : activeMonsters) {
            if (m.currentHp == 0 && m.maxHp == 0) {
                g_DmgTriggerReason = "REJECTED_INVALID_HP_ZERO";
                continue;
            }

            TrackedMonster& monster = g_RealMonsters[m.address];
            monster.currentHP = static_cast<uint16_t>(m.currentHp);
            monster.maxHP = static_cast<uint16_t>(m.maxHp);

            if (monster.isFirstLock) {
                monster.lastHP = monster.currentHP;
                monster.isFirstLock = false;
                g_DmgTriggerReason = "MONSTER_LOCK_INITIALIZED";
            }

            if (monster.currentHP < monster.lastHP) {
                SpawnDamageNumber(monster.lastHP - monster.currentHP, windowW, windowH, layoutMode);
                monster.lastHP = monster.currentHP;
                g_DmgTriggerReason = "EVENT_CALC_DAMAGE_SPAWNED";
            }
            else if (monster.currentHP > monster.lastHP) {
                monster.lastHP = monster.currentHP;
                g_DmgTriggerReason = "MONSTER_HP_RECOVERED";
            }
            else {
                g_DmgTriggerReason = "NO_HP_CHANGE_WAIT_HIT";
            }
            g_DisplayList.push_back({ monster.currentHP, monster.maxHP });
        }

        for (auto it = g_DamageTicks.begin(); it != g_DamageTicks.end(); ) {
            it->lifetime--;
            if (it->lifetime <= 0) it = g_DamageTicks.erase(it); else ++it;
        }

        // Ë¢ÐÂÒ£²â¿ìÕÕ
        g_Diag.trackedMapSize = g_RealMonsters.size();
        g_Diag.activeTicksCount = g_DamageTicks.size();
        strcpy_s(g_Diag.lastTriggerReason, g_DmgTriggerReason.c_str());
    }

    void MockUpdate(const std::vector<MockInput>& mockMonsters, float windowW, float windowH, int layoutMode) {
        g_DisplayList.clear();
        for (size_t i = 0; i < mockMonsters.size(); ++i) {
            TrackedMonster& monster = g_MockMonsters[static_cast<int>(i)];
            monster.currentHP = mockMonsters[i].hp;
            monster.maxHP = mockMonsters[i].max;
            if (monster.isFirstLock) { monster.lastHP = monster.currentHP; monster.isFirstLock = false; }
            if (monster.currentHP < monster.lastHP) {
                SpawnDamageNumber(monster.lastHP - monster.currentHP, windowW, windowH, layoutMode);
                monster.lastHP = monster.currentHP;
            }
            else if (monster.currentHP > monster.lastHP) { monster.lastHP = monster.currentHP; }
            g_DisplayList.push_back({ monster.currentHP, monster.maxHP });
        }
        for (auto it = g_DamageTicks.begin(); it != g_DamageTicks.end(); ) {
            it->lifetime--; if (it->lifetime <= 0) it = g_DamageTicks.erase(it); else ++it;
        }
    }

    const std::vector<DisplayMonster>& GetDisplayMonsters() { return g_DisplayList; }
    std::vector<DamageTick>& GetDamageTicks() { return g_DamageTicks; }
    std::string GetDmgTriggerReason() { return g_DmgTriggerReason; }
    Diagnostics GetDiagnostics() { return g_Diag; }
}