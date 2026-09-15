#include "StdInc.h"

#include "ModernCompatibility.hpp"
#include "extensions/Configs/ModernCompatibility.hpp"
#include "WinPlatform.h"

namespace notsa::modern {
namespace {
    bool g_TimerPeriodRaised{};
    bool g_CursorClipped{};
    uint32 g_NextTrafficDiversityRefreshMs{};

    constexpr uint32 MAX_SAFE_VEHICLE_MODEL_BUDGET = 22;

    uint32 SanitizeFrameRate(int32 fps) {
        return static_cast<uint32>(std::clamp(fps, 30, 1000));
    }

    uint32 SanitizeVehicleModelBudget(int32 budget) {
        // CLoadedCarGroup owns 23 slots. The stock PC streamer uses 22 so one
        // slot remains available while a model is phased out/replaced.
        return static_cast<uint32>(std::clamp(budget, 8, static_cast<int32>(MAX_SAFE_VEHICLE_MODEL_BUDGET)));
    }

    void EnablePerMonitorDpiAwareness() {
        if (!g_ModernCompatibilityConfig.PerMonitorDPIAware) {
            return;
        }

        // SetProcessDpiAwarenessContext is available on modern Windows 10/11.
        // Resolve it dynamically so the same binary keeps a graceful fallback
        // on older systems/toolchains.
        using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(HANDLE);
        if (const auto user32 = GetModuleHandleW(L"user32.dll")) {
            const auto setContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
                GetProcAddress(user32, "SetProcessDpiAwarenessContext")
            );
            if (setContext) {
                // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4.
                if (setContext(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4)))) {
                    return;
                }
            }
        }

        // Windows Vista+ fallback. Failure is harmless (for example if another
        // component configured DPI awareness before gta-reversed loaded).
        SetProcessDPIAware();
    }

    void ReleaseCursorClip() {
        if (g_CursorClipped) {
            ClipCursor(nullptr);
            g_CursorClipped = false;
        }
    }

    void ServiceCursorClip() {
        if (!g_ModernCompatibilityConfig.ConfineCursorToGameWindow || !RsGlobal.ps) {
            ReleaseCursorClip();
            return;
        }

        const auto hwnd = PSGLOBAL(window);
        if (!hwnd || GetForegroundWindow() != hwnd || IsIconic(hwnd)) {
            ReleaseCursorClip();
            return;
        }

        RECT rect{};
        if (!GetClientRect(hwnd, &rect)) {
            ReleaseCursorClip();
            return;
        }

        POINT topLeft{ rect.left, rect.top };
        POINT bottomRight{ rect.right, rect.bottom };
        if (!ClientToScreen(hwnd, &topLeft) || !ClientToScreen(hwnd, &bottomRight)) {
            ReleaseCursorClip();
            return;
        }

        RECT screenRect{
            topLeft.x,
            topLeft.y,
            bottomRight.x,
            bottomRight.y
        };

        // SilentPatch fixes the cursor escaping the game window on multi-monitor
        // systems. Keep the same behaviour here at engine level, while always
        // releasing the clip when the game loses focus so Alt+Tab stays normal.
        if (ClipCursor(&screenRect)) {
            g_CursorClipped = true;
        }
    }

    void ServiceTrafficDiversity() {
        if (!g_ModernCompatibilityConfig.EnableExpandedTrafficDiversity || !CStreaming::ms_bIsInitialised) {
            return;
        }

        // stream.ini may lower this value again after CStreaming::Init2(). Keep
        // the modern PC policy authoritative so the old PS2-era pressure does not
        // reduce the active vehicle model set back to a tiny number.
        const auto vehicleBudget = SanitizeVehicleModelBudget(g_ModernCompatibilityConfig.TrafficVehicleModelBudget);
        CStreaming::desiredNumVehiclesLoaded = vehicleBudget;

        if (CTimer::GetIsPaused() || !CPopCycle::m_pCurrZoneInfo) {
            return;
        }

        const auto minimumCivilianModels = static_cast<uint32>(std::clamp(
            g_ModernCompatibilityConfig.MinimumCivilianTrafficModels,
            3,
            static_cast<int32>(vehicleBudget)
        ));

        if (CPopulation::m_AppropriateLoadedCars.CountMembers() >= minimumCivilianModels) {
            return;
        }

        const auto now = CTimer::GetTimeInMS();
        if (g_NextTrafficDiversityRefreshMs != 0 && static_cast<int32>(now - g_NextTrafficDiversityRefreshMs) < 0) {
            return;
        }

        // Unlike the original emergency/mission-pressure behaviour, actively
        // replenish a normal zone-appropriate model. The normal CStreaming load
        // path still owns eviction and respects mission/game-required models.
        CStreaming::StreamOneNewCar();

        const auto refreshMs = static_cast<uint32>(std::clamp(
            g_ModernCompatibilityConfig.TrafficDiversityRefreshMs,
            250,
            10000
        ));
        g_NextTrafficDiversityRefreshMs = now + refreshMs;
    }
}

uint32 GetDesktopRefreshRate() {
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1) {
        return mode.dmDisplayFrequency;
    }
    return 60;
}

uint32 GetTargetFrameRate() {
    return SanitizeFrameRate(g_ModernCompatibilityConfig.TargetFPS);
}

void Initialise() {
    EnablePerMonitorDpiAwareness();

    if (g_ModernCompatibilityConfig.HighResolutionTimer) {
        g_TimerPeriodRaised = timeBeginPeriod(1) == TIMERR_NOERROR;
    }

    // RsInitialize writes APP_MAX_FPS later in startup. Service() re-applies the
    // configurable value once the engine is alive, so this initial assignment is
    // only an early default.
    RsGlobal.frameLimit = static_cast<int32>(GetTargetFrameRate());

    const auto desktopHz = GetDesktopRefreshRate();
    NOTSA_LOG_INFO(
        "Modern compatibility enabled: target={} FPS, desktop={} Hz, high-resolution timer={}",
        GetTargetFrameRate(),
        desktopHz,
        g_TimerPeriodRaised
    );
}

void Service() {
    // Keep the render limiter configurable even though the original RsInitialize
    // path rewrites it during startup. Gameplay's 30 Hz baseline is deliberately
    // left untouched; high-FPS fixes must be time-step based instead.
    RsGlobal.frameLimit = static_cast<int32>(GetTargetFrameRate());
    ServiceCursorClip();
    ServiceTrafficDiversity();
}

void Shutdown() {
    ReleaseCursorClip();
    g_NextTrafficDiversityRefreshMs = 0;

    if (g_TimerPeriodRaised) {
        timeEndPeriod(1);
        g_TimerPeriodRaised = false;
    }
}
}
