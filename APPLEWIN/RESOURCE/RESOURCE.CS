#include <win32.cs>
!if exist *.aps del *.aps
!if exist *.bak del *.bak
!copy applewin.res .. > NUL:
!del applewin.res
