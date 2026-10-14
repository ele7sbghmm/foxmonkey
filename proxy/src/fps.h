#pragma once

class Fps {
public:
	bool use_fake_elapsed_gameflow = false;
	bool use_fake_elapsed_renderflow = true;
	unsigned fake_elapsed_gameflow = 1;
	unsigned fake_elapsed_renderflow = 1000;
};

inline Fps fps{};
