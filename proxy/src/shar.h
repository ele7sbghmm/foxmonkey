#pragma once

#include <cstdint>

using u8 = uint8_t;
using u32 = uint32_t;

struct IDirect3DDevice8;

namespace shar {
    enum ContextEnum : u32 {
        FRONTEND = 2,
        LOADING_GAMEPLAY = 8,
        GAMEPLAY,
        PAUSE
    };
    struct GameFlow {
        u8 pad[8];
        ContextEnum mCurrentContext;
        static auto* get() { return reinterpret_cast<GameFlow*>(0x6c9014); }
    };

    struct d3dContext {
        u8 pad[0x120];
        IDirect3DDevice8* device;
        static auto* get() { return reinterpret_cast<d3dContext*>(0x65ef5c); }
    };
}
