#!/usr/bin/env bash
# GMBASIC — Gate terminologico del manuale (fase F2 del piano manuale-cartaceo-PUL).
#
# Controlla i testi del libro (`docs/manual/*.md`, ricorsivo) contro le forme vietate
# fissate in `docs/manual/stile.md` e nel glossario: numero di backend, `parity5`,
# `C99Backend`, superficie 982, PUL chiamata open source, percorsi morti, cache legacy.
#
# Sono esclusi dal controllo i due file che *citano* le forme vietate per spiegarle:
# `glossario.md` e `stile.md`.
#
# Uso:
#   tools/sh/check_manual_terms.sh            # elenca le violazioni, exit 1 se ce ne sono
#   tools/sh/check_manual_terms.sh --quiet    # solo il verdetto (per CI)
#
# Exit code: 0 = pulito (anche quando non ci sono ancora capitoli), 1 = violazioni, 2 = libro assente.

MONO="$(cd "$(dirname "$0")/../.." && pwd)"
BOOK="$MONO/docs/manual"
QUIET=0
[ "${1:-}" = "--quiet" ] && QUIET=1

if [ ! -d "$BOOK" ]; then
  echo "ERRORE: cartella del libro assente: $BOOK" >&2
  exit 2
fi

HITS=0
SCANNED=0

check_file() {
  local f="$1"
  SCANNED=$((SCANNED + 1))
  while IFS=$'\t' read -r label pattern; do
    [ -z "${pattern:-}" ] && continue
    while IFS= read -r line; do
      [ -z "$line" ] && continue
      HITS=$((HITS + 1))
      if [ "$QUIET" -eq 0 ]; then
        printf '%s:%s: [%s] %s\n' "${f#$MONO/}" "${line%%:*}" "$label" "${line#*:}"
      fi
    done < <(grep -nE -- "$pattern" "$f" 2>/dev/null)
  done <<'PATTERNS'
numero-backend	(cinque|5)[[:space:]]+(backend|gambe)
parity5	parity5
c99backend-ritirato	C99Backend|GmbBackendC99
superficie-982	(^|[^0-9])982([^0-9]|$)
pul-open-source	open[[:space:]-]?source
percorso-morto	docs/(comparison|implemented)|Sync-GMBasic-Monorepo\.md
cache-legacy	GMB_PKG_CACHE
PATTERNS
}

while IFS= read -r -d '' f; do
  case "$(basename "$f")" in
    glossario.md|stile.md) continue ;;
  esac
  check_file "$f"
done < <(find "$BOOK" -type f -name '*.md' -print0 2>/dev/null)

if [ "$HITS" -eq 0 ]; then
  [ "$QUIET" -eq 0 ] && echo "OK: nessuna forma vietata in $SCANNED file del libro."
  exit 0
fi

echo "FALLITO: $HITS violazioni in $SCANNED file (regole in docs/manual/stile.md)." >&2
exit 1
