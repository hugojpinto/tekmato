# tekmato — TEKtronix MAnipulation TOol

A command-line tool for reading, writing and checking Tektronix development-lab disc images:

| Format | Disc | Images |
|---|---|---|
| `dos50-dsdd` (the standard disc), `dos50-dssd`, `dos50-ssdd`, `dos50-sssd` | **DOS/50** on the 8550 MDL's 8501 Data Management Unit: 8-inch, single- or double-sided, single or double density | raw `.img`, ImageDisk `.imd` |
| `tkdos-sssd` | **TEKDOS** on the 8001/8002/8002A µProcessor Lab's Flexible Disc Unit: 8-inch, single-sided, single density, 32 hard sectors | raw `.img` (315,392 bytes) |

`-open` tells them apart by size. tekmato 0.2 called the DOS/50 formats `sssd`, `ssdd`, `dssd` and `dsdd`; `-new` still accepts those.

It follows the design of tomato, the author's Timex TOS/CP/M disc tool: open an image, chain commands, save. It shares none of tomato's code. The file systems are documented in [FORMAT.md](FORMAT.md) (DOS/50) and [FORMAT-TEKDOS.md](FORMAT-TEKDOS.md) (TEKDOS).

> **Work on copies.** Changes stay in memory until `-save` or `-saveas`.

## Download or build
Binaries for macOS (universal), Linux (x86-64) and Windows (x64) are on the [releases page](https://github.com/hugojpinto/tekmato/releases). They are not signed or notarised; QUICKSTART.txt in each archive says how to run them anyway.
```
make                                   # C++17, no dependencies -> ./tekmato
make CROSS=x86_64-w64-mingw32-         # Windows: tekmato.exe (MinGW-w64)
make test                              # scripts/regression.sh
./tekmato -version
```
The regression runs on the disc images in [testdata/](testdata/README.md). If the [8550 emulator](https://github.com/hugojpinto/tektronix-8550-emulator) or the [8002 emulator](https://github.com/hugojpinto/tektronix-8002-emulator) is checked out and built next to tekmato (or `TEK8550_DIR` / `TEK8002_DIR` point at them), it also has DOS/50 and TEKDOS themselves read, write and FORMAT tekmato's volumes.

## Use
```
tekmato -open System.imd -info -list
tekmato -open System.imd -type 8301/COMMANDS/HELP
tekmato -open System.imd -export ./system text          # whole tree, CR -> LF
tekmato -open work.img -write hello.asm SRC/HELLO.ASM text -save
tekmato -new 256 PROJECT -import ./src SRC text -saveas project.img
tekmato -new 64 SMALL dos50-sssd -saveas small.imd           # single-sided, single density
tekmato -open System.imd -check
tekmato -open TekDOS_3.3_6800_cpy1.img -list -type CONCO      # an 8002 TEKDOS disc
tekmato -new 'WORK DISC' tkdos-sssd -write prog.asm 'PROG;S' text -saveas work.img
```
Images can be raw (`.img`, as the 8550 emulator uses) or ImageDisk (`.imd`). The size gives the geometry: SSSD 1001, SSDD 1989, DSSD 2002 or DSDD 3991 records of 256 bytes. `-saveas x.imd` writes the 8501's physical track format for the geometry.

| Command | What it does | DOS/50 equivalent |
|---|---|---|
| `-open FILE` | open a raw or IMD image | — |
| `-new [maxfiles] [volume] [format]` | empty volume, laid out exactly as the system's FORMAT does (verified for all five formats; default `dos50-dsdd`) | `FORMAT`, `FORMAT:S` |
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

**Paths** are `/`-separated from the volume root. Names keep DOS/50's abbreviation brackets: `8301/COMMANDS/ATT` finds `ATT[RIBUTE]`, and exported files keep them too (`-import` brings them back intact).

**`text`** converts line ends: DOS/50 text files end lines with CR alone.

**Names** are at most 14 characters. Case is kept; `/ [ ] * ?` and spaces are not allowed (on import, spaces become `_`).

**Owners and protection:** DOS/50 enforces capabilities. To write into a directory you don't own, it needs PW. A `-new` volume belongs to the `-owner` user, so DOS/50's default user can use it straight away. The Tektronix distribution discs belong to TEKTRONIX, so give their roots PW (`-attrib / cap=OR/OW/PR/PW`) before writing to them from DOS/50.

**Timestamps** use DOS/50's own calendar (31-day months, see FORMAT.md), so the dates tekmato writes are shown correctly by `ATTRIBUTE`.

## TEKDOS discs
TEKDOS has one flat directory of at most 78 files, so `-mkdir`, `-link` and `-attrib` don't apply, and `-move` renames. Names are 1–8 characters from A–Z, 0–9 and `! " # % & ' ( ) * ; = ? @`, starting with a letter. With no name given, `-write` makes one from the host name: upper case, at most 8 characters, `.` as `;` (`hello.asm` → `HELLO;AS`). `-list` shows what TEKDOS's `LDIR /` shows: blocks, sectors and bytes in the last sector, plus the IDENT, LOAD and START of load modules. `-export` writes the system overlay file `.` as `SYSTEM.SOF`, and `-import` puts it back, so an exported system disc imported into a `-new tkdos-sssd` disc boots.

Checked against TEKDOS 3.3 running in the [8002 emulator](https://github.com/hugojpinto/tektronix-8002-emulator):
- `-new tkdos-sssd` equals TEKDOS's own FORMAT byte for byte, and both equal a real formatted disc from the bitsavers collection;
- TEKDOS lists, prints, copies and deletes files tekmato wrote, and tekmato reads TEKDOS's changes back;
- a system disc rebuilt with `-import` boots.
```
tekmato -new WORK tkdos-sssd -write prog.asm 'PROG;S' text -saveas work.img
tek8002 -rw -0 system.img -1 work.img
> LDIR 1
```

## With the [8550 emulator](https://github.com/hugojpinto/tektronix-8550-emulator)
```
tekmato -new 256 WORK -import ./src SRC text -saveas work.img
tek8550 -rw -0 system.img -1 work.img
> LDIR /VOL/WORK/SRC
```
Not covered: making a DOS/50 disc bootable (copy a system disc and edit it instead), the bad-block maps (read only), and decoding object or load-module formats beyond TEKDOS's module header.

## Licence
tekmato is © 2026 Hugo José Pinto, under the GNU General Public License version 2 or later ([LICENSE](LICENSE)). The disc images in `testdata/` are Tektronix material from public archives and are not covered by it: see [NOTICE.md](NOTICE.md). What changed between versions: [CHANGELOG.md](CHANGELOG.md).
