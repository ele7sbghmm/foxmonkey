#pragma once

#include "main.h"
#include "shar.h"
#include "fps.h"

#include "MinHook.h"

struct HWND__;
typedef HWND__* HWND;

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
        inline void install() { MH_CreateHook(a, (void*)hk, (void**)&o); }
    }

    namespace Elapsed {
        inline void* a = (void*)0x441ede;
        inline void* o = nullptr;
        inline __declspec(naked) void hk() {
            __asm {
                pushad
                push edi
                lea ecx, fps
                mov eax, [Fps::Hook]
                call eax
                mov [esp + 0], eax
                popad
                jmp o
            }
        }
        inline void install() { MH_CreateHook(a, (void*)hk, (void**)&o); }
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
        inline void install() { MH_CreateHook(a, (void*)hk, (void**)&o); }
    }
        
    void install() {
        WndProc::install();
        Elapsed::install();
        DrawGui::install();
        MH_EnableHook(MH_ALL_HOOKS);
    }
}
