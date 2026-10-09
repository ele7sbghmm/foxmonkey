#pragma once

struct HWND__;
typedef HWND__* HWND;
struct IDirect3DDevice8;

class 图形用户界面类 { // túxíng yònghù jièmiàn lèi  graphical-user-interface-class
public:
    bool cull_se = true;
    bool cull_tl = true;
    bool cull_ui = true;
    bool 已初始化 = false; // yǐ chūshǐhuà
    bool device_已初始化 = false; 
	static long WndProc(HWND, unsigned int, unsigned int, long);
    void 初始化(HWND); // chūshǐhuà
    void 祯(); // zhēn
};
