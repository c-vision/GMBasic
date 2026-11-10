#!/usr/bin/env bash
# tools/sh/zigcc.sh — wrapper del COMPILATORE C per CMake e per gli script di build.
#
# Scopo (F1 del piano Toolchain-Zig): un solo punto in cui si sceglie il compilatore C.
# Il wrapper esegue `zig cc "$@"`, quindi si usa come qualunque `cc`:
#
#   tools/sh/zigcc.sh -c file.c -o file.o
#   cmake -DCMAKE_C_COMPILER=<repo>/tools/sh/zigcc.sh ...
#
# Risoluzione della toolchain, in ordine (NESSUNA variabile d'ambiente per controllare
# il comportamento: AGENTS, regola primaria — solo argomenti):
#   1. `--zig <path>`   percorso esplicito del binario zig (consumato da questo wrapper);
#   2. `--zig-version`  versione da cercare nella cache (default: ZIG_VERSION sotto);
#   3. cache del piano: ~/.gmbasic/toolchains/zig/<versione>-<rid>/zig
#                       (poi qualunque <versione>-*/zig, per RID diversi dall'host);
#   4. `zig` nel PATH      (ultima spiaggia: toolchain di sistema);
#   5. errore esplicito che dice COSA manca e COME installarlo (mai fallback silenzioso).
#
# Cache di compilazione (D8): ~/.gmbasic/toolchains/zig-cache, passata con `--cache-dir`
# SOLO se il binario zig la supporta (verificato a runtime, una volta per invocazione):
# così un zig più vecchio non riceve un flag che non conosce.
set -euo pipefail

ZIG_VERSION="0.17.0"
CACHE_DIR="${HOME}/.gmbasic/toolchains/zig-cache"
TOOLCHAINS="${HOME}/.gmbasic/toolchains/zig"

ZIG_ARG=""
_ARGS=()
while [ "$#" -gt 0 ]; do
  case "$1" in
    --zig)          ZIG_ARG="${2:-}"; shift 2; continue ;;
    --zig=*)        ZIG_ARG="${1#--zig=}"; shift; continue ;;
    --zig-version)  ZIG_VERSION="${2:-}"; shift 2; continue ;;
    --zig-version=*) ZIG_VERSION="${1#--zig-version=}"; shift; continue ;;
    --)             _ARGS+=("$@"); break ;;
    *)              _ARGS+=("$1"); shift ;;
  esac
done

host_rid() {
  local os arch
  os="$(uname -s)"; arch="$(uname -m)"
  case "$arch" in aarch64|arm64) arch=arm64 ;; x86_64|amd64) arch=x86_64 ;; esac
  case "$os-$arch" in
    Darwin-arm64)   echo osx-arm64 ;;
    Darwin-x86_64)  echo osx-x64 ;;
    Linux-x86_64)   echo linux-x64 ;;
    Linux-arm64)    echo linux-arm64 ;;
    MINGW*|MSYS*|CYGWIN*) case "$arch" in arm64) echo win-arm64 ;; *) echo win-x64 ;; esac ;;
    *)              echo unknown ;;
  esac
}

resolve_zig() {
  if [ -n "$ZIG_ARG" ]; then
    [ -x "$ZIG_ARG" ] || { echo "zigcc: --zig '$ZIG_ARG' non eseguibile" >&2; return 1; }
    echo "$ZIG_ARG"; return 0
  fi
  local rid candidate
  rid="$(host_rid)"
  candidate="$TOOLCHAINS/${ZIG_VERSION}-${rid}/zig"
  if [ -x "$candidate" ]; then echo "$candidate"; return 0; fi
  # Stessa versione, RID diverso dall'host (es. bundle scaricato per un'altra piattaforma).
  candidate="$(ls -1 "$TOOLCHAINS/${ZIG_VERSION}"-*/zig 2>/dev/null | head -1 || true)"
  if [ -n "$candidate" ] && [ -x "$candidate" ]; then echo "$candidate"; return 0; fi
  # Toolchain di sistema (esplicitamente accettata come fallback, non come default).
  candidate="$(command -v zig 2>/dev/null || true)"
  if [ -n "$candidate" ] && [ -x "$candidate" ]; then echo "$candidate"; return 0; fi
  cat >&2 <<EOF
zigcc: nessuna toolchain zig ${ZIG_VERSION} trovata.
  cercata in: ${TOOLCHAINS}/${ZIG_VERSION}-<rid>/zig
  rimedio:    gmb toolchain ensure            (scarica ${ZIG_VERSION} per il RID host)
oppure:       tools/sh/zigcc.sh --zig /percorso/del/binario/zig ...
EOF
  return 1
}

ZIG="$(resolve_zig)" || exit 2

# Cache di compilazione (D8): `zig cc --help` NON elenca `--cache-dir` (mostra l'help di clang),
# quindi il supporto si verifica con una compilazione vera, una sola volta, e l'esito resta in un
# marker DENTRO la cache (non una variabile d'ambiente: AGENTS). Se un zig piu' vecchio non lo
# supporta si compila senza redirezione, senza fallire.
EXTRA=()
if mkdir -p "$CACHE_DIR" 2>/dev/null; then
  MARK_OK="$CACHE_DIR/.cache-dir-ok"
  MARK_NO="$CACHE_DIR/.cache-dir-unsupported"
  if [ ! -f "$MARK_OK" ] && [ ! -f "$MARK_NO" ]; then
    if "$ZIG" cc --cache-dir "$CACHE_DIR" -c -x c /dev/null -o /dev/null >/dev/null 2>&1; then
      : > "$MARK_OK"
    else
      : > "$MARK_NO"
    fi
  fi
  if [ -f "$MARK_OK" ]; then EXTRA=(--cache-dir "$CACHE_DIR"); fi
fi

# bash 3.2 (macOS): `"${array[@]}"` su array vuoto e' "unbound variable" con `set -u`.
if [ "${#EXTRA[@]}" -gt 0 ]; then
  exec "$ZIG" cc "${EXTRA[@]}" ${_ARGS[@]+"${_ARGS[@]}"}
fi
exec "$ZIG" cc ${_ARGS[@]+"${_ARGS[@]}"}
