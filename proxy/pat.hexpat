struct GameFlow {
    padding[8];
    u32 CurrentContext[1];
};

struct IDirect3DDevice8 { };
struct d3dDisplay {
    padding[0x120];
    IDirect3DDevice8* device : u32;
};

GameFlow* gf : u32 @0x6c9014;
d3dDisplay* pddi : u32 @0x65ef5c;