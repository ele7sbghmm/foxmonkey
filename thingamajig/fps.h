#pragma once

#include <chrono>

class FrameLimiter {
public:
    using Clock = std::chrono::high_resolution_clock;
    using DurationMs = std::chrono::duration<double, std::milli>;

    explicit FrameLimiter(double targetFps);

    void SetTargetFps(double targetFps);

    // Call at the VERY BEGINNING of your game loop / frame tick
    void BeginFrame();

    // Call at the VERY END of your game loop / frame tick
    void EndFrameAndWait();

    // --- Metrics Getters ---

    // Time spent processing game/extraction logic before the frame limiter slept (e.g., 3.2 ms)
    double GetLastWorkTimeMs() const;

    // Total elapsed time for the full frame, including limiter delay (e.g., 16.66 ms)
    double GetLastTotalFrameTimeMs() const;

    // Exact count of completed frames in the previous 1-second window
    uint32_t GetFramesInLastSecond() const;

private:
    DurationMs m_targetFrameDuration{ 0 };
    Clock::time_point m_targetTime;
    Clock::time_point m_frameStart;

    // Last frame duration metrics
    double m_lastWorkTimeMs{ 0.0 };
    double m_lastTotalFrameTimeMs{ 0.0 };

    // 1-second FPS accumulator metrics
    Clock::time_point m_fpsTimer;
    uint32_t m_fpsFrameCounter{ 0 };
    uint32_t m_framesInLastSecond{ 0 };
};
