#pragma once
#include <string>

struct OverlayColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

class ConfigManager {
public:
    static ConfigManager& Get() {
        static ConfigManager instance;
        return instance;
    }

    // [Renderer]
    std::string fontPath = "C:\\Windows\\Fonts\\bahnschrift.ttf";
    float fontSize = 60.0f;
    bool showMonsterHP = true;      // 新增：控制怪物血量显示
    bool showDamageNumbers = true;  // 新增：控制伤害飘字显示
    OverlayColor damageColor = { 1.0f, 0.70f, 0.10f, 1.0f };
    bool damageShadowEnabled = true;
    OverlayColor damageShadowColor = { 0.0f, 0.0f, 0.0f, 0.85f };
    int damageShadowOffsetX = 2;
    int damageShadowOffsetY = 2;
    int damageShadowThickness = 2;

    // [Logic]
    int lifetime = 90;
    int fadeTime = 30;
    float xStaggerStep = 45.0f;
    int overlapMax = 10;

    // [Scanner]
    unsigned int hpMaxLimit = 40000;

    static void Load(const std::string& filePath);

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
};