#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# tekmato regression: invariants on the DOS/50 and TEKDOS disc collections in testdata/,
# round trips, and (if the emulators are checked out and built next door) DOS/50 and TEKDOS themselves reading
# and writing volumes made by tekmato. Usage: scripts/regression.sh   (from the tekmato dir)
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
T="$HERE/tekmato"
DISKS="$HERE/testdata/dos50"
TK="$HERE/testdata/tekdos"
# optional: the emulators, for the DOS/50 and TEKDOS cross-checks (sections 5 and 6)
MDL="${TEK8550_DIR:-$HERE/../tektronix-8550-emulator}"
T8002="${TEK8002_DIR:-$HERE/../tektronix-8002-emulator}"
W="$(mktemp -d)"
trap 'rm -rf "$W"' EXIT
pass=0; fail=0
ok()   { pass=$((pass+1)); }
bad()  { fail=$((fail+1)); echo "FAIL: $*"; }
expect() {  # expect "description" "needle" command...
	local what="$1" needle="$2"; shift 2
	local out; out="$("$@" 2>&1)"
	if grep -qF -- "$needle" <<<"$out"; then ok; else bad "$what (wanted '$needle')"; echo "$out" | tail -5; fi
}

[ -x "$T" ] || { echo "build tekmato first (make)"; exit 2; }

# ---- 1. reading the collection (originals are never written)
expect "System label"        "Volume         SYSTEM"   "$T" -open "$DISKS/System.imd" -info
expect "System free blocks"  "Free blocks    245"      "$T" -open "$DISKS/System_Fixed.img" -info
expect "System files used"   "Files used     223"      "$T" -open "$DISKS/System_Fixed.img" -info   # = LDIR
expect "System orphans"      "88 orphan files"         "$T" -open "$DISKS/System.imd" -check
expect "8085 disc clean"     "volume is consistent"    "$T" -open "$DISKS/062-4641-03_8300E06_8085_Emulator_V2.1_DOS50.imd" -check
expect "PROM disc clean"     "volume is consistent"    "$T" -open "$DISKS/062-6036-01_8550F30_prom_programmer_2.1_1982.imd" -check
expect "8086 disc clean"     "volume is consistent"    "$T" -open "$DISKS/8550_8086.IMD" -check
expect "DIAG 2.4 clean"      "volume is consistent"    "$T" -open "$DISKS/062-5412-08_8501_1err_DOS50.imd" -check
expect "DIAG 3.0 name"       "Volume         8550DIAG3.0" "$T" -open "$DISKS/8550_DIAG.IMD" -info
expect "14-char volume name" "Volume         ABCDEFGHIJKLMN" "$T" -new 64 ABCDEFGHIJKLMN -info
expect "DIAG lost blocks"    "24 blocks marked in use" "$T" -open "$DISKS/8550_DIAG.IMD" -check
expect "DOS/50 calendar"     "CREATION=02-OCT-80/08:52:43" "$T" -open "$DISKS/System.imd" -attrib 8501.SAV
expect "abbreviated name"    "ATT[RIBUTE]"             "$T" -open "$DISKS/System.imd" -list 8301/COMMANDS flat
expect "abbreviation lookup" "Alters or displays file attributes" "$T" -open "$DISKS/System_Fixed.img" -type 8301/COMMANDS/HELP
expect "hard links counted"  "LINK=7"                  "$T" -open "$DISKS/System.imd" -attrib 8301/COMMANDS/ATT
# abbreviation brackets in new names: one trailing pair, as DOS/50's command files have
expect "bracket name written" "DS[]"                   "$T" -new 64 B -write "$HERE/README.md" 'DS[]' -write "$HERE/README.md" 'HE[LLO]' -list
expect "bracket name lookup"  "HELL ->"                "$T" -new 64 B -write "$HERE/README.md" 'HE[LLO]' -read HELL /dev/null
expect "bad bracket refused"  "invalid DOS/50 name"    "$T" -new 64 B -write "$HERE/README.md" 'A[B]C'
expect "8086 overlays export" "BK[]"                   bash -c "'$T' -open '$DISKS/8550_8086.IMD' -export '$W/x86' >/dev/null 2>&1; ls '$W/x86/VERSION2'"

