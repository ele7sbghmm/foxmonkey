#include "玩意儿/玩意儿.h"

#include "p3d/utility.hpp"
#include "pddi/dx8/display.hpp"

struct IDirect3DDevice8;

void 玩意儿::初始化(HWND hWnd) { // chūshǐhuà
	d3dDisplay* 显示 = reinterpret_cast<d3dDisplay*>(p3d::display); // xiǎnshì
	IDirect3DDevice8* 设备 = 显示->GetD3DDevice(); // shèbèi


}
