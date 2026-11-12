#!/usr/bin/env bash
# tools/sh/libs_exports_file.sh — i simboli che le NOSTRE dylib cercano a LOAD-TIME.
#
# Perché serve. Le dylib nostre (`libgmb_runtime_ai`, `libgmb_ai_llm`, `libgmb_ai_modelio`, …) sono
# linkate con `-undefined dynamic_lookup`: non importano il core, lo risolvono quando dyld le carica,
# e chi lo fornisce è la TABELLA DEGLI EXPORT dell'eseguibile. Se un simbolo manca non c'è errore
# di link: la dylib ha uno stub che al primo uso salta a 0 (crash in un inizializzatore, prima di
# `main`). Su Darwin ld64 esporta i globali dell'eseguibile; lld di `zig cc` dead-strippa quelli che
# nulla referenzia (misurato 2026-11-12, stesso link: clang 4020 export con 0 su 35 assenti, zig 198
# con 32 su 35 assenti). Da qui l'obbligo di forzare ogni simbolo cercato con `-Wl,-u,<simbolo>`.
#
# Perché un FILE e non gli import letti al volo. Gli import describe le dylib che ci sono ORA su
# questa macchina: una dylib ricompilata altrove (o un bundle pubblicato) porta requisiti che il
# linker locale non vedrebbe, e il programma nascerebbe con un buco. Il bundle scrive con `write`
# la lista `libgmb.exports.txt` accanto alle dylib; il runner la legge (`list` non serve più) e,
# dopo il link, `verify` blocca la consegna se resta un simbolo scoperto.
#
# Uso:
#   libs_exports_file.sh write  <dir-bundle>                 → scrive <dir>/libgmb.exports.txt
#   libs_exports_file.sh list   <dir> [<dir>…]               → stampa la lista su stdout
#   libs_exports_file.sh verify <eseguibile> <file-export>   → exit 1 se restano simboli scoperti
#
# `list` esiste per il fallback del runner: se il bundle non ha il file, la lista si ricava dalle
# dylib presenti (equivalente al file, ma non autorevole: il runner lo dichiara).
set -uo pipefail

MODE="${1:-write}"

# I tre simboli che il core cerca con `dlsym(RTLD_DEFAULT, …)` (native/gmb_runtime_c/src/gmb_abi.c):
# non passano da un import, quindi non compaiono in `dyld_info -imports` e vanno aggiunti a mano.
DLSYM_SYMS=(_llmdesc_abi_valid _llmdesc_abi_locate _llmdesc_set_abi_policy)

die() { echo "libs_exports_file: $*" >&2; exit 2; }

need_dyld_info() {
  command -v dyld_info >/dev/null 2>&1 || die "dyld_info assente (serve per leggere gli import/exports Mach-O)"
}

# Import `<flat-namespace>` di una dylib, una riga per simbolo.
# `|| true`: una dylib senza import flat-namespace fa uscire `grep` con 1 e con `pipefail` la
# pipeline fallirebbe — non è un errore, è una dylib che non partecipa.
imports_of() {
  dyld_info -imports "$1" 2>/dev/null | grep 'flat-namespace' \
    | tr -s ' ' | cut -d' ' -f2 | grep '^_[A-Za-z_]' || true
}

# Export dell'eseguibile, una riga per simbolo.
exports_of() {
  dyld_info -exports "$1" 2>/dev/null | awk '{print $NF}' | grep '^_[A-Za-z_]' | sort -u || true
}

case "$MODE" in
  list)
    [ "$#" -ge 2 ] || die "list: serve almeno una directory"
    need_dyld_info
    {
      for dir in "${@:2}"; do
        [ -d "$dir" ] || continue
        for d in "$dir"/libgmb*.dylib; do
          [ -f "$d" ] || continue
          imports_of "$d"
        done
      done
      printf '%s\n' "${DLSYM_SYMS[@]}"
    } | sort -u
    ;;

  write)
    dir="${2:-}"
    [ -n "$dir" ] || die "write: serve la directory del bundle"
    [ -d "$dir" ] || die "write: directory inesistente: $dir"
    need_dyld_info
    out="$dir/libgmb.exports.txt"
    tmp="$(mktemp "${TMPDIR:-/tmp}/gmbexports.XXXXXX")"
    # Lista vuota = nessuna dylib nel bundle: non è un errore, ma si dichiara (il file non si
    # scrive: un file vuoto verrebbe letto come "niente da esportare" senza spiegazione).
    syms="$({
      for d in "$dir"/libgmb*.dylib; do [ -f "$d" ] && imports_of "$d"; done
      printf '%s\n' "${DLSYM_SYMS[@]}"
    } | sort -u)"
    if [ -z "$syms" ]; then
      rm -f "$tmp"
      echo "libs_exports_file: nessuna dylib in $dir — $out NON scritto" >&2
      exit 0
    fi
    printf '%s\n' "$syms" > "$tmp"
    mv -f "$tmp" "$out"
    echo "libs_exports_file: $out ($(printf '%s\n' "$syms" | wc -l | tr -d ' ') simboli)" >&2
    ;;

  verify)
    bin="${2:-}"; list_file="${3:-}"
    [ -n "$bin" ] && [ -n "$list_file" ] || die "verify: servono <eseguibile> <file-export>"
    [ -f "$bin" ] || die "verify: eseguibile inesistente: $bin"
    [ -f "$list_file" ] || die "verify: lista export inesistente: $list_file"
    need_dyld_info
    exp="$(mktemp "${TMPDIR:-/tmp}/gmbexp.XXXXXX")"
    exports_of "$bin" > "$exp"
    missing="$(comm -23 <(sort -u "$list_file") "$exp")"
    n="$(printf '%s' "$missing" | grep -c '^_' || true)"
    rm -f "$exp"
    if [ "${n:-0}" -gt 0 ]; then
      echo "libs_exports_file: $n simboli cercati a load-time dalle dylib NON esportati da $(basename "$bin"):" >&2
      printf '%s\n' "$missing" | sed 's/^/    /' >&2
      exit 1
    fi
    exit 0
    ;;

  *)
    die "modo sconosciuto: $MODE (write|list|verify)"
    ;;
esac
