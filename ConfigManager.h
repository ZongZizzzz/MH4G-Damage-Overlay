#pragma once
#include <string>

class ConfigManager {
public:
    static ConfigManager& Get() {
        static ConfigManager instance;
        return instance;
    }

    // [Renderer]
    std::string fontPath = "C:\\Windows\\Fonts\\bahnschrift.ttf";
    float fontSize = 60.0f;

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