#pragma once

#include <cstdint>

using u8 = uint8_t;

struct IDirect3DDevice8;

namespace shar {
    struct d3dDisplay {
        u8 pad0[0x120];
        IDirect3DDevice8* device;
    };

    namespace p3d {
        inline auto* display = reinterpret_cast<d3dDisplay*>(0x0);
    }
}
