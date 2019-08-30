#include <win32.cs>
set crtlib=crtdll.lib
set extralib=comdlg32.lib ctl3d32s.lib winmm.lib
!if exist *.bak del *.bak
!copy %project%.exe ..\.. > NUL:
!del %project%.exe
