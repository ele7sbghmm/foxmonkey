#pragma once

#include "MinHook.h"

namespace hooks {
    namespace elapsed {
        inline void* a = (void*)0x441ede;
        inline void* o = nullptr;
        inline __declspec(naked) void hk() {
            __asm {
                mov edi, 20
                jmp o
            }
        }
        inline void install() { MH_CreateHook(a, (void*)hk, (void**)&o); }
    }

    void install() {
        elapsed::install();
        MH_EnableHook(MH_ALL_HOOKS);
    }
}
