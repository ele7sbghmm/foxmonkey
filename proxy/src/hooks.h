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
        inline void __cdecl wndproc(Reg* reg) {
            Gui::WndProc((HWND)reg->edi, reg->ebp, reg->ebx, reg->esi);
        }
        inline __declspec(naked) void hk() {
            __asm {
                pushad
                push esp
                call wndproc
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

    void install() {
        WndProc::install();
        DrawGui::install();
    }
}
