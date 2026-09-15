#include "StdInc.h"

#include "ModernCompatibility.hpp"
#include "extensions/Configs/ModernCompatibility.hpp"
#include "WinPlatform.h"

namespace notsa::modern {
namespace {
    bool g_TimerPeriodRaised{};
    bool g_CursorClipped{};

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
}

void Shutdown() {
    ReleaseCursorClip();

    if (g_TimerPeriodRaised) {
        timeEndPeriod(1);
        g_TimerPeriodRaised = false;
    }
}
}
