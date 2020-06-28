path=..\..\..\release\;%path%
del *.60hz
del *.psg
del *.sbf

set name=1Antarctic
vgm_psg2psg -ignoreweird %name%.vgm
prepare4sn %name%.vgm_ton00.60hz %name%.vgm_ton01.60hz - - %name%.psg
if not errorlevel 1 goto skip1
pause
:skip1

set name=2CaGames
vgm_psg2psg -ignoreweird %name%.vgm
prepare4sn %name%.vgm_ton00.60hz %name%.vgm_ton01.60hz %name%.vgm_ton02.60hz %name%.vgm_noi03.60hz %name%.psg
if not errorlevel 1 goto skip2
pause
:skip2

set name=3DDragon
vgm_psg2psg -ignoreweird %name%.vgm
prepare4sn %name%.vgm_ton00.60hz %name%.vgm_ton01.60hz %name%.vgm_ton02.60hz %name%.vgm_noi03.60hz %name%.psg
if not errorlevel 1 goto skip3
pause
:skip3

set name=4Beatit
vgm_psg2psg -ignoreweird %name%.vgm
prepare4sn %name%.vgm_ton00.60hz %name%.vgm_ton01.60hz %name%.vgm_ton02.60hz %name%.vgm_noi03.60hz %name%.psg
if not errorlevel 1 goto skip4
pause
:skip4

set name=5Alf
vgm_psg2psg -ignoreweird %name%.vgm
prepare4sn %name%.vgm_ton00.60hz %name%.vgm_ton01.60hz %name%.vgm_ton02.60hz %name%.vgm_noi03.60hz %name%.psg
if not errorlevel 1 goto skip5
pause
:skip5

set name=6Collect
vgm_psg2psg -ignoreweird %name%.vgm
prepare4sn - - %name%.vgm_ton00.60hz - %name%.psg
if not errorlevel 1 goto skip6
pause
:skip6

set name=7Fall
vgm_psg2psg -ignoreweird %name%.vgm
prepare4sn - - %name%.vgm_ton00.60hz - %name%.psg
if not errorlevel 1 goto skip7
pause
:skip7

set name=8Jump
vgm_psg2psg -ignoreweird %name%.vgm
prepare4sn - - %name%.vgm_ton00.60hz - %name%.psg
if not errorlevel 1 goto skip8
pause
:skip8

vgmcomp2 -v -sn 1Antarctic.psg 2CaGames.psg 3DDragon.psg 4Beatit.psg 5alf.psg 6collect.psg 7fall.psg 8jump.psg demo.sbf
if not errorlevel 1 goto skip9
pause
:skip9