# ---- 2. image round trips
"$T" -open "$DISKS/System.imd" -saveas "$W/s.img" -saveas "$W/s.imd" >/dev/null 2>&1
"$T" -open "$W/s.imd" -saveas "$W/s2.img" >/dev/null 2>&1
cmp -s "$W/s.img" "$W/s2.img" && ok || bad "IMD -> img -> IMD -> img differs"
[ "$(stat -f %z "$W/s.img" 2>/dev/null || stat -c %s "$W/s.img")" = $((3991*256)) ] && ok || bad "raw image size"

# ---- 3. export the system disc, import it into a new volume, export again: same files
"$T" -open "$DISKS/System.imd" -export "$W/x1" >/dev/null 2>&1
"$T" -new 500 COPY -import "$W/x1" -check -saveas "$W/copy.img" > "$W/imp.log" 2>&1
grep -q "volume is consistent" "$W/imp.log" && ok || { bad "import consistent"; tail -5 "$W/imp.log"; }
"$T" -open "$W/copy.img" -export "$W/x2" >/dev/null 2>&1
diff -r "$W/x1" "$W/x2" >/dev/null && ok || bad "export/import/export differs"
expect "links kept"          "LINK=7"                  "$T" -open "$W/copy.img" -attrib 8301/COMMANDS/ATTRIBUTE
# a file with 7 links is exported as 7 host names of one file (host hard links)
nl=$(find "$W/x1" -samefile "$W/x1/8301/COMMANDS/ATT[RIBUTE]" | wc -l); [ "$nl" -eq 7 ] && ok || bad "export: $nl host hard links, wanted 7"
n1=$(find "$W/x1" -type f | wc -l); [ "$n1" -gt 100 ] && ok || bad "export found only $n1 files"

# ---- 4. edits on a new volume, checked after every step
printf 'LINE ONE\nLINE TWO\n' > "$W/a.txt"; head -c 3000 /dev/urandom > "$W/b.bin"
V="$W/v.img"
step() { "$T" -open "$V" "$@" -check -save > "$W/step.log" 2>&1; grep -q "volume is consistent" "$W/step.log" && ok || { bad "step $*"; tail -4 "$W/step.log"; }; }
"$T" -new 64 EDITS -saveas "$V" >/dev/null 2>&1
step -mkdir D1 D1/D2
step -write "$W/a.txt" D1/A.TXT text
step -write "$W/b.bin" D1/D2/B.BIN
step -link D1/A.TXT ALINK
expect "link count 2"        "LINK=2"                  "$T" -open "$V" -attrib ALINK
step -delete D1/A.TXT
expect "survives via link"   "LINE TWO"                "$T" -open "$V" -type ALINK
step -move D1/D2 MOVED
"$T" -open "$V" -read MOVED/B.BIN "$W/b.out" >/dev/null 2>&1; cmp -s "$W/b.bin" "$W/b.out" && ok || bad "binary round trip"
step -write "$W/a.txt" MOVED/B.BIN text
expect "replace shrinks"     "SIZE=18"                 "$T" -open "$V" -attrib MOVED/B.BIN
step -attrib ALINK cap=OR/PR owner=HUGO
expect "attrib set"          "CAPABILITIES=OR/PR"      "$T" -open "$V" -attrib ALINK
step -delete D1 MOVED ALINK
expect "all freed"           "Free files     62"       "$T" -open "$V" -info
expect "bad name refused"    "invalid DOS/50 name"     "$T" -open "$V" -mkdir "TOO_LONG_A_NAME_X"
expect "dup refused"         "file exists"             "$T" -new -mkdir X X
# fill the volume: the error is reported and the volume stays consistent
head -c 1100000 /dev/zero > "$W/big"
"$T" -open "$V" -write "$W/big" BIG -check > "$W/full.log" 2>&1
grep -q "disc full" "$W/full.log" && grep -q "volume is consistent" "$W/full.log" && ok || { bad "disc full handling"; tail -4 "$W/full.log"; }

