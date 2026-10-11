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
        "vencor/imgui"
        "vendor/MinHook_134_lib/include"
    }

    files { 'src/**.cpp', 'src/**.h' }
    filter "action:vs*"
        linkoptions { '/DEF:"../src/d3d8.def"' }
        debugcommand "C:/Users/m/Documents/foxmonkey/shar/Simpsons-Final.exe"
        debugdir "../shar"
