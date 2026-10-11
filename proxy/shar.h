#pragma once

#include <cstdint>

using u8 = uint8_t;

struct IDirect3DDevice8;

namespace shar {
    namespace p3d {
        inline auto* display = reinterpret_cast<struct d3dDisplay*>(0x0);
    }

    struct d3dDisplay {
        u8 pad0[0x124];
        IDirect3DDevice8* device;
    };
}