# ---- 5. DOS/50 itself (needs the 8550 emulator built: $MDL/emu/build/tek8550)
EMU="$MDL/emu/build/tek8550"
if [ -x "$EMU" ]; then
	printf 'BRACKETS OK\n' > "$W/b.txt"
	"$T" -new 256 TEKMATO -mkdir SRC -write "$W/a.txt" SRC/A.TXT text -write "$W/b.txt" 'SRC/BR[ACKET]' text -saveas "$W/d1.img" >/dev/null 2>&1
	cp "$DISKS/System_Fixed.img" "$W/d0.img"; chmod u+w "$W/d0.img"
	(cd "$MDL/emu" && ./build/tek8550 -rw -term tty -0 "$W/d0.img" -1 "$W/d1.img" -cycles $((75*2000000)) \
		-type 32 'COPY /VOL/TEKMATO/SRC/A.TXT CONO\r' -type 40 'COPY MAKE.KONG /VOL/TEKMATO/K\r' \
		-type 48 'CREATE /VOL/TEKMATO/NEW\r' -type 56 'DELETE:N /VOL/TEKMATO/SRC/A.TXT\r' \
		-type 62 'COPY /VOL/TEKMATO/SRC/BR[ACKET] CONO\r' </dev/null 2>/dev/null) | tr '\r' '\n' > "$W/emu.log"
	grep -q "LINE TWO" "$W/emu.log" && ok || bad "DOS/50 reads a tekmato file"
	grep -q "BRACKETS OK" "$W/emu.log" && ok || bad "DOS/50 reads a tekmato bracket name"
	grep -q "Error" "$W/emu.log" && bad "DOS/50 reported an error: $(grep Error "$W/emu.log" | head -1)" || ok
	"$T" -open "$W/d1.img" -check -list > "$W/after.log" 2>&1
	grep -q "volume is consistent" "$W/after.log" && grep -q " K " "$W/after.log" && grep -q "NEW/" "$W/after.log" && ok || { bad "DOS/50's writes read back"; cat "$W/after.log" | tail -8; }
	# every disc geometry: DOS/50's own FORMAT on a blank disc must equal tekmato -new
	# (label layout and bitmaps), and DOS/50 must read and write a tekmato .imd volume
	for g in sssd ssdd dssd dsdd; do
		case $g in sssd) recs=1989; opt=":S"; want=1001;; ssdd) recs=1989; opt=""; want=1989;;
		           dssd) recs=3991; opt=":S"; want=2002;; dsdd) recs=3991; opt=""; want=3991;; esac
		head -c $((recs*256)) /dev/zero > "$W/f_$g.img"
		cp "$DISKS/System_Fixed.img" "$W/d0.img"; chmod u+w "$W/d0.img"
		# the controller formats at the drive's real speed: two-sided discs need ~2.5 minutes
		(cd "$MDL/emu" && ./build/tek8550 -rw -term tty -0 "$W/d0.img" -1 "$W/f_$g.img" -cycles $((240*2000000)) \
			-type 32 "FORMAT${opt}:N /DEV/FLX1 100 FMT\\r" </dev/null >/dev/null 2>&1)
		[ $(( $(wc -c < "$W/f_$g.img") / 256 )) -eq $want ] && ok || bad "$g: FORMAT left $(( $(wc -c < "$W/f_$g.img") / 256 )) records, wanted $want"
		expect "$g FORMAT volume clean" "volume is consistent" "$T" -open "$W/f_$g.img" -check
		"$T" -new 100 FMT dos50-$g -saveas "$W/n_$g.img" >/dev/null 2>&1
		python3 -I - "$W/f_$g.img" "$W/n_$g.img" <<'PY' && ok || bad "$g: -new differs from FORMAT"
