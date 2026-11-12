# tekmato

tekmato is a command-line tool for Tektronix development-lab disc images: DOS/50 (the 8550 MDL's 8501 DMU; formats `dos50-sssd/ssdd/dssd/dsdd`) and TEKDOS (the 8001/8002/8002A µProcessor Lab's Flexible Disc Unit; format `tkdos-sssd`).
- **Version:** `TEKMATO_VERSION` in `command.cpp`.
- **Siblings:** the 8550 emulator in `../MDL` and the 8002A emulator in `../tektronix-8002-emulator`; their disc images are this tool's test corpus, and their emulators check what tekmato writes.
- **Model:** tekmato reuses the ideas, syntax and command structure of tomato (`../../tomato`, a Timex TOS/CP/M tool) but none of its code. It does not support TOS or CP/M, and it should stay that way.

## Layout
| File | What |
|---|---|
| `dos50_volume.{hpp,cpp}` | the DOS/50 file system: image I/O (raw and IMD), label, bitmaps, descriptors, directories, allocation, check |
| `tekdos_volume.{hpp,cpp}` | the TEKDOS file system: header, directory, block maps, module headers, allocation, check |
| `command.{hpp,cpp}` | base `command` class (shared open volume — `vol` for DOS/50, `tvol` for TEKDOS, `tk` says which — helpers), one `DECLARE_COMMAND` per CLI command, version. Each command handles TEKDOS at the top of `process`; DOS/50-only ones call `dos50_only()` |
| `command_image.cpp` | `-open -new -save -saveas -info -rename -owner` |
| `command_list.cpp` | `-list` |
| `command_files.cpp` | `-read -type -write -export -import` |
| `command_tree.cpp` | `-delete -mkdir -link -move -attrib -check` |
| `tekmato.cpp` | `cmd_map` dispatcher, help (as tomato: `-cmd args… -cmd args…`, prints `C : name`, errors `E :`, info `I :`) |
| `FORMAT.md`, `FORMAT-TEKDOS.md` | **the on-disk formats and the evidence for each fact.** Update them whenever a field is newly understood |
| `scripts/regression.sh` | 95 checks (section 6: TEKDOS). Run it after any change (`make test`) |

Adding a command means:
1. adding `DECLARE_COMMAND(x)` in `command.hpp`;
2. implementing it in the matching `command_*.cpp`;
3. adding an entry in `cmd_map`;
4. adding a check in `regression.sh`.

## Rules
- **Fidelity:** DOS/50 and TEKDOS must accept everything tekmato writes. The regression runs DOS/50 in `../MDL/emu/build/tek8550` and TEKDOS in `../tektronix-8002-emulator/emu/build/tek8002` against tekmato-made volumes; keep both passing.
- **Unknown bytes:** when a descriptor or directory changes, change only the known fields. Their meanings came from diffing images before and after a DOS/50 command (FORMAT.md, "Evidence").
- **Originals are read only:** never write into `../MDL/sources/` or `../tektronix-8002-emulator/sources/`. Tests work on copies in a temp directory.
- **Learning a new detail:** script a DOS/50 or TEKDOS session in its emulator (`-rw` on a copy, `-type T 'CMD\r'`), diff the image before and after, and record the finding in FORMAT.md or FORMAT-TEKDOS.md.

## Open questions
- What ALLOCATION=SCAT/CONT is stored as (`ATT … ALLOC=CONT` changed nothing on a 1-block file).
- What the label's byte 0x1C means (`C`, `2`, `3` or 0 on different discs).
- The bad-block map is assumed to be bit-per-block like the block bitmap, but no disc seen has a bad block.
- (Resolved) FORMAT and FORMAT:S work in the emulator for all four geometries; `-new` matches them, and the regression compares them.
- No real SS or SD disc image has been seen: those geometries rest on the service manual plus DOS/50's FORMAT in the emulator.
