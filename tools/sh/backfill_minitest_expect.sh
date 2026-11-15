#!/usr/bin/env bash
# backfill_minitest_expect.sh — riempie la riga canonica `' EXPECT: ` dei MiniTest che NON
# ce l'hanno, usando la gamba **C99Native** (riferimento) e con CONTROLLI di sicurezza.
#
# PERCHÉ ESISTE. Un MiniTest senza `' EXPECT: ` verifica solo l'exit code e la parità fra
# gambe: quattro gambe d'accordo su un valore SBAGLIATO restano verdi. Il gate statico
# `tools/basic/check_minitest_expect.bas` elenca questi file (oggi 333). Riempirli a mano è
# lungo; farlo con un backfill cieco è pericoloso (l'oracolo `ok= list-fail` di
# ai/observe_list è nato da un backfill su un run FALLITO).
#
# REGOLE (un file che non le passa è SKIP, mai un oracolo indovinato):
#   1. exit code 0 sulla gamba C99Native;
#   2. output NON vuoto e STABILE su DUE esecuzioni consecutive;
#   3. nessun path assoluto o valore volatile nell'output (`/Users/`, `/tmp/`, `/private/`,
#      `$HOME`, date/ore, durate `ms`/`s`);
#   4. la riga `' EXPECT: ` viene inserita PRIMA della prima riga eseguibile (l'oracolo deve
#      stare nell'header: è la regola dei due consumer, vedi Knowledge/MiniTest-Oracoli-*).
#
# Uso:
#   tools/sh/backfill_minitest_expect.sh [--apply] [dominio...]
#     senza --apply: DRY-RUN (scrive solo il report su stdout)
#     dominio: cartella sotto shared/minitests (default: tutte)
#
# Output: per ogni file `BACKFILL <file> :: <output>` oppure `SKIP <file> (<motivo>)`.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GMBASIC_ROOT="${GMBASIC_ROOT:-$ROOT/GMBasic}"
export GMBASIC_ROOT
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-dummy}"
RUN="$ROOT/tools/sh/gmb_c99native_run.sh"
MINI="$ROOT/shared/minitests"

APPLY=0
DOMAINS=()
for a in "$@"; do
  case "$a" in
    --apply) APPLY=1 ;;
    -h|--help) sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) DOMAINS+=("$a") ;;
  esac
done

# normalizza come gli harness: trim per riga, scarta le righe vuote, unisce con '|'
norm() { sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' | grep -v '^$' | tr '\n' '|' | sed 's/|$//'; }

# valori che NON devono finire in un oracolo (dipendono dalla macchina o dal tempo)
volatile() {
  printf '%s' "$1" | grep -qE '/Users/|/tmp/|/private/|/var/folders|\$HOME|https?://|:[0-9]+ms|: [0-9.]+s\b|20[0-9]{2}-[0-9]{2}-[0-9]{2}'
}

has_oracle() { grep -qE "^' EXPECT: " "$1"; }

files=()
if [ ${#DOMAINS[@]} -eq 0 ]; then
  while IFS= read -r f; do files+=("$f"); done < <(find "$MINI" -name '*.bas' | sort)
else
  for d in "${DOMAINS[@]}"; do
    while IFS= read -r f; do files+=("$f"); done < <(find "$MINI/$d" -name '*.bas' 2>/dev/null | sort)
  done
fi

nb=0; ns=0; nt=0
for f in "${files[@]}"; do
  nt=$((nt+1))
  has_oracle "$f" && continue
  rel="${f#"$MINI"/}"
  out1="$(cd "$ROOT" && timeout 300 bash "$RUN" "../shared/minitests/$rel" bfx 2>/dev/null | norm)"
  rc=$?
  if [ $rc -ne 0 ]; then echo "SKIP $rel (exit $rc)"; ns=$((ns+1)); continue; fi
  if [ -z "$out1" ]; then echo "SKIP $rel (output vuoto)"; ns=$((ns+1)); continue; fi
  # SECONDA esecuzione per la stabilità (default ON: costa un compile, evita oracoli ballerini).
  # BACKFILL_RUNS=1 dimezza il tempo e lascia la validazione al run 4-gambe successivo.
  if [ "${BACKFILL_RUNS:-2}" = 2 ]; then
    out2="$(cd "$ROOT" && timeout 300 bash "$RUN" "../shared/minitests/$rel" bfx 2>/dev/null | norm)"
    if [ "$out1" != "$out2" ]; then echo "SKIP $rel (output NON stabile fra due run)"; ns=$((ns+1)); continue; fi
  fi
  if volatile "$out1"; then echo "SKIP $rel (path/valore volatile)"; ns=$((ns+1)); continue; fi
  if [ ${#out1} -gt 3000 ]; then echo "SKIP $rel (output troppo lungo: ${#out1} char)"; ns=$((ns+1)); continue; fi

  echo "BACKFILL $rel :: $out1"
  nb=$((nb+1))
  if [ "$APPLY" = 1 ]; then
    # BACKUP prima di modificare (AGENTS: mai toccare l'originale senza copia).
    mkdir -p "/tmp/gmb-minitest-backup/$(dirname "$rel")"
    cp "$f" "/tmp/gmb-minitest-backup/$rel"
    tmp="$(mktemp)"
    EXPECT_LINE="' EXPECT: $out1" awk '
      BEGIN { done = 0 }
      {
        if (!done && $0 !~ /^[[:space:]]*\047/ && $0 !~ /^[[:space:]]*$/) {
          print ENVIRON["EXPECT_LINE"]; done = 1
        }
        print
      }
      END { if (!done) print ENVIRON["EXPECT_LINE"] }
    ' "$f" > "$tmp" && cp "$tmp" "$f"
    rm -f "$tmp"
  fi
  rm -rf "$(dirname "$f")/bin"
done

echo "---"
echo "esaminati=$nt backfill=$nb skip=$ns$([ "$APPLY" = 1 ] && echo ' (APPLICATO)' || echo ' (dry-run)')"
