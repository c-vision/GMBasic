#!/usr/bin/env bash
# gmb_lib.sh — COME SI COMPILA (e si pubblica) una delle NOSTRE librerie FFI:
# amqp, grpc, kafka, mqtt, nats, protobuf, media, opt.
#
# Le librerie vivono in `shared/libs/<nome>/` insieme al resto del condiviso: sono usate da
# ENTRAMBI i compilatori (export BASIC `.inc`, header C, dylib per l'FFI).
#
# CONTRATTO DI BUILD (normalizzato — prima ogni progetto faceva a modo suo):
#   sorgenti      shared/libs/<nome>/src/*.c
#   header        shared/libs/<nome>/include/*.h        (interni del progetto)
#                 shared/include/gmb<nome>.h            (PUBBLICI, cercati dal compilatore)
#   export BASIC  shared/include/<nome>.inc             (PUBBLICI, risolti da IMPORT)
#   binario       shared/libs/<nome>/libgmb<nome>.<ext> (dove l'FFI lo cerca)
#
# USO
#   tools/sh/gmb_lib.sh build   <nome>            compila la lib (o tutte con `all`)
#   tools/sh/gmb_lib.sh check   <nome>            verifica install_name @rpath + file attesi
#   tools/sh/gmb_lib.sh publish <nome> [--tag T]  carica il binario su c-vision/GMBasicModules
#                                                 (gruppo `ffi`, tag immutabile `ffi-v1`)
#
# Il PUBLISH usa gli strumenti del monorepo (`libs_publish_files_gh.sh`) e poi rigenera l'indice
# con `libs_manifest_gh.sh`: la capacità di aggiornare GMBasicModules resta identica.
set -eu

MONO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LIBS="$MONO/shared/libs"
INCLUDE="$MONO/shared/include"
TOOLS="$MONO/tools/sh"

ALL_LIBS="amqp grpc kafka mqtt nats protobuf media opt graph flow or"

die() { echo "gmb_lib: $*" >&2; exit 1; }

# Nome dell'export BASIC di un pacchetto (default `<lib>.inc`). Il pacchetto `opt` espone il
# namespace OPTIMIZE → `optimize.inc` (2026-11-03, riuso del namespace del builtin dismesso).
inc_name() {
  case "$1" in
    opt) echo "optimize.inc" ;;
    or)  echo "ortools.inc" ;;
    *)   echo "$1.inc" ;;
  esac
}

rid() {
  case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)  echo osx-arm64 ;;
    Darwin-x86_64) echo osx-x64 ;;
    Linux-x86_64)  echo linux-x64 ;;
    Linux-aarch64|Linux-arm64) echo linux-arm64 ;;
    *)             echo osx-arm64 ;;
  esac
}

ext() {
  case "$(uname -s)" in
    Darwin) echo dylib ;;
    Linux)  echo so ;;
    *)      echo dll ;;
  esac
}

# ── build ─────────────────────────────────────────────────────────────────────────────────────
build_one() {
  local lib="$1"
  local dir="$LIBS/$lib"
  [ -d "$dir/src" ] || die "sorgenti assenti: $dir/src"
  [ -d "$dir/include" ] || die "header interni assenti: $dir/include"

  local E; E="$(ext)"
  local out="$dir/libgmb$lib.$E"
  local cc="${CC:-cc}"
  local srcs; srcs=$(find "$dir/src" -name '*.c' | sort)

  echo "[$lib] cc -> $out"
  # `@rpath` = relocabile (distribuibile). NIENTE `-fvisibility=hidden`: gli export sono i
  # simboli C che l'FFI cerca per nome, senza attributi nel sorgente.
  case "$(uname -s)" in
    Darwin)
      $cc -std=c99 -O2 -fPIC -dynamiclib \
          -I"$dir/include" -I"$dir/src" -I"$INCLUDE" \
          $srcs -o "$out" -lm -ldl -install_name "@rpath/libgmb$lib.$E" ;;
    *)
      $cc -std=c99 -O2 -fPIC -shared \
          -I"$dir/include" -I"$dir/src" -I"$INCLUDE" \
          $srcs -o "$out" -lm -ldl ;;
  esac
  # Gli export BASIC e gli header PUBBLICI vivono in shared/include (sorgente, in git).
  sync_sources "$lib"
}

# Sposta/copia gli export e gli header pubblici in shared/include (una sola posizione).
sync_sources() {
  local lib="$1"
  local dir="$LIBS/$lib"
  mkdir -p "$INCLUDE"
  local incn; incn="$(inc_name "$lib")"
  [ -f "$dir/$incn" ] && mv -f "$dir/$incn" "$INCLUDE/$incn"
  for h in "$dir"/gmb"$lib"*.h; do
    [ -f "$h" ] || continue
    cp -f "$h" "$INCLUDE/"
  done
  return 0
}

# ── check ─────────────────────────────────────────────────────────────────────────────────────
check_one() {
  local lib="$1"
  local E; E="$(ext)"
  local bin="$LIBS/$lib/libgmb$lib.$E"
  [ -f "$bin" ] || die "binario assente: $bin"
  local incn; incn="$(inc_name "$lib")"
  [ -f "$INCLUDE/$incn" ] || echo "  ATTENZIONE: manca l'export BASIC $INCLUDE/$incn" >&2
  if [ "$(uname -s)" = "Darwin" ]; then
    otool -D "$bin" | tail -n +2 | grep -q "^@rpath/libgmb$lib\.$E$" \
      || die "$bin: install_name non è @rpath/libgmb$lib.$E (non sarebbe distribuibile)"
  fi
  echo "[$lib] ok: $bin"
}

# ── publish ───────────────────────────────────────────────────────────────────────────────────
publish_one() {
  local lib="$1"; shift
  local R; R="$(rid)"; local E; E="$(ext)"
  local bin="$LIBS/$lib/libgmb$lib.$E"
  [ -f "$bin" ] || die "binario assente: $bin (esegui prima `build $lib`)"
  local tag="ffi-v1"
  while [ $# -gt 0 ]; do
    case "$1" in
      --tag) tag="${2:?}"; shift 2 ;;
      *) die "opzione sconosciuta: $1" ;;
    esac
  done
  local stage; stage="$(mktemp -d "${TMPDIR:-/tmp}/gmblib.XXXXXX")"
  trap 'rm -rf "$stage"' RETURN
  cp "$bin" "$stage/"
  bash "$TOOLS/libs_publish_files_gh.sh" ffi "$R" --source "$stage" --tag "$tag" --yes
  # Indice allineato alle release reali (il gruppo `ffi` entra nel manifest come gli altri).
  bash "$TOOLS/libs_manifest_gh.sh" build --rid "$R" --groups ffi 2>/dev/null || true
  echo "[$lib] pubblicato su $tag ($R)"
}

case "${1:-}" in
  build)
    shift
    if [ "${1:-}" = "all" ] || [ -z "${1:-}" ]; then
      for l in $ALL_LIBS; do build_one "$l" || exit 1; done
    else
      build_one "$1"
    fi
    ;;
  check)
    shift
    if [ "${1:-}" = "all" ] || [ -z "${1:-}" ]; then
      for l in $ALL_LIBS; do check_one "$l" || exit 1; done
    else
      check_one "$1"
    fi
    ;;
  publish)
    shift; lib="${1:?nome libreria}"; shift
    publish_one "$lib" "$@"
    ;;
  *)
    sed -n '2,25p' "$0"
    exit 2
    ;;
esac
