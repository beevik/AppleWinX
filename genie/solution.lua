------------------------------------------------------------------------------
-- GENie configuration script for AppleWinX
--
-- GENie is project generator tool. It generates compiler solution and project
-- files from Lua scripts and supports many different compiler versions.
--
-- This configuration script generates Visual Studio solution and project
-- files for any Visual Studio version of interest. For instance, to generate
-- a solution for Visual Studio 2019, open a command prompt in the directory
-- containing this file, and type:
--
--   genie vs2019
--
-- This will create a Visual Studio solution file called AppleWinX.sln in the
-- same directory.
--
-- See https://github.com/bkaradzic/GENie for more information.
--
-- Full documentation:
--  https://github.com/bkaradzic/GENie/blob/master/docs/scripting-reference.md
------------------------------------------------------------------------------

-- Add the genie "clean" action
newaction {
    trigger     = "clean",
    description = "Clean the build directory",
}

-- Display usage if no action is specified on the command line.
if _ACTION == nil then
    return
end

-- Prepare commonly-used directory variables.
solutionDir = path.getabsolute(".")
rootDir     = path.getabsolute("..")
targetDir   = path.join(rootDir, "build")
objDir      = path.join(rootDir, "build/obj")
srcDir      = path.join(rootDir, "src")
rsrcDir     = path.join(rootDir, "resource")

if _ACTION == "clean" then
    os.rmdir(targetDir)
    return
end

-- Visual Studio always means windows.
if string.match(_ACTION, "vs.*") then
    _OPTIONS['os'] = "windows"
end

-- For gmake/ninja biulds, use clang instead of gcc.
if _ACTION == "gmake" or _ACTION == "ninja" then
    premake.gcc.cc   = "clang"
    premake.gcc.cxx  = "clang++"
    premake.gcc.ar   = "llvm-ar"
end


------------------------------------------------------------------------------
-- AppleWinX solution
--
-- Generate the Visual Studio solution file.
------------------------------------------------------------------------------
solution("AppleWinX")
    location(solutionDir)
    startproject("AppleWinX")

    language "C++"

    configurations {
        "Debug",
        "Release",
    }

    platforms {
        "x64",
        "x32",
    }

    flags {
        "NativeWChar",
        "Symbols",
    }

    configuration { "Debug*" }
        defines {
            "_DEBUG",
        }
        flags {
            "DebugRuntime",
        }

    configuration { "Release*" }
        defines {
            "NDEBUG",
        }
        flags {
            "OptimizeSpeed",
            "ReleaseRuntime",
        }

    configuration { "windows" }
        defines {
            "_WINDOWS",
            "WIN32",
        }

    configuration {} -- reset


------------------------------------------------------------------------------
-- set_output_dirs
--
-- Helper function to set a project's target and object directories.
------------------------------------------------------------------------------
function set_output_dirs(name)
    configuration { "Debug" }
        targetdir(path.join(targetDir, "Debug"))
        objdir(path.join(targetDir, "Debug/obj", name))

    configuration { "Release" }
        targetdir(path.join(targetDir, "Release"))
        objdir(path.join(targetDir, "Release/obj", name))

    configuration {} -- reset
end


------------------------------------------------------------------------------
-- AppleWinX project
--
-- Generate the AppleWinX executable project
------------------------------------------------------------------------------
project("AppleWinX")
    kind "WindowedApp"
    uuid(os.uuid("app-AppleWinX"))
    set_output_dirs("AppleWinX")

    -- Include paths
    includedirs {
        path.join(srcDir),
    }

    -- All files that should appear in the project
    files {
        path.join(srcDir, "*.h"),
        path.join(srcDir, "*.cpp"),
        path.join(srcDir, "*.inl"),
        path.join(rsrcDir, "*.rc"),
    }

    configuration { "vs*"}

        flags {
            "WinMain",
        }

        -- Add -D compile options
        defines {
            "_CRT_SECURE_NO_WARNINGS",
        }

        -- Add additional compile options
        buildoptions {
            "/utf-8",
        }

        -- Libraries that must be linked to build the executable
        links {
            "advapi32",
            "comctl32",
            "comdlg32",
            "dinput8",
            "dsound",
            "dxguid",
            "gdi32",
            "htmlhelp",
            "ole32",
            "shell32",
            "strmiids",
            "user32",
            "version",
            "winmm",
            "wsock32",
        }

        -- Set up precompiled headers
        pchheader("pch.h")
        pchsource(path.join(srcDir, "pch.cpp"))

    configuration {} -- reset
