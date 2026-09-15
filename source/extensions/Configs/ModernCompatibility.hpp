#pragma once

#include "extensions/Configuration.hpp"

inline struct ModernCompatibilityConfig {
    INI_CONFIG_SECTION("ModernCompatibility");

    // High-refresh defaults. 180 is intentionally the stock target for this
    // branch, but it remains configurable from gta-reversed.ini.
    int32 TargetFPS = 180;

    bool HighResolutionTimer = true;
    bool PerMonitorDPIAware = true;
    bool UseDesktopRefreshRate = true;
    bool ConfineCursorToGameWindow = true;
    bool EnablePS2VisualFixes = true;
    bool EnableSilentPatchFixes = true;

    void Load() {
        STORE_INI_CONFIG_VALUE(TargetFPS, 180);
        STORE_INI_CONFIG_VALUE(HighResolutionTimer, true);
        STORE_INI_CONFIG_VALUE(PerMonitorDPIAware, true);
        STORE_INI_CONFIG_VALUE(UseDesktopRefreshRate, true);
        STORE_INI_CONFIG_VALUE(ConfineCursorToGameWindow, true);
        STORE_INI_CONFIG_VALUE(EnablePS2VisualFixes, true);
        STORE_INI_CONFIG_VALUE(EnableSilentPatchFixes, true);
    }
} g_ModernCompatibilityConfig{};
