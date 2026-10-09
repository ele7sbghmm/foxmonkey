#pragma once

struct HWND__;
typedef HWND__* HWND;
struct IDirect3DDevice8;

class 图形界面 { // túxíng jièmiàn
public:
	图形界面() = default;
	void 初始化(HWND, IDirect3DDevice8*);
};
