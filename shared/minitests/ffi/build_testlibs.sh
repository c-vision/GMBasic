#!/bin/sh
# build_testlibs.sh — ricompila le librerie native usate dai MiniTest FFI.
#
# Perché esiste: i `.dylib` delle fixture sono esclusi dal `.gitignore` (`*.dylib`), quindi da
# un clone pulito NON ci sono. Alcune sono storiche e già tracciate (`libvec_module.dylib`),
# altre (`libudt_ext`, `libdims_ext`) si generano da qui. Nessun Python, nessuna dipendenza.
#
# Uso:  sh shared/minitests/ffi/build_testlibs.sh
set -e
cd "$(dirname "$0")"

build() { # $1 = sorgente, $2 = nome senza estensione
    src="$1"; out="$2"
    case "$(uname -s)" in
        Darwin) ext=".dylib" ;;
        *)      ext=".so" ;;
    esac
    echo "  cc -shared $src -> $out$ext"
    cc -shared -fPIC -O2 -o "$out$ext" "$src"
}

build udt_ext.c   libudt_ext
build dims_ext.c  libdims_ext

echo "ok: librerie di test FFI pronte in $(pwd)"
