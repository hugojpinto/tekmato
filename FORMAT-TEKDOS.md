# The TEKDOS disc format

This is the file system of the Tektronix 8001/8002/8002A µProcessor Lab: TEKDOS (versions 3.1 and 3.3 seen) on the Flexible Disc Unit's 8-inch, single-sided, single-density, **hard-sectored** floppies. tekmato calls it **`tkdos-sssd`**. The DOS/50 format of the 8550 is unrelated; see [FORMAT.md](FORMAT.md).

Sources, as cited below:
- the Flexible Disc Unit service manual 070-2587 (Table 1-1 "Disc File Structure"), in the [8002 emulator](https://github.com/hugojpinto/tektronix-8002-emulator)'s `sources/bitsavers/manuals/800x/`;
- the 33 disc images on bitsavers (`bits/Tektronix/8002/`, copied to [testdata/tekdos/](testdata/tekdos/)), several photographed with a printout of their directory;
- TEKDOS 3.3 itself, running in the [8002 emulator](https://github.com/hugojpinto/tektronix-8002-emulator): its FORMAT, COPY, DELETE and LDIR, with the image diffed before and after.

## Geometry
77 tracks × 32 sectors × 128 bytes, FM. A raw image holds the sectors in **logical** order, track by track: 315,392 bytes, and that size identifies the format. The disc is physically interleaved (sectors written 0, 16, 1, 17 …, 070-2587 "Sector Interleaving"), but an image does not show this. ImageDisk cannot represent hard sectors, so tekmato writes TEKDOS discs only as raw images.

## Layout (070-2587 Table 1-1)
| Where | Contents |
|---|---|
| track 0, sector 0 | disc header |
| track 0, sectors 1–5 | directory: 78 names of 8 bytes |
| track 0, sectors 6–31 | file block maps: 78 records of 41 bytes, 3 per sector (the last 5 bytes of each sector unused) |
| tracks 1–4 | TEKDOS itself (the bootstrap PROM loads it from here) |
| tracks 5–76 | files |

The allocation unit is a **block of 8 sectors (1024 bytes)**. The 304 blocks cover tracks 1–76: block *b* is track 1 + *b*/4, sectors (*b* mod 4)·8 to +7. The system area, tracks 1–4, is blocks 0–15. It is the data of the file TEKDOS, so a disc whose TEKDOS was deleted can use it for files (users manual 070-2701, FORMAT).

## Disc header (track 0 sector 0)
| Bytes | Contents | Evidence |
|---|---|---|
| 0–1 | "millisecond identification" (Table 1-1); 0000 on every disc seen | collection |
| 2 | number of files (TEKDOS prints it as LDIR's TOTAL FILES) | DELETE lowers it by one; FORMAT sets it to 1 |
| 3–51 | disc identifier: FORMAT's name, ending in CR (48 characters at most, users manual) | collection, FORMAT |
| 52–89 | bad-block map: 304 bits | cpy1 marks 8 blocks; LDIR reports 8 bad |
| 90–127 | master map: 304 bits, 1 = block in use (by a file or bad) | see below |

## Directory (sectors 1–5)
Slot *n* is 8 bytes at sector 1 offset 8*n*. It holds the name, padded with blanks. An all-zero slot is free: **DELETE zeroes the name** (observed in the emulator). The directory is not sorted: LDIR lists the slots in order, and TEKDOS's COPY put a new file in the first free slot (observed). The system overlay file is named with the single byte **0F**, and LDIR shows it as `.`.

Names (users manual 070-2701 §2, reference booklet 070-2706): 1–8 characters from A–Z, 0–9 and `! " # % & ' ( ) * ; = ? @`, starting with a letter. Device names (CONI, CONO, LPT1, PPTP, REMI, REMO, …) are reserved. Suffixes are a convention, not a structure: `;S` temporary source, `%S` permanent source, `O` object, `L` load, and so on (booklet Table 1-2).

LDIR lists every named slot. On disc 8, byte 2 says 15 but 22 slots are named, and LDIR lists all 22 (its TOTAL FILES line says 15). So byte 2 is a count that DOS keeps, not a limit.

## File records (sectors 6–31)
Record *n* belongs to slot *n*: sector 6 + *n*/3, offset 41·(*n* mod 3).

| Bytes | Contents |
|---|---|
| 0–37 | block map: 304 bits |
| 38–39 | number of sectors used, **big-endian** |
| 40 | data bytes in the last sector (1–128; 0 for an empty file) |

**Bit order: LSB first** (bit *b* is byte *b*/8, bit *b* mod 8). A file's data is its blocks in **ascending** order, sectors in order within a block, cut to (sectors − 1)·128 + last bytes. Evidence: the source file EMAIN%S on Tek8002_1a reads as continuous assembler text only this way (MSB-first gives text in the wrong order). TEKDOS's `COPY EDEF%S/1 CONO` prints what tekmato reads.

Consistency, checked on all 33 discs:
- the file maps never overlap;
- every file has ⌈sectors/8⌉ blocks;
- the master map is the union of the file maps and the bad-block map;
- the sector and byte counts match the printouts photographed with the discs (Tek8002_1a: all 47 files).

Exceptions: 4 discs are inconsistent as imaged, and `-check` names them: 18 (stale file count: LDIR lists its 50 files but says 34), 24a (two orphan blocks), 7a (a short map), 8 (two blocks free in the master map but used).

An empty file has no blocks and 0 / 0 counts (CHANGE on the TEKDOS 3.3 disc).

**DELETE** (observed): it zeroes the name and the whole 41-byte record, clears the file's blocks in the master map, and decrements byte 2. tekmato's `-delete` does the same.

## Load modules
A file made by TEKDOS's MODULE command starts with a header in its first sector:

| Bytes | Contents |
|---|---|
| 0–2 | `MOD` |
| 3 | 03 on every module seen |
| 4–5, 6–7 | load address range, low and high, big-endian |
| 8–9 | start address, big-endian |
| 10– | IDENT text, ending in CR (FF then CR when there is none) |

`LDIR /` prints these as IDENT, LOAD and START (the printout photographed with Tek8002_1a). The TEKDOS file itself is such a module: `TEKDOS 3.1 5/10/79`, 80–3FFF, start 100.

## FORMAT
TEKDOS's `FORMAT 1 NAME` writes every sector (2,464 write-unformatted commands in the emulator) and leaves:
- all data zero;
- the header with byte 2 = 1, the name and CR, and blocks 0–15 set in the master map;
- slot 0 `TEKDOS  ` with blocks 0–15, 128 sectors and 128 bytes in the last.

This matches the real disc Tek8002_1b, labelled FORMATTED, byte for byte, and `-new tkdos-sssd` makes the same disc.

## What tekmato chooses (TEKDOS's own behaviour unknown)
- **Allocation:** new files take the lowest free blocks. TEKDOS reads a file's blocks in ascending order, so any choice works. Replacing TEKDOS (on `-import`) therefore puts it back in blocks 0–15, where the bootstrap PROM needs it. A disc rebuilt with `-new … -import` of an exported system disc boots in the emulator.
- **New slots:** the first free one.
- Byte 2 is set to the number of named slots after every change.

## Open questions
- Bytes 0–1 of the header ("millisecond identification").
- Byte 3 of the module header.
