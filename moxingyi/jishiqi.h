#pragma once

#include <chrono>

class 计时器 { // 计时器 jìshíqì  count-time-device  时钟类 time-clock-class 
public:
    using 时钟 = std::chrono::high_resolution_clock;
    using 时长 = std::chrono::duration<double, std::milli>;
    using rad_时长 = std::chrono::duration<unsigned, std::milli>;

    计时器() = default;
    void Begin(unsigned radNewTime);
    void 结束(); // jiéshù
    void 等待() const; // děngdài
    void SetTargetFps(double target);
	unsigned GetElapsedRad() const {
        return use_fake_time ? fake_经过时间 : rad_经过时间.count();
    }

    rad_时长 rad_当前时间{}; // dāngqián shíjiān current time
    rad_时长 rad_经过时间{}; // jīngguò shíjiān pass time

    时钟::time_point target_time{};
    时钟::time_point frame_start{};
    时钟::time_point fps_timer;
    时长 target_duration{ 0 };
    double last_work_time = 0.;
    double interpolated_fps = 0.;
    double elapsed = 0.;

    bool use_fake_time = false;
    bool cap_fps = false;
    unsigned fake_经过时间 = 2;
      
    double fps = 30.;
    uint32_t frame_counter = 0;
    uint32_t frames_last_second = 0;

};

class 帧率计数器 { // zhēn lǜ jì shù qì

};