import sys, struct
a, b = (open(f, 'rb').read() for f in sys.argv[1:3])
la, lb = a[2560:2560 + 0x36], b[2560:2560 + 0x36]
bm = struct.unpack_from('<H', la, 0x2a)[0]
same = len(a) == len(b) and la[:16] == lb[:16] and la[0x2a:0x36] == lb[0x2a:0x36] and a[bm * 512:(bm + 3) * 512] == b[bm * 512:(bm + 3) * 512]
sys.exit(0 if same else 1)
PY
		"$T" -new 64 TK dos50-$g -write "$W/a.txt" A.TXT text -saveas "$W/tk_$g.imd" >/dev/null 2>&1
		cp "$DISKS/System_Fixed.img" "$W/d0.img"; chmod u+w "$W/d0.img"
		(cd "$MDL/emu" && ./build/tek8550 -rw -term tty -0 "$W/d0.img" -1 "$W/tk_$g.imd" -cycles $((60*2000000)) \
			-type 32 'COPY /VOL/TK/A.TXT CONO\r' -type 40 'COPY MAKE.KONG /VOL/TK/K\r' </dev/null 2>/dev/null) | tr '\r' '\n' > "$W/e_$g.log"
		grep -q "LINE TWO" "$W/e_$g.log" && ! grep -q "Error" "$W/e_$g.log" && ok || bad "$g: DOS/50 on a tekmato .imd"
		expect "$g: DOS/50's write read back" "LOG KONG.DISM" "$T" -open "$W/tk_$g.imd" -check -type K
	done
else
	echo "(skipping DOS/50 cross-check: $EMU not built)"
fi

