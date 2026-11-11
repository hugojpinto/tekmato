# tekmato

tekmato is a command-line tool for DOS/50 disc images, the file system of the Tektronix 8550 MDL's 8501 DMU.
- **Version:** `TEKMATO_VERSION` in `command.cpp`.
- **Sibling:** the emulator lives in `../MDL`, whose disc images are this tool's test corpus.
- **Model:** tekmato reuses the ideas, syntax and command structure of tomato (`../../tomato`, a Timex TOS/CP/M tool) but none of its code. It does not support TOS or CP/M, and it should stay that way.

## Layout
| File | What |
|---|---|
| `dos50_volume.{hpp,cpp}` | the file system: image I/O (raw and IMD), label, bitmaps, descriptors, directories, allocation, check |
| `command.{hpp,cpp}` | base `command` class (shared open volume, helpers), one `DECLARE_COMMAND` per CLI command, version |
| `command_image.cpp` | `-open -new -save -saveas -info -rename -owner` |
| `command_list.cpp` | `-list` |
| `command_files.cpp` | `-read -type -write -export -import` |
| `command_tree.cpp` | `-delete -mkdir -link -move -attrib -check` |
| `tekmato.cpp` | `cmd_map` dispatcher, help (as tomato: `-cmd args… -cmd args…`, prints `C : name`, errors `E :`, info `I :`) |
| `FORMAT.md` | **the on-disk format and the evidence for each fact.** Update it whenever a field is newly understood |
| `scripts/regression.sh` | 40 checks. Run it after any change (`make test`) |

Adding a command means:
1. adding `DECLARE_COMMAND(x)` in `command.hpp`;
2. implementing it in the matching `command_*.cpp`;
3. adding an entry in `cmd_map`;
4. adding a check in `regression.sh`.

## Rules
- **Fidelity:** DOS/50 must accept everything tekmato writes. The regression's last section runs DOS/50 in `../MDL/emu/build/tek8550` against a tekmato-made volume; keep it passing.
- **Unknown bytes:** when a descriptor or directory changes, change only the known fields. Their meanings came from diffing images before and after a DOS/50 command (FORMAT.md, "Evidence").
- **Originals are read only:** never write into `../MDL/sources/` (those files are read-only anyway). Tests work on copies in a temp directory.
- **Learning a new detail:** script a DOS/50 session in the emulator (`-rw` on a copy, `-type T 'CMD\r'`), diff the image records, and record the finding in FORMAT.md.

## Open questions
- What ALLOCATION=SCAT/CONT is stored as (`ATT … ALLOC=CONT` changed nothing on a 1-block file).
- What the label's byte 0x1C means (`C`, `2`, `3` or 0 on different discs).
- The bad-block map is assumed to be bit-per-block like the block bitmap, but no disc seen has a bad block.
- `FORMAT` hangs in the emulator after reading the blank disc's label, so `-new` is modelled on the distribution discs and verified by DOS/50 reading and writing the result.
