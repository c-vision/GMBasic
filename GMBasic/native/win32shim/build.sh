#!/usr/bin/env bash
# build.sh — compila la shim Win32 e crea i symlink per-libreria in <monorepo>/shared/libs/win32/.
#
# La shim esporta TUTTI i simboli Win32 (wininet/kernel32/user32/shell32/advapi32/gdi32)
# in un'unica dylib; i symlink `lib<lib>.dylib` permettono all'FFI di risolvere
# `DECLARE ... LIB "wininet"` (auto-discovery `libs/`, Guide/FFI-Guida-Utente.md §9).
#
# Uso: native/win32shim/build.sh   (da qualunque CWD; i path sono relativi allo script)
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
# Le nostre librerie interne vivono in `shared/libs` (condivise dai due compilatori).
LIBS="$(cd "$ROOT/../shared" && pwd)/libs/win32"   # una sottocartella: non sporca shared/libs
mkdir -p "$LIBS"
OUT="$LIBS/libwin32shim"

case "$(uname -s)" in
  Darwin) EXT=dylib; FLAGS="-dynamiclib" ;;
  Linux)  EXT=so;    FLAGS="-shared -fPIC" ;;
  *)      EXT=dll;   FLAGS="-shared" ;;
esac

echo "cc -> $OUT.$EXT"
cc -O2 $FLAGS -fvisibility=hidden -I"$HERE" -o "$OUT.$EXT" "$HERE/win32shim.c"

# symlink per-libreria: l'FFI risolve "wininet" via libs/libwininet.$EXT
for L in wininet kernel32 user32 shell32 advapi32 gdi32 winmm ws2_32 urlmon comctl32 netapi32 iphlpapi; do
  ln -sf "libwin32shim.$EXT" "$LIBS/lib$L.$EXT"
done
echo "symlink: $LIBS/lib{wininet,kernel32,user32,shell32,advapi32,gdi32}.$EXT"
echo "fatto."
