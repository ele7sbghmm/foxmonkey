#pragma once

#include "main.h"
#include "shar.h"
#include "fps.h"

#include "MinHook.h"

struct HWND__;
typedef HWND__* HWND;

struct Reg { uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; };

namespace hooks {
    namespace WndProc {
        inline void* a = (void*)0x444721;
        inline void* o = nullptr;
        inline void __stdcall wndproc(HWND hwnd, unsigned int msg, unsigned int wparam, long lparam) {
            Gui::WndProc(hwnd, msg, wparam, lparam);
        }
        inline __declspec(naked) void hk() {
            __asm {
                pushad
                push esi
                push ebx
                push ebp
                push edi
                call wndproc
                popad
                jmp o
            }
        }
        inline void install() { MH_CreateHook(a, (void*)hk, &o); }
    }

    namespace GameFlow_OnTimerDone {
        inline void* a = (void*)0x432e74;
        inline void* o = nullptr;
        inline void __cdecl fn(Reg* reg) {
            fps.frame_count++;
            if (fps.use_fake_elapsed)
                reg->esi = fps.fake_elapsed;
        }
        inline __declspec(naked) void hk() {
            __asm {
                pushad
                push esp
                call fn
                add esp, 4
                popad
                jmp o
            }
        }
        inline void install() { MH_CreateHook(a, (void*)hk, &o); }
    }
    
    namespace DrawGui {
        inline void* a = (void*)0x4a8dd8;
        inline void* o = nullptr;
        inline void __stdcall fn() {
            Gui::Frame();
        }
        inline __declspec(naked) void hk() {
            __asm {
                pushad
                call fn
                popad
                jmp o
            }
        }
        inline void install() { MH_CreateHook(a, (void*)hk, &o); }
    }

    namespace radGetMilli {
        inline void* a = (void*)0x56294b;
        inline void* o = nullptr;
        inline unsigned __cdecl fn() {
            return fps.frame_count;
        }
        inline __declspec(naked) void hk() {
            __asm {
                call fn
                leave
                ret
            }
        }
        inline void install() { MH_CreateHook(a, (void*)hk, &o); }
    }
        
    void install() {
        WndProc::install();
        GameFlow_OnTimerDone::install();
        DrawGui::install();
        radGetMilli::install();
        MH_EnableHook(MH_ALL_HOOKS);
    }
}
