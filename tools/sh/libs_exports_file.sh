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
# Cosa fa oggi (dopo la riduzione a 4 simboli, 2026-11-12): `abi` è la fonte AUTOREVOLE del link —
# i tre runner la usano e il gate che segue il link verifica quegli stessi simboli; `list` e
# `verify` restano per la diagnostica a richiesta. NON esiste più un file `libgmb.exports.txt`:
# nessun runner lo leggeva, e un manifest prodotto e copiato nel bundle senza essere consumato fa
# credere che il link lo usi (due hook lo scrivevano, rimossi). Per la lista completa dei requisiti:
# `list <dir-dylib>…`.
#
# Uso:
#   libs_exports_file.sh [abi]                              → i simboli ABI MINIMI (4), su stdout
#   libs_exports_file.sh list   <dir> [<dir>…]               → stampa la lista completa su stdout
#   libs_exports_file.sh verify <eseguibile> <file-export>   → exit 1 se restano simboli scoperti
#
# QUALI si forzano al link. SOLO `abi` (4 simboli): sono quelli che il core offre alla dylib
# quando questa si carica, e forzarli costa pochi KB. LA LISTA COMPLETA NON SI FORZA PIÙ: `-u`
# non "esporta" un simbolo, lo OBBLIGA a restare nel link trascinando l'oggetto che lo definisce
# — misurato 2026-11-12, su `PRINT "ciao"` la lista completa portava il binario da 198 KB a 594 KB
# e ci infilava oggetti AI (`gmb_ai_*`) mai chiamati dal BASIC. Dal fix dell'anchor in
# `gmb_abi.c` (gmb_abi_set_llmdesc referenziato dal core) lld tiene ed esporta da sé tutto ciò che
# le dylib si aspettano: i sette MiniTest AI passano con zig senza forzare nulla.
set -uo pipefail

MODE="${1:-abi}"

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
  abi)
    # ABI MINIMA che il core deve offrire alla dylib quando questa si carica:
    #  - `gmb_abi_set_llmdesc`: chiamato dal COSTRUTTORE di libgmb_runtime_ai (load-time);
    #  - i tre target del `dlsym(RTLD_DEFAULT, …)` che non passano da un import.
    printf '%s\n' _gmb_abi_set_llmdesc "${DLSYM_SYMS[@]}"
    ;;

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
    die "modo sconosciuto: $MODE (abi|list|verify)"
    ;;
esac
