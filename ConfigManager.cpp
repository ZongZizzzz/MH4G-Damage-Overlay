#include "ConfigManager.h"
#include <windows.h>

void ConfigManager::Load(const std::string& filePath) {
    ConfigManager& cfg = ConfigManager::Get();
    std::string fullPath = "./" + filePath;

    // 1. ∂¡»° [Renderer]
    char fontBuf[512];
    GetPrivateProfileStringA("Renderer", "FontPath", "C:\\Windows\\Fonts\\bahnschrift.ttf", fontBuf, sizeof(fontBuf), fullPath.c_str());
    cfg.fontPath = fontBuf;

    int rFontSize = GetPrivateProfileIntA("Renderer", "FontSize", 60, fullPath.c_str());
    cfg.fontSize = static_cast<float>(rFontSize);

    // 2. ∂¡»° [Logic]
    cfg.lifetime = GetPrivateProfileIntA("Logic", "Lifetime", 90, fullPath.c_str());
    cfg.fadeTime = GetPrivateProfileIntA("Logic", "FadeTime", 30, fullPath.c_str());

    int rXStagger = GetPrivateProfileIntA("Logic", "XStaggerStep", 45, fullPath.c_str());
    cfg.xStaggerStep = static_cast<float>(rXStagger);

    cfg.overlapMax = GetPrivateProfileIntA("Logic", "OverlapMax", 10, fullPath.c_str());

    // 3. ∂¡»° [Scanner]
    cfg.hpMaxLimit = static_cast<unsigned int>(GetPrivateProfileIntA("Scanner", "HpMaxLimit", 40000, fullPath.c_str()));
}