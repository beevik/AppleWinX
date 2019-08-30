#include <win32.cs>
set crtlib=
set extralib=
set winlib=
set linkopt=%linkopt% -entry:DllMain
!if exist *.bak del *.bak
!copy %project%.dll ..\.. > NUL:
!del %project%.dll
