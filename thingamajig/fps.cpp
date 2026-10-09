#include "thingamajig/fps.h"

#include <thread>
#include <cstdint>
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#if defined(_M_X64) || defined(_M_IX86)
#include <emmintrin.h>
#endif

FrameLimiter::FrameLimiter(double targetFps) {
    SetTargetFps(targetFps);
    auto now = Clock::now();
    m_frameStart = now;
    m_fpsTimer = now;
    m_targetTime = now;

    timeBeginPeriod(1);
}

void FrameLimiter::SetTargetFps(double targetFps) {
    if (targetFps > 0.0) {
        m_targetFrameDuration = DurationMs(1000.0 / targetFps);
    }
}

// Call at the VERY BEGINNING of your game loop / frame tick
void FrameLimiter::BeginFrame() {
    m_frameStart = Clock::now();
}

// Call at the VERY END of your game loop / frame tick
void FrameLimiter::EndFrameAndWait() {
    auto workEnd = Clock::now();

    // 1. Calculate time taken for ACTUAL WORK this frame (excluding limiter sleep)
    m_lastWorkTimeMs = std::chrono::duration_cast<DurationMs>(workEnd - m_frameStart).count();

    // --- Hybrid Sleep & Spin-Wait Limiter ---
    m_targetTime += std::chrono::duration_cast<Clock::duration>(m_targetFrameDuration);
    auto now = Clock::now();

    while (m_targetTime - now > std::chrono::milliseconds(2)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        now = Clock::now();
    }

    while (Clock::now() < m_targetTime) {
#if defined(_M_X64) || defined(_M_IX86)
        _mm_pause();
#endif
    }

    auto frameEnd = Clock::now();

    // 2. Calculate TOTAL duration of the frame (Work + Limiter Sleep)
    double elapsed = std::chrono::duration_cast<DurationMs>(frameEnd - m_frameStart).count();
    m_lastTotalFrameTimeMs = elapsed > .001 ? elapsed : .001;

    // 3. Track frames completed in the previous second
    m_fpsFrameCounter++;
    if (frameEnd - m_fpsTimer >= std::chrono::seconds(1)) {
        m_framesInLastSecond = m_fpsFrameCounter;
        m_fpsFrameCounter = 0;
        m_fpsTimer += std::chrono::seconds(1); // Advance by 1s to prevent timer drift
    }

    // Overrun recovery
    if (frameEnd - m_targetTime > std::chrono::milliseconds(100)) {
        m_targetTime = frameEnd;
    }
}

// --- Metrics Getters ---

// Time spent processing game/extraction logic before the frame limiter slept (e.g., 3.2 ms)
double FrameLimiter::GetLastWorkTimeMs() const { return m_lastWorkTimeMs; }

// Total elapsed time for the full frame, including limiter delay (e.g., 16.66 ms)
double FrameLimiter::GetLastTotalFrameTimeMs() const { return m_lastTotalFrameTimeMs; }

// Exact count of completed frames in the previous 1-second window
uint32_t FrameLimiter::GetFramesInLastSecond() const { return m_framesInLastSecond; }

