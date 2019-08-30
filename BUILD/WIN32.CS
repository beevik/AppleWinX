// IF THE USER TRIES TO RUN THIS SCRIPT DIRECTLY, INSTEAD OF USING THIS 
// SCRIPT BY INCLUDING IT FROM ANOTHER ONE, DISPLAY AN ERROR
if not %project%==win32 goto start
echo You must specify a project script.
echo Type "C /?" for help.
halt
:start

// SET UP AUTO-DEPENDENCY CHECKING 
set -autodependencies  =.c;.cpp;.h;.hpp
set -ignoredependencies=windows.h;windowsx.h;ole2.h;rpc.h;ddraw.h

// SET THE DEFAULT COMMAND LINE OPTIONS FOR THE COMPILER AND LINKER
if %debug% set clopt=-D_DEBUG -Zi
if %debug% set linkopt=-debug
if not %debug% set clopt=-DNDEBUG -O2b2 -G5Fr
set mlopt=-Cx
set subsystem=windows
set crtlib=libc.lib
set winlib=kernel32.lib user32.lib gdi32.lib advapi32.lib

// SET THE DEFAULT BUILD RULES
set .asm=.obj
set .c  =.obj
set .cpp=.obj
set .def=.exp
set .rc =.res
if exist %project%.def set .obj=.dll
if not exist %project%.def set .obj=.exe
set .asm.obj=ml -nologo -c -coff %mlopt% %file%
set .c.obj  =cl -nologo -c -D_X86_ -DWIN32 %clopt% -YX %file%
set .cpp.obj=cl -nologo -c -D_X86_ -DWIN32 %clopt% -YX %file%
set .def.exp=lib -nologo -machine:i386 -def:%project%.def -out:%project%.lib *.obj
set .rc.res =rc -r %file%
set .obj.dll=link -nologo -machine:i386 -subsystem:%subsystem% %linkopt% -dll -out:%project%.dll *.obj *.res %project%.exp -nod %crtlib% %winlib% %extralib%
set .obj.exe=link -nologo -machine:i386 -subsystem:%subsystem% %linkopt% -out:%project%.exe *.obj *.res -nod %crtlib% %winlib% %extralib%
