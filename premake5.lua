workspace "gproxy"
    configurations {
        "Debug", "Release"
    }
    location "gproxy"

project "gproxy"
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"
    characterset "MBCS"
    callingconvention "FastCall"
    architecture "x86"

    libdirs "lib"
    links { "luajit_stuff" }

    includedirs { "headers", "gluajit/src" }
    files { "headers/*.h", "src/*.c", "src/*.cpp", "headers/*.hpp", "gluajit/src/*.h" }
    removefiles { "src/luajit_stuff_cpp.cpp", "src/luajit_stuff.cpp" }

    vpaths {
        ["headers/*"] = "headers/*",
        ["src/*"] = "src/*",
        ["luajit/*"] = "gluajit/src/*.h"
    }
    defines "LUAJIT_DISABLE_FFI"

    configuration "Debug"
        defines "DEBUG"
        symbols "On"
        optimize "Debug"
        targetdir "debug"

    configuration "Release"
        defines "NDEBUG"
        optimize "Full"
        targetdir "release"

project "luajit_stuff"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    characterset "MBCS"
    callingconvention "Cdecl"
    architecture "x86"

    libdirs "lib"
    links { "lua_shared" }


    includedirs { "headers", "gluajit/src" }
    files { "headers/*.h", "src/luajit_stuff.cpp", "src/luajit_stuff_cpp.cpp", "headers/*.hpp", "gluajit/src/*.h" }
    defines { "LUAJIT_DISABLE_FFI", "LUA_BUILD_AS_DLL" }

    vpaths {
        ["headers/*"] = "headers/*",
        ["src/*"] = "src/luajit_stuff.cpp",
        ["luajit/*"] = "gluajit/src/*.h"
    }

    configuration "Debug"
        defines "DEBUG"
        symbols "On"
        optimize "Debug"
        targetdir "debug"

    configuration "Release"
        defines "NDEBUG"
        optimize "Full"
        targetdir "release"