# ---- 6. TEKDOS (8002A) discs (the last part needs the 8002 emulator built: $T8002/emu/build/tek8002)
if [ -d "$TK" ]; then
	B="$TK/Tek8002_Belgium_Amusements"
	expect "old format name kept" "Format         dos50-sssd" "$T" -new 64 X sssd -info
	expect "TEKDOS opened by size" "TEKDOS volume SYS 680X"    "$T" -open "$B/Tek8002_1a.img"
	# Tek8002_1a.jpg is a printout of LDIR / for this disc
	expect "TEKDOS list as LDIR /" "EFE           3   17  128   OLD 20F + 5F V 8606           7800-7FFF  709C" "$T" -open "$B/Tek8002_1a.img" -list
	expect "TEKDOS totals"         "BLOCKS AVAILABLE     0"    "$T" -open "$B/Tek8002_1a.img" -list
	expect "TEKDOS text file"      'STITLE	"MODULE : EDEF'    "$T" -open "$B/Tek8002_1a.img" -type 'EDEF%S'
	n=0; for f in "$TK"/*.img "$B"/*.img "$TK"/*/tekdos*.img; do "$T" -open "$f" -check 2>&1 | grep -q "volume is consistent" && n=$((n+1)); done
	[ "$n" -eq 29 ] && ok || bad "TEKDOS collection: $n of 33 consistent, wanted 29"
	expect "TEKDOS stale count"    "sector 0 counts 34 files, the directory names 50" "$T" -open "$B/Tek8002_18.img" -check
	# -new equals the disc TEKDOS's FORMAT left (Tek8002_1b, labelled FORMATTED)
	"$T" -new FORMATTED tkdos-sssd -saveas "$W/fmt.img" >/dev/null 2>&1
	cmp -s "$W/fmt.img" "$B/Tek8002_1b.img" && ok || bad "-new tkdos-sssd differs from a FORMATTED disc"
	expect "no IMD for TEKDOS"     "ImageDisk cannot hold"     "$T" -new X tkdos-sssd -saveas "$W/x.imd"
	expect "TEKDOS flat"           "TEKDOS discs have no directories" "$T" -new X tkdos-sssd -mkdir A
	expect "TEKDOS no links"       "TEKDOS discs have no links"  "$T" -new X tkdos-sssd -write "$W/a.txt" A -link A B
	expect "TEKDOS no attributes"  "TEKDOS discs have no file attributes" "$T" -new X tkdos-sssd -write "$W/a.txt" A -attrib A
	expect "TEKDOS name rule"      "start with a letter"       "$T" -new X tkdos-sssd -write "$W/a.txt" 1A
	expect "TEKDOS default name"   "-> A;TX"                   "$T" -new X tkdos-sssd -write "$W/a.txt" text
	# edits, checked after every step
	V="$W/tk.img"
	"$T" -new EDITS tkdos-sssd -saveas "$V" >/dev/null 2>&1
	step -write "$W/a.txt" 'SRC;S' text
	step -write "$W/b.bin" 'BIN%O'
	"$T" -open "$V" -read 'BIN%O' "$W/b.out" >/dev/null 2>&1; cmp -s "$W/b.bin" "$W/b.out" && ok || bad "TEKDOS binary round trip"
	step -move 'SRC;S' 'MAIN;S'
	step -write "$W/a.txt" 'BIN%O' text
	expect "TEKDOS replace shrinks" "BIN%O         1    1   18" "$T" -open "$V" -list
	step -delete 'MAIN;S' 'BIN%O'
	expect "TEKDOS all freed"      "Free blocks    288"        "$T" -open "$V" -info
	"$T" -open "$V" -write "$W/big" BIG -check > "$W/tkfull.log" 2>&1
	grep -q "disc full" "$W/tkfull.log" && grep -q "volume is consistent" "$W/tkfull.log" && ok || { bad "TEKDOS disc full"; tail -3 "$W/tkfull.log"; }
	# export / import / export of a system disc: same files
	"$T" -open "$TK/TekDOS_3.3_6800_cpy1.img" -export "$W/tx1" >/dev/null 2>&1
	"$T" -new REBUILT tkdos-sssd -import "$W/tx1" -check -saveas "$W/rebuilt.img" > "$W/tkimp.log" 2>&1
	grep -q "volume is consistent" "$W/tkimp.log" && ok || bad "TEKDOS import consistent"
	"$T" -open "$W/rebuilt.img" -export "$W/tx2" >/dev/null 2>&1
	diff -r "$W/tx1" "$W/tx2" >/dev/null && ok || bad "TEKDOS export/import/export differs"
	E8002="$T8002/emu/build/tek8002"
	if [ -x "$E8002" ]; then
		run8002() { "$E8002" -prom "$T8002/roms/tekdos_boot.bin" -rw "$@" </dev/null 2>/dev/null | tr -d '\000\007' | tr '\r' '\n'; }
		# TEKDOS boots from the rebuilt system disc
		run8002 -0 "$W/rebuilt.img" -cycles $((200*1250000)) -type prompt 'LDIR\r' > "$W/tkboot.log"
		grep -q "REBUILT" "$W/tkboot.log" && grep -q "TOTAL FILES" "$W/tkboot.log" && ok || bad "TEKDOS boots a tekmato-built system disc"
		# TEKDOS reads, copies and deletes on a tekmato disc; tekmato reads its writes back
		cp "$TK/TekDOS_3.3_6800_cpy1.img" "$W/tsys.img"; chmod u+w "$W/tsys.img"
		"$T" -new 'TEKMATO' tkdos-sssd -write "$W/a.txt" 'A;S' text -write "$W/b.bin" 'B%O' -saveas "$W/t1.img" >/dev/null 2>&1
		run8002 -0 "$W/tsys.img" -1 "$W/t1.img" -cycles $((600*1250000)) -type prompt 'LDIR 1\r' -type prompt 'COPY A;S/1 CONO\r' \
			-type prompt 'COPY A;S/1 COPIED/1\r' -type prompt 'DELETE B%O/1\r' > "$W/tkemu.log"
		grep -q "LINE TWO" "$W/tkemu.log" && ok || bad "TEKDOS reads a tekmato file"
		grep -q "ERROR" "$W/tkemu.log" && bad "TEKDOS reported an error: $(grep ERROR "$W/tkemu.log" | head -1)" || ok
		"$T" -open "$W/t1.img" -check -list > "$W/tkafter.log" 2>&1
		grep -q "volume is consistent" "$W/tkafter.log" && grep -q "COPIED" "$W/tkafter.log" && ! grep -q "B%O" "$W/tkafter.log" && ok || { bad "TEKDOS's writes read back"; tail -8 "$W/tkafter.log"; }
		# TEKDOS's own FORMAT equals -new
		# (on a never-formatted disc, new:, and typed at TEKDOS's prompt: the emulator runs the
		# real disc controller, whose timings are the drive's)
		run8002 -0 "$W/tsys.img" -1 "new:$W/blank.img" -cycles $((2000*1250000)) -type prompt 'FORMAT 1 FORMATTED\r' >/dev/null
		cmp -s "$W/blank.img" "$W/fmt.img" && ok || bad "TEKDOS FORMAT differs from -new tkdos-sssd"
	else
		echo "(skipping TEKDOS cross-check: $E8002 not built)"
	fi
else
	echo "(skipping TEKDOS checks: $TK not found)"
fi

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
