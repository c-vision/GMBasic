#!/bin/bash
# ffi_ext_flipgen.sh <nome.inc senza estensione> <dir libreria>... — migrazione FFI "estensioni automatiche"
# SENZA rompere il tree finche' l'interruttore e' spento: aggiunge in coda ai sorgenti C nativi uno SHIM
# `<sym>__ffi` (firma con int64_t dopo ogni array, chiama l'originale) e scrive in tools/ffi_ext_flip/<nome>.patch
# la patch che punta gli ALIAS del .inc agli shim. La patch si applica TUTTE INSIEME al momento del flip.
# Il .inc reale NON viene modificato. Uso tipico: ffi_ext_flipgen.sh neural shared/libs/neural shared/libs/compute shared/libs/math
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
inc="$1"; shift
W="$(mktemp -d)"; mkdir -p "$W/shared/include"; cp "shared/include/$inc.inc" "$W/shared/include/$inc.inc"
FFI_SHIM_ALL=1 perl tools/sh/ffi_ext_shim.pl "$W/shared/include/$inc.inc" /dev/null "$@"
mkdir -p tools/ffi_ext_flip
(cd "$W" && diff -u --label "a/shared/include/$inc.inc" --label "b/shared/include/$inc.inc" "$ROOT/shared/include/$inc.inc" "shared/include/$inc.inc" > "$ROOT/tools/ffi_ext_flip/$inc.patch") || true
rm -rf "$W"; echo "patch: tools/ffi_ext_flip/$inc.patch"
