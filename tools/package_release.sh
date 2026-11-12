#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Hugo José Pinto <hugo.pinto@gmail.com>
# Package a binary release: the program with its documentation, as a .tar.gz, or a .zip for
# Windows. Used by .github/workflows/release.yml; works locally too.
# usage: tools/package_release.sh PLATFORM PROGRAM OUT_DIR   (e.g. macos-universal ./tekmato dist)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME=tekmato
PLATFORM="$1"; PROGRAM="$2"; OUT="$3"
[ -n "$PLATFORM" ] && [ -f "$PROGRAM" ] && [ -n "$OUT" ] || { echo "usage: $0 PLATFORM PROGRAM OUT_DIR"; exit 1; }
VERSION="$(cat "$ROOT/VERSION")"
EXE=; case "$PLATFORM" in windows*) EXE=.exe;; esac
D="$OUT/$NAME-$VERSION-$PLATFORM"
rm -rf "$D"; mkdir -p "$D"
cp "$PROGRAM" "$D/tekmato$EXE"
cp "$ROOT/README.md" "$ROOT/FORMAT.md" "$ROOT/FORMAT-TEKDOS.md" "$ROOT/LICENSE" "$ROOT/NOTICE.md" "$ROOT/CHANGELOG.md" "$ROOT/VERSION" "$D/"
X=./; [ -n "$EXE" ] && X=
cat > "$D/QUICKSTART.txt" <<TXT
tekmato $VERSION ($PLATFORM): DOS/50 and TEKDOS disc images of the Tektronix 8550 MDL and
8001/8002/8002A development labs

  ${X}tekmato$EXE -help                                   the commands
  ${X}tekmato$EXE -open System.imd -info -list            what is on a disc
  ${X}tekmato$EXE -open System.imd -export system text    copy every file out
  ${X}tekmato$EXE -new 256 WORK -import src SRC text -saveas work.img

Changes stay in memory until -save or -saveas: work on copies. README.md has the full command
list; FORMAT.md and FORMAT-TEKDOS.md the on-disc formats. ${X}tekmato$EXE -version prints the
version. Source, tests and disc images: https://github.com/hugojpinto/tekmato
TXT
case "$PLATFORM" in
macos*) printf '\nmacOS: tekmato is not signed or notarised, so Gatekeeper blocks it at first. Clear the\ndownload quarantine once, in Terminal, in this folder: xattr -dr com.apple.quarantine .\n(or run it, then System Settings > Privacy & Security > Open Anyway).\n' >> "$D/QUICKSTART.txt";;
windows*) printf '\r\nWindows: tekmato is not code-signed, so SmartScreen may warn: More info > Run anyway.\r\nUnblocking the zip before extracting avoids it (Properties > Unblock, or Unblock-File in PowerShell).\r\n' >> "$D/QUICKSTART.txt";;
esac
cd "$OUT"
if [ -n "$EXE" ]; then rm -f "$NAME-$VERSION-$PLATFORM.zip"; zip -qr "$NAME-$VERSION-$PLATFORM.zip" "$NAME-$VERSION-$PLATFORM"; echo "$OUT/$NAME-$VERSION-$PLATFORM.zip"
else tar czf "$NAME-$VERSION-$PLATFORM.tar.gz" "$NAME-$VERSION-$PLATFORM"; echo "$OUT/$NAME-$VERSION-$PLATFORM.tar.gz"; fi
