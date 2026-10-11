#include "model/fps.h"

void FPS::SetFPS(float fps) {
    if (fps > 0) {
        target_fps = fps;
        target_duration = std::chrono::duration<float, std::milli>(1'000. / fps);
    }
}

void FPS::Start() {
    frame_start = std::chrono::steady_clock::now();
}

void FPS::End() {
    frames_this_second++;

    auto now = std::chrono::steady_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    if (sec - second >= 100) {
        second = sec;
        total_frames_last_second = frames_this_second;
        frames_this_second = 0;
    }
    
    target_time += std::chrono::duration_cast<std::chrono::steady_clock::duration>(target_duration);

    if (cap_fps)
        Wait();

    now = std::chrono::steady_clock::now();
    if (now - target_time > std::chrono::milliseconds(100))
        target_time = now;
}

void FPS::Wait() const {
    while (target_time - std::chrono::steady_clock::now() > std::chrono::milliseconds(2))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

#if defined(_M_X64) || defined(M_IX86)
    while (std::chrono::steady_clock::now() < target_time)
        _mm__pause();
#endif
}
