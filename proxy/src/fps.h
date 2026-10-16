#pragma once

class Fps {
public:
	bool use_fake_elapsed = false;
	unsigned fake_elapsed = 1;
	unsigned frame_count = 1;
};

inline Fps fps{};
