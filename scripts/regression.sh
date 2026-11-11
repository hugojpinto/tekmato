#!/bin/bash
# tekmato regression: invariants on the MDL disc collection, round trips, and
# (if the MDL emulator is built next door) DOS/50 itself reading and writing
# volumes made by tekmato. Usage: scripts/regression.sh   (from the tekmato dir)
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
T="$HERE/tekmato"
MDL="$HERE/../MDL"
DISKS="$MDL/sources/bitsavers/disks"
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
expect "System free blocks"  "Free blocks    245"      "$T" -open "$MDL/disks/System_helpfix.img" -info
expect "System files used"   "Files used     224"      "$T" -open "$MDL/disks/System_helpfix.img" -info   # = LDIR
expect "System orphans"      "88 orphan files"         "$T" -open "$DISKS/System.imd" -check
expect "8085 disc clean"     "volume is consistent"    "$T" -open "$DISKS/062-4641-03_8300E06_8085_Emulator_V2.1_DOS50.imd" -check
expect "PROM disc clean"     "volume is consistent"    "$T" -open "$DISKS/062-6036-01_8550F30_prom_programmer_2.1_1982.imd" -check
expect "8086 disc clean"     "volume is consistent"    "$T" -open "$DISKS/8550_8086.IMD" -check
expect "DIAG 2.4 clean"      "volume is consistent"    "$T" -open "$DISKS/062-5412-08_8501_1err_DOS50.imd" -check
expect "DIAG lost blocks"    "24 blocks marked in use" "$T" -open "$DISKS/8550_DIAG.IMD" -check
expect "DOS/50 calendar"     "CREATION=02-OCT-80/08:52:43" "$T" -open "$DISKS/System.imd" -attrib 8501.SAV
expect "abbreviated name"    "ATT[RIBUTE]"             "$T" -open "$DISKS/System.imd" -list 8301/COMMANDS flat
expect "abbreviation lookup" "Alters or displays file attributes" "$T" -open "$MDL/disks/System_helpfix.img" -type 8301/COMMANDS/HELP
expect "hard links counted"  "LINK=7"                  "$T" -open "$DISKS/System.imd" -attrib 8301/COMMANDS/ATT

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

# ---- 5. DOS/50 itself (needs ../MDL/emu/build/tek8550)
EMU="$MDL/emu/build/tek8550"
if [ -x "$EMU" ]; then
	"$T" -new 256 TEKMATO -mkdir SRC -write "$W/a.txt" SRC/A.TXT text -saveas "$W/d1.img" >/dev/null 2>&1
	cp "$MDL/disks/System_helpfix.img" "$W/d0.img"
	(cd "$MDL/emu" && ./build/tek8550 -rw -term tty -0 "$W/d0.img" -1 "$W/d1.img" -cycles $((70*2000000)) \
		-type 32 'COPY /VOL/TEKMATO/SRC/A.TXT CONO\r' -type 40 'COPY MAKE.KONG /VOL/TEKMATO/K\r' \
		-type 48 'CREATE /VOL/TEKMATO/NEW\r' -type 56 'DELETE:N /VOL/TEKMATO/SRC/A.TXT\r' </dev/null 2>/dev/null) | tr '\r' '\n' > "$W/emu.log"
	grep -q "LINE TWO" "$W/emu.log" && ok || bad "DOS/50 reads a tekmato file"
	grep -q "Error" "$W/emu.log" && bad "DOS/50 reported an error: $(grep Error "$W/emu.log" | head -1)" || ok
	"$T" -open "$W/d1.img" -check -list > "$W/after.log" 2>&1
	grep -q "volume is consistent" "$W/after.log" && grep -q " K " "$W/after.log" && grep -q "NEW/" "$W/after.log" && ok || { bad "DOS/50's writes read back"; cat "$W/after.log" | tail -8; }
	# every disc geometry: DOS/50's own FORMAT on a blank disc must equal tekmato -new
	# (label layout and bitmaps), and DOS/50 must read and write a tekmato .imd volume
	for g in sssd ssdd dssd dsdd; do
		case $g in sssd) recs=1989; opt=":S"; want=1001;; ssdd) recs=1989; opt=""; want=1989;;
		           dssd) recs=3991; opt=":S"; want=2002;; dsdd) recs=3991; opt=""; want=3991;; esac
		head -c $((recs*256)) /dev/zero > "$W/f_$g.img"
		cp "$MDL/disks/System_helpfix.img" "$W/d0.img"
		(cd "$MDL/emu" && ./build/tek8550 -rw -term tty -0 "$W/d0.img" -1 "$W/f_$g.img" -cycles $((100*2000000)) \
			-type 32 "FORMAT${opt}:N /DEV/FLX1 100 FMT\\r" </dev/null >/dev/null 2>&1)
		[ $(( $(wc -c < "$W/f_$g.img") / 256 )) -eq $want ] && ok || bad "$g: FORMAT left $(( $(wc -c < "$W/f_$g.img") / 256 )) records, wanted $want"
		expect "$g FORMAT volume clean" "volume is consistent" "$T" -open "$W/f_$g.img" -check
		"$T" -new 100 FMT $g -saveas "$W/n_$g.img" >/dev/null 2>&1
		python3 -I - "$W/f_$g.img" "$W/n_$g.img" <<'PY' && ok || bad "$g: -new differs from FORMAT"
import sys, struct
a, b = (open(f, 'rb').read() for f in sys.argv[1:3])
la, lb = a[2560:2560 + 0x36], b[2560:2560 + 0x36]
bm = struct.unpack_from('<H', la, 0x2a)[0]
same = len(a) == len(b) and la[:16] == lb[:16] and la[0x2a:0x36] == lb[0x2a:0x36] and a[bm * 512:(bm + 3) * 512] == b[bm * 512:(bm + 3) * 512]
sys.exit(0 if same else 1)
PY
		"$T" -new 64 TK $g -write "$W/a.txt" A.TXT text -saveas "$W/tk_$g.imd" >/dev/null 2>&1
		cp "$MDL/disks/System_helpfix.img" "$W/d0.img"
		(cd "$MDL/emu" && ./build/tek8550 -rw -term tty -0 "$W/d0.img" -1 "$W/tk_$g.imd" -cycles $((60*2000000)) \
			-type 32 'COPY /VOL/TK/A.TXT CONO\r' -type 40 'COPY MAKE.KONG /VOL/TK/K\r' </dev/null 2>/dev/null) | tr '\r' '\n' > "$W/e_$g.log"
		grep -q "LINE TWO" "$W/e_$g.log" && ! grep -q "Error" "$W/e_$g.log" && ok || bad "$g: DOS/50 on a tekmato .imd"
		expect "$g: DOS/50's write read back" "LOG KONG.DISM" "$T" -open "$W/tk_$g.imd" -check -type K
	done
else
	echo "(skipping DOS/50 cross-check: $EMU not built)"
fi

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
