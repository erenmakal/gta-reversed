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

    // The original trilogy traffic streamer was designed around the PS2's tiny
    // memory budget. San Andreas' PC structures can safely keep 22 vehicle models
    // active (CLoadedCarGroup has 23 slots, with one slot effectively needed for
    // replacement/streaming churn). Keep a healthy civilian subset so police,
    // emergency and mission vehicles cannot collapse normal traffic diversity.
    bool  EnableExpandedTrafficDiversity = true;
    int32 TrafficVehicleModelBudget = 22;
    int32 MinimumCivilianTrafficModels = 12;
    int32 TrafficDiversityRefreshMs = 1500;

    void Load() {
        STORE_INI_CONFIG_VALUE(TargetFPS, 180);
        STORE_INI_CONFIG_VALUE(HighResolutionTimer, true);
        STORE_INI_CONFIG_VALUE(PerMonitorDPIAware, true);
        STORE_INI_CONFIG_VALUE(UseDesktopRefreshRate, true);
        STORE_INI_CONFIG_VALUE(ConfineCursorToGameWindow, true);
        STORE_INI_CONFIG_VALUE(EnablePS2VisualFixes, true);
        STORE_INI_CONFIG_VALUE(EnableSilentPatchFixes, true);
        STORE_INI_CONFIG_VALUE(EnableExpandedTrafficDiversity, true);
        STORE_INI_CONFIG_VALUE(TrafficVehicleModelBudget, 22);
        STORE_INI_CONFIG_VALUE(MinimumCivilianTrafficModels, 12);
        STORE_INI_CONFIG_VALUE(TrafficDiversityRefreshMs, 1500);
    }
} g_ModernCompatibilityConfig{};
