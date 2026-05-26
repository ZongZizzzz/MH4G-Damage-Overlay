#include "MockGenerator.h"
#include <cstdlib>

namespace MockGenerator {
    static uint16_t s_m1HP = 18500; static uint16_t s_m1Max = 18500;
    static uint16_t s_m2HP = 6400;  static uint16_t s_m2Max = 6400;
    static bool     s_spaceWasDown = false;
    static bool     s_cKeyWasDown = false;

    void Run(float windowW, float windowH, int layoutMode) {
        bool spaceIsDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        bool cKeyIsDown = (GetAsyncKeyState('C') & 0x8000) != 0;

        if (spaceIsDown && !s_spaceWasDown) {
            int dmg = rand() % 350 + 80;
            if (s_m1HP > dmg) s_m1HP -= dmg; else s_m1HP = s_m1Max;
        }
        s_spaceWasDown = spaceIsDown;

        if (cKeyIsDown && !s_cKeyWasDown) {
            int dmg = rand() % 210 + 45;
            if (s_m2HP > dmg) s_m2HP -= dmg; else s_m2HP = s_m2Max;
        }
        s_cKeyWasDown = cKeyIsDown;

        std::vector<GameLogic::MockInput> inputs = { {s_m1HP, s_m1Max}, {s_m2HP, s_m2Max} };
        GameLogic::MockUpdate(inputs, windowW, windowH, layoutMode);
    }
}