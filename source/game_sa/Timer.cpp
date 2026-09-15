/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"

#include "oswrapper.h"
#include "extensions/ModernCompatibility.hpp"

namespace {
// GTA stores its public timers as integer milliseconds. At high refresh rates a
// frame may be shorter than 1 ms; truncating every frame makes game timers stop
// advancing. Keep the fractional part between frames and only commit whole
// milliseconds to the original globals. This mirrors the timing correction
// used by SilentPatch while keeping gta-reversed's native CTimer implementation.
double g_TimeRemainderNonClippedMs{};
double g_TimeRemainderClippedMs{};
double g_TimeRemainderPauseModeMs{};

uint32 AccumulateWholeMilliseconds(double deltaMs, double& remainder) {
    double whole{};
    remainder = std::modf(deltaMs + remainder, &whole);
    return static_cast<uint32>(whole);
}

void ResetTimerRemainders() {
    g_TimeRemainderNonClippedMs = 0.0;
    g_TimeRemainderClippedMs = 0.0;
    g_TimeRemainderPauseModeMs = 0.0;
}
} // namespace

void CTimer::InjectHooks()
{
    RH_ScopedClass(CTimer);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Initialise, 0x5617E0);
    RH_ScopedInstall(Shutdown, 0x5618C0);
    RH_ScopedInstall(Suspend, 0x5619D0);
    RH_ScopedInstall(Resume, 0x561A00);
    RH_ScopedInstall(Stop, 0x561AA0);
    RH_ScopedInstall(StartUserPause, 0x561AF0);
    RH_ScopedInstall(EndUserPause, 0x561B00);
    RH_ScopedInstall(GetCyclesPerMillisecond, 0x561A40);
    RH_ScopedInstall(GetCyclesPerFrame, 0x561A50);
    RH_ScopedInstall(GetCurrentTimeInCycles, 0x561A80);
    RH_ScopedInstall(GetIsSlowMotionActive, 0x561AD0);
    RH_ScopedInstall(UpdateVariables, 0x5618D0);
    RH_ScopedInstall(Update, 0x561B10);
}

// 64-bit RsTimer wrapper
// 0x5617C0
uint64 GetMillisecondTime() {
    return plugin::CallAndReturn<uint64, 0x5617C0>();
    // return RsTimer();
}

// 0x5617E0
void CTimer::Initialise()
{
    m_UserPause = false;
    m_CodePause = false;
    bSlowMotionActive = false;
    bSkipProcessThisFrame = false;

    m_snTimeInMilliseconds = 0;
    m_snTimeInMillisecondsPauseMode = 1;
    m_snTimeInMillisecondsNonClipped = 1;
    m_snPreviousTimeInMilliseconds = 0;
    m_snPPreviousTimeInMilliseconds = 0;
    m_snPPPreviousTimeInMilliseconds = 0;
    m_snPPPPreviousTimeInMilliseconds = 0;
    m_snPreviousTimeInMillisecondsNonClipped = 0;

    m_snRenderTimerPauseCount = 0;

    m_FrameCounter = 0;
    m_sbEnableTimeDebug = false;
    game_FPS = 0.0f;

    ms_fTimeScale = 1.0f;
    ms_fSlowMotionScale = -1.0f; // unused
    ms_fTimeStep = 1.0f;
    ms_fOldTimeStep = 1.0f;

    ResetTimerRemainders();

    TimerFunction_t timerFunc;
    auto frequency = GetOSWPerformanceFrequency();
    if (frequency) {
        timerFunc = GetOSWPerformanceTime;
        m_snTimerDivider = (uint32)(frequency / 1000);
    } else {
        timerFunc = GetMillisecondTime;
        m_snTimerDivider = 1;
    }
    ms_fnTimerFunction = timerFunc;
    m_snRenderStartTime = timerFunc();
}

// 0x5618C0
void CTimer::Shutdown() {
    m_sbEnableTimeDebug = false;
}

// 0x5619D0
void CTimer::Suspend()
{
    if (m_sbEnableTimeDebug)
    {
        if (++m_snRenderTimerPauseCount <= 1)
            m_snRenderPauseTime = ms_fnTimerFunction();
    }
}

// 0x561A00
void CTimer::Resume()
{
    if (m_sbEnableTimeDebug)
    {
        if (!--m_snRenderTimerPauseCount) {
            m_snRenderStartTime = ms_fnTimerFunction() - m_snRenderPauseTime + m_snRenderStartTime;
        }
    }
}

