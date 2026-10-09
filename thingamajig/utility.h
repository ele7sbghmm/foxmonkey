#pragma once

#include "d3d8.h"
#include "p3d/utility.hpp"
#include "pddi/dx8/display.hpp"

inline IDirect3DDevice8* get_device() {
	return reinterpret_cast<d3dDisplay*>(p3d::display)->GetD3DDevice();
}
