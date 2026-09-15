#include "StdInc.h"

#include "ModernCompatibility.hpp"
#include "extensions/Configs/ModernCompatibility.hpp"

namespace notsa::modern {
namespace {
    bool g_TimerPeriodRaised{};

    uint32 SanitizeFrameRate(int32 fps) {
        return static_cast<uint32>(std::clamp(fps, 30, 1000));
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

    // The stock PC game targets 30 FPS when its limiter is enabled. This branch
    // deliberately targets 180 FPS by default. CTimer has been made fractional
    // millisecond-safe so this does not speed up the simulation or freeze
    // millisecond timers.
    RsGlobal.frameLimit = static_cast<int32>(GetTargetFrameRate());

    const auto desktopHz = GetDesktopRefreshRate();
    NOTSA_LOG_INFO(
        "Modern compatibility enabled: target={} FPS, desktop={} Hz, high-resolution timer={}",
        GetTargetFrameRate(),
        desktopHz,
        g_TimerPeriodRaised
    );
}

void Shutdown() {
    if (g_TimerPeriodRaised) {
        timeEndPeriod(1);
        g_TimerPeriodRaised = false;
    }
}
}