// 0x561AA0
void CTimer::Stop()
{
    m_snPPPPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_snPPPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_snPPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_snPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_sbEnableTimeDebug = false;
    m_snPreviousTimeInMillisecondsNonClipped = m_snTimeInMillisecondsNonClipped;
    ResetTimerRemainders();
}

// 0x561AF0
void CTimer::StartUserPause()
{
    m_UserPause = true;
}

// 0x561B00
void CTimer::EndUserPause()
{
    m_UserPause = false;
}

// 0x561A40
uint32 CTimer::GetCyclesPerMillisecond()
{
    return m_snTimerDivider;
}

// cycles per ms * 20
// 0x561A50
uint32 CTimer::GetCyclesPerFrame()
{
    return (uint32)((float)m_snTimerDivider * 20.0f);
}

// 0x561A80
uint32 CTimer::GetCurrentTimeInCycles()
{
    // The public ABI is 32-bit, but calculation remains 64-bit until the final
    // conversion. Frame pacing no longer relies on this value for long-running
    // high-refresh sessions.
    const uint64 now = ms_fnTimerFunction ? ms_fnTimerFunction() : GetOSWPerformanceTime();
    return static_cast<uint32>(now - m_snRenderStartTime);
}

// 0x561AD0
bool CTimer::GetIsSlowMotionActive()
{
    return ms_fTimeScale < 1.0f;
}

// 0x5618D0
void CTimer::UpdateVariables(float timeElapsed)
{
    // Keep the fractional millisecond component rather than truncating it every
    // frame. This is important at 180 Hz and essential above 1000 FPS.
    const double frameDeltaMs = double(timeElapsed) / double(m_snTimerDivider);

    m_snTimeInMillisecondsNonClipped += AccumulateWholeMilliseconds(
        frameDeltaMs,
        g_TimeRemainderNonClippedMs
    );
    ms_fTimeStepNonClipped = float(frameDeltaMs / double(TIMESTEP_LEN_IN_MS));

    m_snTimeInMilliseconds += AccumulateWholeMilliseconds(
        std::min(frameDeltaMs, 300.0),
        g_TimeRemainderClippedMs
    );

    if (!m_UserPause && !m_CodePause && !CSpecialFX::bSnapShotActive) {
        // Make it be something at least, to avoid division by 0.
        ms_fTimeStepNonClipped = std::max(ms_fTimeStepNonClipped, 0.01f);
    }

    ms_fOldTimeStep = ms_fTimeStep;
    SetTimeStep(std::clamp(ms_fTimeStepNonClipped, 0.00001f, 3.0f));
}

// 0x561B10
void CTimer::Update() {
    ZoneScoped;

    if (!ms_fnTimerFunction)
        return;

    // By this point the RenderWare/platform globals are initialized. Running the
    // modern compatibility service here keeps high-refresh and cursor/focus
    // behaviour in sync without introducing a polling/background thread.
    notsa::modern::Service();

    m_sbEnableTimeDebug = true;

    // Update history
    m_snPPPPreviousTimeInMilliseconds = m_snPPPreviousTimeInMilliseconds;
    m_snPPPreviousTimeInMilliseconds = m_snPPreviousTimeInMilliseconds;
    m_snPPreviousTimeInMilliseconds = m_snPreviousTimeInMilliseconds;
    m_snPreviousTimeInMilliseconds = m_snTimeInMilliseconds;
    m_snPreviousTimeInMillisecondsNonClipped = m_snTimeInMillisecondsNonClipped;

    const uint64 nRenderTimeBefore = m_snRenderStartTime;
    m_snRenderStartTime = ms_fnTimerFunction();

    const uint64 rawDeltaTicks = m_snRenderStartTime - nRenderTimeBefore;
    const double rawDeltaMs = double(rawDeltaTicks) / double(m_snTimerDivider);
    game_FPS = rawDeltaMs > 0.0 ? float(1000.0 / rawDeltaMs) : 0.0f;

    double timeDeltaTicks = double(rawDeltaTicks);
    if (!GetIsPaused())
        timeDeltaTicks *= double(ms_fTimeScale);

    const double pauseModeDeltaMs = timeDeltaTicks / double(m_snTimerDivider);
    m_snTimeInMillisecondsPauseMode += AccumulateWholeMilliseconds(
        pauseModeDeltaMs,
        g_TimeRemainderPauseModeMs
    );

    if (GetIsPaused())
        timeDeltaTicks = 0.0;

    UpdateVariables(float(timeDeltaTicks));
    m_FrameCounter++;
}
