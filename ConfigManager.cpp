#include "ConfigManager.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>


namespace {
    int ClampInt(int value, int minValue, int maxValue) {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    float ByteToFloat(unsigned int value) {
        return static_cast<float>(value) / 255.0f;
    }

    bool ParseHexColor(const char* input, OverlayColor& outColor) {
        if (!input) return false;

        std::string value = input;
        value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c) != 0; }), value.end());
        if (!value.empty() && value[0] == '#') value.erase(0, 1);
        if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0) value.erase(0, 2);
        if (value.length() != 6 && value.length() != 8) return false;

        char* end = nullptr;
        unsigned long parsed = std::strtoul(value.c_str(), &end, 16);
        if (!end || *end != '\0') return false;

        if (value.length() == 6) {
            outColor.r = ByteToFloat((parsed >> 16) & 0xFF);
            outColor.g = ByteToFloat((parsed >> 8) & 0xFF);
            outColor.b = ByteToFloat(parsed & 0xFF);
            outColor.a = 1.0f;
        }
        else {
            outColor.r = ByteToFloat((parsed >> 24) & 0xFF);
            outColor.g = ByteToFloat((parsed >> 16) & 0xFF);
            outColor.b = ByteToFloat((parsed >> 8) & 0xFF);
            outColor.a = ByteToFloat(parsed & 0xFF);
        }
        return true;
    }

    OverlayColor ReadHexColor(const char* section, const char* key, const char* fallback, const std::string& fullPath, const OverlayColor& currentValue) {
        char colorBuf[32];
        GetPrivateProfileStringA(section, key, fallback, colorBuf, sizeof(colorBuf), fullPath.c_str());
        OverlayColor parsed = currentValue;
        ParseHexColor(colorBuf, parsed);
        return parsed;
    }
}


void ConfigManager::Load(const std::string& filePath) {
    ConfigManager& cfg = ConfigManager::Get();
    std::string fullPath = "./" + filePath;

    // 1. ∂¡»° [Renderer]
    char fontBuf[512];
    GetPrivateProfileStringA("Renderer", "FontPath", "C:\\Windows\\Fonts\\bahnschrift.ttf", fontBuf, sizeof(fontBuf), fullPath.c_str());
    cfg.fontPath = fontBuf;

    int rFontSize = GetPrivateProfileIntA("Renderer", "FontSize", 60, fullPath.c_str());
    cfg.fontSize = static_cast<float>(rFontSize);

    cfg.showMonsterHP = GetPrivateProfileIntA("Renderer", "ShowMonsterHP", 1, fullPath.c_str()) != 0;
    cfg.showDamageNumbers = GetPrivateProfileIntA("Renderer", "ShowDamageNumbers", 1, fullPath.c_str()) != 0;

    cfg.damageColor = ReadHexColor("Renderer", "DamageColor", "#FFB31A", fullPath, cfg.damageColor);
    cfg.damageShadowEnabled = GetPrivateProfileIntA("Renderer", "DamageShadowEnabled", 1, fullPath.c_str()) != 0;
    cfg.damageShadowColor = ReadHexColor("Renderer", "DamageShadowColor", "#000000D9", fullPath, cfg.damageShadowColor);
    cfg.damageShadowOffsetX = GetPrivateProfileIntA("Renderer", "DamageShadowOffsetX", 2, fullPath.c_str());
    cfg.damageShadowOffsetY = GetPrivateProfileIntA("Renderer", "DamageShadowOffsetY", 2, fullPath.c_str());
    cfg.damageShadowThickness = ClampInt(GetPrivateProfileIntA("Renderer", "DamageShadowThickness", 2, fullPath.c_str()), 0, 8);

    // 2. ∂¡»° [Logic]
    cfg.lifetime = GetPrivateProfileIntA("Logic", "Lifetime", 90, fullPath.c_str());
    cfg.fadeTime = GetPrivateProfileIntA("Logic", "FadeTime", 30, fullPath.c_str());

    int rXStagger = GetPrivateProfileIntA("Logic", "XStaggerStep", 45, fullPath.c_str());
    cfg.xStaggerStep = static_cast<float>(rXStagger);

    cfg.overlapMax = GetPrivateProfileIntA("Logic", "OverlapMax", 10, fullPath.c_str());

    // 3. ∂¡»° [Scanner]
    cfg.hpMaxLimit = static_cast<unsigned int>(GetPrivateProfileIntA("Scanner", "HpMaxLimit", 40000, fullPath.c_str()));
}