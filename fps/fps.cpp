#include "fps/fps.h"

#include <chrono>
#include <thread>

namespace srr2
{
namespace fps
{

namespace
{

using Clock = std::chrono::steady_clock;
using Duration = std::chrono::duration<double>;

static bool g_Enabled = false;
static int g_TargetFps = 60;
static float g_FrameTimeMs = 16.6667f;
static float g_CurrentFps = 60.0f;
static bool g_Initialized = false;
static Clock::time_point g_LastFrameEnd;
static Clock::time_point g_NextFrameTarget;

} // anonymous namespace

void Limit()
{
    const Clock::time_point now = Clock::now();
    if (!g_Initialized)
    {
        g_LastFrameEnd = now;
        g_NextFrameTarget = now;
        g_Initialized = true;
        return;
    }

    if (g_Enabled && g_TargetFps > 0)
    {
        const Duration targetFrameDuration(1.0 / static_cast<double>(g_TargetFps));
        g_NextFrameTarget += std::chrono::duration_cast<Clock::duration>(targetFrameDuration);

        // If we fell behind by more than one frame (e.g., during loading), resync target
        if (now > g_NextFrameTarget)
        {
            g_NextFrameTarget = now;
        }
        else
        {
            // Sleep for the bulk of the remaining time, leaving ~1.5ms margin for scheduler granularity
            const auto sleepMargin = std::chrono::microseconds(1500);
            const auto remaining = g_NextFrameTarget - now;
            if (remaining > sleepMargin)
            {
                std::this_thread::sleep_for(remaining - sleepMargin);
            }

            // Spin-wait with std::chrono::steady_clock for sub-millisecond accuracy
            while (Clock::now() < g_NextFrameTarget)
            {
                std::this_thread::yield();
            }
        }
    }
    else
    {
        g_NextFrameTarget = now;
    }

    const Clock::time_point frameEnd = Clock::now();
    const std::chrono::duration<float, std::milli> elapsedMs = frameEnd - g_LastFrameEnd;
    g_LastFrameEnd = frameEnd;

    g_FrameTimeMs = elapsedMs.count();
    if (g_FrameTimeMs > 0.0001f)
    {
        g_CurrentFps = 1000.0f / g_FrameTimeMs;
    }
}

bool IsEnabled()
{
    return g_Enabled;
}

void SetEnabled(bool enabled)
{
    g_Enabled = enabled;
}

bool* GetEnabledPtr()
{
    return &g_Enabled;
}

int GetTargetFps()
{
    return g_TargetFps;
}

void SetTargetFps(int targetFps)
{
    if (targetFps < 1)
    {
        targetFps = 1;
    }
    g_TargetFps = targetFps;
}

int* GetTargetFpsPtr()
{
    return &g_TargetFps;
}

float GetFrameTimeMs()
{
    return g_FrameTimeMs;
}

float GetCurrentFps()
{
    return g_CurrentFps;
}

} // namespace fps
} // namespace srr2
