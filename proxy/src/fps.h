#pragma once

class Fps {
public:
	unsigned Hook(unsigned new_time) {
		rad_elapsed = new_time - rad_time;
		rad_time = new_time;
		return use_fake_elapsed ? fake_elapsed : rad_elapsed;
	}
	unsigned rad_elapsed = 0;
	unsigned rad_time = 0;
	unsigned fake_elapsed = 10;
	bool use_fake_elapsed = true;
};

inline Fps fps{};
