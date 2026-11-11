# The DOS/50 disc format

This is the file system of the Tektronix 8550 MDL: DOS/50 V1.0 and later, on the 8501 Data Management Unit's double-sided, double-density 8-inch floppies. No Tektronix document describing it has been found, so everything below was worked out from the disc images in `../MDL/sources/bitsavers/disks/`. It was then confirmed by running DOS/50 in the MDL emulator and diffing the image before and after a command. Each fact gives its evidence.

All multi-byte values are little-endian PDP-11 words. 32-bit values store the **high word first** (PDP-11 order).

## Records, geometry and blocks

The controller addresses 256-byte records in physical order (cylinder, then side). The 8501 takes all four IBM-compatible 8" formats (service manual 061-2355-00, controller chapter). Each has 77 cylinders and 26 sectors per track:

- **single density (FM):** 128-byte sectors, so 13 records per track;
- **double density (MFM):** 256-byte sectors, so 26 records per track, except track 0 (cylinder 0 side 0), which is always FM.

| Geometry | Records | Last block | Image size |
|---|---|---|---|
| SSSD | 1001 | 499 | 256 256 |
| SSDD | 1989 | 993 | 509 184 |
| DSSD | 2002 | 1000 | 512 512 |
| DSDD (the 8501's standard disc) | 3991 | 1994 | 1 021 696 |

A raw image (`.img`) is the records back to back, so its size identifies the geometry. An ImageDisk (`.imd`) image decodes to the same bytes.

The file system works in **blocks of 512 bytes, which are 2 records**: block *n* is records 2*n* and 2*n*+1. The label stores the **last** block number, which is records/2 − 1. On DSDD that is 0x7CA = 1994, leaving record 3990 unused.

Every disc in the collection is DSDD. The other three geometries were established by running DOS/50's FORMAT (DD) and `FORMAT:S` (single density) on one- and two-sided blank images in the emulator.

## Layout

| Blocks | Contents |
|---|---|
| 0–4 | boot code. The 8501's ROM boots record 0, which must start with 000240. These blocks are the data of file `8501_BOOTSTRAP` |
| 5 | **volume label** (record 10) |
| 6 … 6+*maxfiles*−1 | **file descriptors**, one block each; file id *N* is block 6+*N* |
| next | **block bitmap**: bit *n* = block *n*, **1 = free**, LSB first |
| next | bad-block map (same layout; all zero on every disc seen) |
| next | **descriptor bitmap**: indexed by block number, so file id *N* is bit 6+*N*; **1 = free** |
| next | first block of the root directory |
| the rest | file data |

*maxfiles* is FORMAT's parameter. It is 500 on the SYSTEM disc and 256 on the distribution discs, so the bitmaps sit at blocks 506–509 or 262–265. Always take their positions from the label.

Evidence:
- **Counts:** the bitmap bit counts equal LDIR's "Free blocks" and "Free files" (245 and 276 on SYSTEM).
- **Allocation:** creating one file cleared exactly one bit in each bitmap.

## Volume label (block 5)

| Offset | Size | Contents |
|---|---|---|
| 00 | 16 | `TEKTRONIX DOS/50` |
| 14 | 15 | volume name: up to 14 characters, NUL-terminated (`SYSTEM`, `EMU.8085`, `PP`, `8550DIAG3.0`). Bytes after the NUL are leftover buffer contents |
| 24 | 2 | 0x0101 |
| 26 | 4 | time of FORMAT (DOS/50 time, see below) |
| 2A | 2 | block bitmap block |
| 2C | 2 | bad-block map block |
| 2E | 2 | descriptor bitmap block |
| 30 | 2 | root directory block |
| 32 | 2 | **last** block number = records/2 − 1 (DSDD 0x7CA = 1994). FORMAT marks it free, and LDIR counts it |
| 34 | 2 | *maxfiles* − 1 |

The rest of the block, from 0x36 up, holds leftover buffer contents: it differs from disc to disc with no pattern. So do 10–13, 23 and whatever follows the name's NUL. The 8501's volume names appear as `/VOL/<name>`.

**Volume name length** (settled 2026-11-11; this table used to say 8 bytes, leaving byte 1C "unknown"):
- **FORMAT:** `FORMAT:Z:N /DEV/FLX1 100 ABCDEFGHIJK`, run in the emulator on an image filled with AA, writes `ABCDEFGHIJK` at 14–1E and 00 at 1F. Bytes 10–13, 20–23 and 36 up hold buffer leftovers, not AA. A 20-character name is cut to 14 (`ABCDEFGHIJKLMN`, 00 at 22).
- **Lookup:** DOS/50 then lists `/VOL/ABCDEFGHIJK`, but `/VOL/ABCDEFGH` gives "Error 1D File not found". The whole name counts.
- **The diagnostic discs:** their names are `8550DIAG3.0` and `8550DIAG2.4`. 3.0's own resident opens `/VOL/8550DIAG3.0/8085.TST`, so byte 1C (`3`, `2`) is the name's ninth character.
- **SYSTEM:** its byte 1C (`C`), after `SYSTEM` and its NUL, is a leftover. So is the `5` at 20 on `EMU.8085` and `EMU.8086`, whose names fill 14–1B and end with the NUL at 1C.
- **The PROM programmer disc** reads `..85PP.SAV` from 10. Its name is `PP` (14–15, NUL at 16); the `85` at 12–13 and the `SAV` after the NUL are leftovers.
- **tekmato** reads and writes the whole name since 0.2.2 (before, 8 bytes: it showed the diagnostic discs as `8550DIAG`).

## File descriptor (512 bytes)

| Offset | Size | Contents |
|---|---|---|
| 000 | 4 × *n* | **extents**: (first block, number of blocks) word pairs. A pair with count 0 ends the list, giving at most 108 extents |
| 1B4 | 4 | CREATION time |
| 1B8 | 4 | UPDATE time |
| 1BC | 4 | ACCESS time |
| 1C0 | 4 | SIZE in bytes |
| 1C4 | 2 | CONSUMPTION, the number of blocks allocated |
| 1C6 | 1 | CAPABILITIES: 01 OR, 02 OW, 10 PR, 20 PW (owner/public read/write) |
| 1C7 | 1 | 1 for a live file, 0 after deletion |
| 1C8 | 16 | OWNER, zero-padded (`TEKTRONIX`, `NO.NAME`, `STEVE`…) |
| 1F8 | 1 | LINK count, the number of directory entries naming the file |
| 1F9 | 1 | TYPE: 1 directory, 0 file |
| 1FC | 2 | FFFF |

How each field was identified:
- **ATTRIBUTE output:** every field except FFFF and the live flag was matched against what ATTRIBUTE displays for the same file.
- **Capabilities:** `ATT f CAP=PW` changed 1C6 from 13 to 20. DOS/50 rejects OE and PE.
- **Link count:** `FLINK` raised 1F8 from 1 to 2.
- **Unused bytes:** bytes outside these fields are stale buffer contents.

Root (id 0) and `8501_BOOTSTRAP` (id 1) are created by FORMAT, both owned by the user who runs it:
- the root has one block;
- `8501_BOOTSTRAP` has extent (0, 5), size 0 on a data disc and 2048 on a system disc.

## Directories

A directory's data is an array of 16-byte entries: a file id (2 bytes) followed by a name (14 bytes, zero-padded). SIZE counts the bytes up to the last live entry.

- **CREATE** makes an empty directory: size 0, no blocks.
- **Adding an entry** reuses the first zeroed slot, or appends and allocates a block when the directory grows past its blocks.
- **DELETE** zeroes the entry in place, then trims trailing free entries off SIZE and frees blocks that are no longer needed. An emptied directory gives its block back.
- **Last link removed:** the file's extents, consumption, live flag and both bitmap bits are cleared.

**Hard links** are normal: SYSTEM's command directories (`COMMANDS`, `NO.EMULATOR`, `Z80`, `8085`…) share their command files, so `ATTRIBUTE` has LINK=7.

**Names** may carry an abbreviation, as in `ATT[RIBUTE]`, `L[DIR]`, `COP[Y]` and `DS[]` (no abbreviation). The part before `[` is the shortest form accepted, and DOS/50's **command lookup** matches any prefix of the full name at least that long. A file path does not: `COPY /VOL/X/BR[ACKET] CONO` finds the file, `COPY /VOL/X/BRAC CONO` and `.../BRACKET` give "Error 1D File not found" (checked in the emulator, 2026-11-11). tekmato's own lookups accept the abbreviations too, as a convenience. Lower case is legal (`hugo` on SYSTEM).

## Time

DOS/50 keeps local time as **seconds since 1-JAN-70 00:00, on a calendar of twelve 31-day months** (372-day years). So:

```
t = ((year-1970)*372 + (month-1)*31 + (day-1)) * 86400 + h*3600 + m*60 + s
```

Evidence: raw descriptor times decoded as Unix time are wrong by a growing number of days, but the time of day is always exact. `ATT 8501.SAV *` shows CREATION=02-OCT-80/08:52:43 for t = 345631963 (day 4000), where 4000 = 10·372 + 9·31 + 1. Two more points fit exactly: day 4468 = 05-JAN-82, and a file tekmato stamped 11-NOV-26 shows as 11-NOV-26. The emulator's clock starts at 0 (01-JAN-70) until `DATE` is set.

## Protection

DOS/50 enforces the capabilities. A user who doesn't own a directory needs PW to write into it. The distribution discs' roots belong to TEKTRONIX with OR/OW/PR, so writing into them as the default user `NO.NAME` fails with **"Error 12 Device I/O error on write"**, which is V1.0's message for a protection violation. Subdirectories owned by NO.NAME work, and so does a root after `ATT … CAP=OR/OW/PR/PW`.

## Leftovers seen on real discs

- **Orphans.** SYSTEM has 88 descriptors marked in use that no directory names. Their link count is 0, their extent lists are stale, and they hold 95 blocks. DOS/50 counts them in "Files used". They are probably files deleted while still open. `tekmato -check fix` frees them, and only the blocks no live file uses.
- **Leaked blocks.** 8550_DIAG.IMD (diagnostics 1984) has 24 blocks marked in use that no file owns.

## FORMAT
Watched in the emulator on blank one- and two-sided images (`FORMAT[:S]:N /DEV/FLX1 n NAME`):
1. FORMAT reads the old label (record 10) several times.
2. It issues one format command per track: 047 for double density, 051 for `:S`. FDDAR = 0, 1, 2… with no upper limit, and it stops when the controller refuses the first track past the end (77 one-sided, 154 two-sided).
3. It asks the status command for the capacity, and sizes the volume from it.
4. It writes the label, all *maxfiles* descriptor blocks (unused ones carry stale buffer contents), the three maps and the root.

`FORMAT:Z` skips step 2.

For all four geometries the result agrees with tekmato `-new` in every field above, and the bitmaps are byte-identical. `scripts/regression.sh` checks this, and also has DOS/50 read and write a tekmato-made `.imd` of each geometry.
