# Toolchain reference and gotchas

All tools are in the vgmcomp2 `Release` folder (Windows exes). Run any tool without
arguments for its usage. Channel files are `<prefix>_tonNN.60hz` / `_noiNN.60hz`: one row
per 1/60s, `0x<freq>,0x<vol>`; noise flags in the upper bits (0x10000 retrigger, 0x100000
periodic). All channel files of a song must have the same row count.

## Importers
| Tool | Notes |
|---|---|
| mod2psg [opts] file | -forcenoise/-forcetone (one instrument per flag!), -forcefreq ins rate, -tuneins ins f, -volins ins f, -vol f (pins auto scale), -speedscale, -d debug |
| midi2psg.py [opts] file.mid | -poly n, -partpoly track n, -bendrange n; speed/decay scale positional |
| vgm_psg2psg / vgm_ay2psg / vgm_md2psg / vgm_nes2psg / vgm_gb2psg / vgm_pokey2psg | -add n to offset channel numbers when combining chips |
| sid2psg -len s [-subtune n] file.sid | length mandatory |
| voice2psg [-channels n] file.wav | basic; the skill's voice2psg_proto.py is better |
| nsf2vgm.py (this skill) | NSF -> VGM for vgm_nes2psg |

## Editing tools (most rename the input to an .xxx.old backup)
| Tool | What it does | Gotcha |
|---|---|---|
| scalepitch f ch | counter x f (0.5 octave up) | rests (freq 0) become 1 - do mutetones before it |
| scalevolume f ch / -maximize | volume x f | clips at 255 |
| clipmaxvol max ch | caps volume | |
| smoothvolume [-hermite\|-cubic] ch | 3-row average | gives rests volume - follow with mutetones -SN |
| mutetones -SN\|-AY\|-SID ch | mutes out-of-range and freq-0 rows | |
| cliptones -SN ch | clamps range | changes the note - prefer foldoctave.py |
| despeckle ch | mutes lone rows, fills single-row gaps | gaps only filled if same data both sides |
| cleardupes a b | mutes b where equal to a | renames BOTH inputs to .dupes.old |
| underlaytones a b | a fills b's silences (b priority), result in b | |
| arptones / arp3tones | merge 2/3 channels as arpeggio | result in the first file |
| mergenoise a b | louder wins, result in a | won't overwrite an existing .merge.old |
| fixsnnoise ch | noise to fixed SN rates | lossy |
| forcenoisetype -white\|-periodic ch | | per channel, not per instrument |
| bass2noise / noise2bass | bass <-> periodic SN noise | |
| trimsilence ch / padsilence | whole song | |
| changespeed ch hz | resample whole song | |
| reducetonecnt / reducevolumecnt | fit 256 note limit / compress better | for vgmcomp2 packing |

Backup collisions: most editing tools refuse to overwrite an existing backup (error code 17).
When a tool runs twice on one file, rename the previous backup first, e.g. in a .cmd:
`move /y x.60hz.merge.old x.60hz.merge1.old >nul`. Start each build by deleting all
generated `*.60hz*` files so stale backups never block a run.

## Prepare, play, pack
- `prepare4SN t1 t2 t3 noise out` / `prepare4AY ...` / `prepare4SID v1 v2 v3 out` - any input
  may be `-`. Output length = shortest input.
- `testPlayer [-sn|-ay] <prefix | prepared file | files...>` - plays channel files by prefix
  (e.g. `SONG.MOD`) or a prepared file; -sn/-ay enforce chip limits. Windows only.
- `psg2vgm` - back to VGM for listening elsewhere.
- `vgmcomp2 -sn|-ay|-sid in.psg ... out.sbf` - compression (the final step, often not
  needed while arranging). `bestPacker` tries song combinations.

## Build script template (Windows .cmd)

```bat
@echo off
rem Convert SONG.MOD to SN (song.snpsg). Paths are relative to this folder.
set R=<path to vgmcomp2>\Release
set S=<path to this skill>\scripts
set N=SONG.MOD
del /q %N%_*.60hz* 2>nul
rem why: ...
%R%\mod2psg -forcenoise 2 -forcefreq 2 64 -tuneins 4 0.993 %N%
python %S%\foldoctave.py 0x3ff %N%_ton02.60hz
%R%\smoothvolume %N%_ton02.60hz
%R%\mutetones -SN %N%_ton02.60hz
call :dupes 00 01 1
%R%\prepare4SN %N%_ton00.60hz %N%_ton01.60hz %N%_ton02.60hz %N%_noi04.60hz song.snpsg
goto :eof

:dupes
%R%\cleardupes %N%_ton%1.60hz %N%_ton%2.60hz
move /y %N%_ton%1.60hz.dupes.old %N%_ton%1.60hz.dupes%3.old >nul
move /y %N%_ton%2.60hz.dupes.old %N%_ton%2.60hz.dupes%3.old >nul
goto :eof
```
Use CRLF line endings for .cmd files. `%%` must be doubled inside .cmd for literal percent.
