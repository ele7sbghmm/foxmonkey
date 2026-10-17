workspace 'd3d8_workspace'
    configurations { 'Debug' }
    architecture 'x86'
    location 'build'
    cppdialect 'C++26'

project "d3d8_project"
    kind 'SharedLib'
    language 'C++'
    targetdir '../shar'
    targetprefix ''
    targetname 'd3d8'
    targetextension '.dll'

    includedirs {
        '../vendor',
        '../vendor/imgui',
        '../vendor/MinHook_134_lib/include',
        '../vendor/min-dx8-sdk',
        '../vendor/rl-tools/include',
        'C:/Program Files/HDF_Group/HDF5/2.2.0/include'
    }
    libdirs { '../vendor/MinHook_134_lib/lib' }

    files {
        'src/**.cpp',
        'src/**.h',
        '../vendor/imgui/*.cpp',
        '../vendor/imgui/*.h', 
        '../vendor/imgui/backends/imgui_impl_win32.*',
        '../vendor/imgui/backends/imgui_impl_dx8.*',
    }
    filter "action:vs*"
        linkoptions { '/DEF:"../src/d3d8.def"' }
        debugcommand "C:/Users/m/Documents/foxmonkey/shar/Simpsons-Final.exe"
        debugdir "../shar"

    filter {}
