# Changelog

What changed, newest first.

- 2026-11-12: **Version 0.4.0**, the first public release.
  - **Builds on Windows.** Host files go through `std::filesystem`: `-export` makes NTFS hard links where the system has them (a copy elsewhere, as on FAT), and `-import` recognises host hard links by identity. Disc names with characters Windows forbids in a file name (`"*:<>?\|`, a trailing `.` or space; one TEKDOS disc has a file `O""""""O`) are exported as U+F000 + the character, as Cygwin and WSL do, and imported back on any system.
  - **Versioning:** the `VERSION` file is compiled in (`-version` prints `tekmato 0.4.0`). A tag `vX.Y.Z` equal to it builds the binary releases on GitHub (macOS universal, Linux x86-64, Windows x64).
  - **Self-contained tests:** the regression reads the disc images in `testdata/` (68 checks with no emulator). With the 8550 and 8002 emulators built next door, DOS/50 and TEKDOS themselves check tekmato's volumes (98 checks). New check: a DOS/50 file with 7 links exports as 7 host hard links.
  - Licensed GPL-2.0-or-later (`LICENSE`, `NOTICE.md`, a header in each file).
  - README: exported files keep DOS/50's abbreviation brackets (it said they dropped them).
- 2026-11-12: one `filesystem` interface and a format registry; commands no longer depend on the format. The regression types at TEKDOS's prompt and FORMATs a never-formatted disc.
- 2026-11-12: **0.3.0**: TEKDOS discs of the 8001/8002/8002A (`tkdos-sssd`), checked against TEKDOS 3.3 in the 8002 emulator (FORMAT, COPY, DELETE, LDIR, a rebuilt system disc boots). The DOS/50 formats are named `dos50-*` (the old names are still accepted).
- 2026-11-11: **0.2.2**: volume names up to 14 characters.
- 2026-11-11: **0.2.1**: names with DOS/50's abbreviation brackets (`ATT[RIBUTE]`, `DS[]`).
- 2026-11-11: **0.2.0**: all four 8501 disc geometries (SSSD, SSDD, DSSD, DSDD), raw and ImageDisk; `-new` lays a volume out exactly as DOS/50's FORMAT does.
- 2026-11-11: **0.1.0**: DOS/50 disc image tool for the Tektronix 8550 MDL: list, read, write, export, import, links, attributes, check.
