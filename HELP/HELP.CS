set title=Apple Emulator
set .txt=.hpj
set .hpj=.hlp
set .txt.hpj=easyhelp %file% %title%
set .hpj.hlp=hc31 %file%
!if exist *.bak del *.bak
!copy output.hlp ..\..\applewin.hlp
!del output.*
