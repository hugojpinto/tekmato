# tekmato — TEKtronix MAnipulation TOol

A command-line tool for reading, writing and checking **DOS/50 disc images** of the Tektronix 8550 Microcomputer Development Lab: the 8501 Data Management Unit's 8-inch floppies. All four formats are supported: single- or double-sided, single or double density. The standard disc was double-sided, double density.

It follows [tomato](../../tomato)'s design: open an image, chain commands, save. It doesn't share any of tomato's Timex/TOS code. The DOS/50 file system it implements is documented in [FORMAT.md](FORMAT.md).

> **Work on copies.** Changes stay in memory until `-save` or `-saveas`.

## Build
```
make            # C++17, no dependencies -> ./tekmato
make test       # scripts/regression.sh
```

## Use
```
tekmato -open System.imd -info -list
tekmato -open System.imd -type 8301/COMMANDS/HELP
tekmato -open System.imd -export ./system text          # whole tree, CR -> LF
tekmato -open work.img -write hello.asm SRC/HELLO.ASM text -save
tekmato -new 256 PROJECT -import ./src SRC text -saveas project.img
tekmato -new 64 SMALL sssd -saveas small.imd                 # single-sided, single density
tekmato -open System.imd -check
```
Images can be raw (`.img`, as the MDL emulator uses) or ImageDisk (`.imd`). The size gives the geometry: SSSD 1001, SSDD 1989, DSSD 2002 or DSDD 3991 records of 256 bytes. `-saveas x.imd` writes the 8501's physical track format for the geometry.

| Command | What it does | DOS/50 equivalent |
|---|---|---|
| `-open FILE` | open a raw or IMD image | — |
| `-new [maxfiles] [volume] [sssd\|ssdd\|dssd\|dsdd]` | empty data volume, laid out exactly as DOS/50's FORMAT does (verified for all four; default dsdd) | `FORMAT`, `FORMAT:S` |
| `-save`, `-saveas FILE` | write the image | — |
| `-info` | geometry, label, layout, free space | `LDIR` totals |
| `-list [dir] [flat] [ids]` | tree with size, blocks, links, capabilities, owner, date | `LDIR` |
| `-type PATH` | print a text file | `COPY f CONO` |
| `-read PATH [FILE] [text]` | copy a file out | — |
| `-write FILE [PATH] [text]` | copy a file in (replaces an existing one) | — |
| `-export [DIR] [PATH] [text]` | copy a tree out; DOS/50 links become host hard links | `DUP COPY` |
| `-import DIR [PATH] [text]` | copy a host tree in; host hard links become DOS/50 links | `DUP COPY` |
| `-mkdir PATH…` | create directories | `CREATE` |
| `-delete PATH…` | remove entries, freeing a file with its last link; directories recursively | `DELETE` |
| `-link PATH NEWPATH` | another name for the same file | `FLINK` |
| `-move PATH NEWPATH` | rename or move | `RENAME` |
| `-attrib PATH [cap=OR/OW/PR/PW] [owner=NAME]` | show or change attributes | `ATTRIBUTE` |
| `-owner NAME` | owner of files created afterwards (default `NO.NAME`) | `USER` |
| `-rename VOLUME` | change the volume name | — |
| `-check [fix]` | verify bitmaps, links, extents; list orphans; `fix` frees them | — |

**Paths** are `/`-separated from the volume root. Names keep DOS/50's abbreviation brackets: `8301/COMMANDS/ATT` finds `ATT[RIBUTE]`, and exported files drop the brackets.

**`text`** converts line ends: DOS/50 text files end lines with CR alone.

**Names** are at most 14 characters. Case is kept; `/ [ ] * ?` and spaces are not allowed (on import, spaces become `_`).

**Owners and protection:** DOS/50 enforces capabilities. To write into a directory you don't own, it needs PW. A `-new` volume belongs to the `-owner` user, so DOS/50's default user can use it straight away. The Tektronix distribution discs belong to TEKTRONIX, so give their roots PW (`-attrib / cap=OR/OW/PR/PW`) before writing to them from DOS/50.

**Timestamps** use DOS/50's own calendar (31-day months, see FORMAT.md), so the dates tekmato writes are shown correctly by `ATTRIBUTE`.

## With the MDL emulator
```
tekmato -new 256 WORK -import ./src SRC text -saveas work.img
../MDL/emu/build/tek8550 -rw -0 system.img -1 work.img
> LDIR /VOL/WORK/SRC
```
Not covered: making a disc bootable (copy a system disc and edit it instead), the bad-block map (read only), and decoding object or load-module file formats.
