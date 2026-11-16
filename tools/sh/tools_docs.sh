#!/usr/bin/env bash
# tools/sh/tools_docs.sh — allinea le SCHEDE dei tool (vault `Guide/Tools/`) al loro header.
#
# PERCHÉ ESISTE: le schede del vault erano scritte a mano una per una, e lo «Scopo» era la
# PRIMA RIGA del commento d'intestazione — quindi troncato a metà frase (es. la scheda di
# `check_minitests.sh` si fermava a «... al 2026-10-24: 36»). Qui lo «Scopo» è il PRIMO
# PARAGRAFO INTERO del commento (righe consecutive fino alla riga di commento vuota), con gli
# spazi collassati; l'«Intestazione del tool» è il blocco di commento verbatim dal sorgente.
# Lo script riscrive SOLO quelle due sezioni: frontmatter, «Note» e tutto il resto della
# scheda restano com'erano, quindi una scheda con note scritte a mano non viene calpestata.
#
# Uso:
#   bash tools/sh/tools_docs.sh                 # --check: elenca le schede divergenti (exit 1)
#   bash tools/sh/tools_docs.sh --write         # riscrive le schede divergenti (backup prima)
#   bash tools/sh/tools_docs.sh --only minitest # limita a un tool (sottostringa del nome file)
#   bash tools/sh/tools_docs.sh --vault DIR     # altro vault (default: ~/Documents/AiWiki/01-Projects/GMBasic)
#   bash tools/sh/tools_docs.sh --quiet         # solo il riepilogo
#
# Copre `tools/basic/*.bas`, `tools/c/*.c`, `tools/sh/*.sh`. I progetti .NET (`tools/audit`,
# `tools/registry-export`) non hanno un header di commento unico e restano fuori.
#
# Uscita: 0 tutte allineate (o scritte) · 1 almeno una divergente in --check · 2 uso/vault.
# Nessuna variabile d'ambiente: i parametri passano solo dai flag.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VAULT="$HOME/Documents/AiWiki/01-Projects/GMBasic"
WRITE=0
QUIET=0
ONLY=""

while [ $# -gt 0 ]; do
    case "$1" in
        --write) WRITE=1; shift ;;
        --check) WRITE=0; shift ;;
        --only)  ONLY="${2:?--only vuole un nome}"; shift 2 ;;
        --vault) VAULT="${2:?--vault vuole una dir}"; shift 2 ;;
        --quiet) QUIET=1; shift ;;
        -h|--help) awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "$0"; exit 0 ;;
        *) echo "opzione sconosciuta: $1 (usa --help)" >&2; exit 2 ;;
    esac
done

[ -d "$VAULT/Guide/Tools" ] || { echo "ERRORE: schede non trovate in $VAULT/Guide/Tools" >&2; exit 2; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmb_tools_docs.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

# ── estrazione dal sorgente ────────────────────────────────────────────────────────────

kind_of() {  # <file> → sh|bas|c|""
    case "$1" in
        *.sh)  echo sh ;;
        *.bas) echo bas ;;
        *.c)   echo c ;;
        *)     echo "" ;;
    esac
}

# Blocco d'intestazione verbatim: le righe di commento iniziali (shebang incluso per gli .sh),
# fermandosi alla prima riga che non è commento.
header_of() {  # <file> <kind>
    awk -v kind="$2" -v q="'" '
        NR == 1 && kind == "sh" && $0 ~ /^#!/ { print; next }
        {
            ok = 0
            if (kind == "sh")       ok = ($0 ~ /^#/)
            else if (kind == "bas") ok = (substr($0, 1, 1) == q)
            else                    ok = ($0 ~ /^\/\*/ || $0 ~ /^\*/ || $0 ~ /^\/\//)
            if (!ok) exit
            print
        }' "$1"
}

# Scopo: si scartano lo shebang e la PRIMA riga di commento (il titolo "nome — descrizione"),
# poi si prende il PRIMO PARAGRAFO, cioè tutte le righe consecutive di commento non vuoto fino
# alla riga di commento vuota. Nessun taglio a metà frase: se il paragrafo è lungo, si prende
# tutto (era il difetto delle schede scritte a mano: si fermavano alla prima riga).
scopo_of() {  # <kind>  (header su stdin)
    awk -v kind="$1" -v q="'" '
        function body(l) {
            if (kind == "sh")       sub(/^#[ \t]?/, "", l)
            else if (kind == "bas") { if (substr(l, 1, 1) == q) l = substr(l, 2); sub(/^[ \t]/, "", l) }
            else {
                sub(/^\/\*+[ \t]?/, "", l); sub(/^\/\/[ \t]?/, "", l)
                sub(/^\*+[ \t]?/, "", l);    sub(/[ \t]*\*+\/[ \t]*$/, "", l)
            }
            gsub(/[ \t]+/, " ", l)
            sub(/^ /, "", l); sub(/ $/, "", l)
            return l
        }
        {
            l = body($0)
            if (l ~ /^!/) next                 # shebang
            if (!title) { if (l == "") next; title = 1; next }   # riga-titolo
            if (l == "") { if (n > 0) exit; next }
            out = (n > 0) ? out " " l : l
            n++
        }
        END { if (n > 0) print out }' 
}

# ── riscrittura delle due sezioni nella scheda ────────────────────────────────────────

render_card() {  # <scheda> <file-scopo> <file-header>  → stdout
    awk -v scf="$2" -v hdf="$3" '
        BEGIN { state = "pre" }
        state == "pre" {
            print
            if ($0 ~ /^## Scopo[ \t]*$/) state = "scopo"
            next
        }
        state == "scopo" {
            if ($0 ~ /^## Intestazione del tool/) {
                print ""
                while ((getline l < scf) > 0) print l
                close(scf)
                print ""
                print
                state = "fence"
            }
            next
        }
        state == "fence" {
            if ($0 ~ /^```[ \t]*$/) {
                print
                while ((getline l < hdf) > 0) print l
                close(hdf)
                state = "body"
            }
            next
        }
        state == "body" {
            if ($0 ~ /^```[ \t]*$/) { print; state = "post" }
            next
        }
        { print }
    ' "$1"
}

# ── ciclo sui tool ────────────────────────────────────────────────────────────────────

aligned=0; drift=0; written=0; skipped=0; missing=0
BK="${TMPDIR:-/tmp}/gmbasic-backup/tools-docs/$(date +%Y%m%d-%H%M%S)"

for f in "$ROOT"/tools/basic/*.bas "$ROOT"/tools/c/*.c "$ROOT"/tools/sh/*.sh; do
    [ -f "$f" ] || continue
    base="$(basename "$f")"; stem="${base%.*}"
    [ -n "$ONLY" ] && case "$base" in *"$ONLY"*) ;; *) continue ;; esac

    card="$VAULT/Guide/Tools/$stem.md"
    if [ ! -f "$card" ]; then
        missing=$((missing + 1))
        [ "$QUIET" = 0 ] && echo "SCHEDA ASSENTE $base (attesa: Guide/Tools/$stem.md)"
        continue
    fi
    # Solo le schede che hanno entrambe le sezioni generate: le altre restano intatte.
    if ! grep -q '^## Scopo[[:space:]]*$' "$card" || ! grep -q '^## Intestazione del tool' "$card"; then
        skipped=$((skipped + 1))
        [ "$QUIET" = 0 ] && echo "SALTATA $base (scheda senza le sezioni generate)"
        continue
    fi

    kind="$(kind_of "$f")"
    hdr="$WORK/hdr"; scp="$WORK/scp"; new="$WORK/new"
    header_of "$f" "$kind" > "$hdr"
    scopo_of "$kind" < "$hdr" > "$scp"
    if [ ! -s "$scp" ]; then
        skipped=$((skipped + 1))
        [ "$QUIET" = 0 ] && echo "SALTATA $base (header senza paragrafo descrittivo)"
        continue
    fi
    render_card "$card" "$scp" "$hdr" > "$new"
    # Una resa vuota o monca (marcatori non trovati) non deve mai sostituire la scheda.
    if [ ! -s "$new" ] || ! grep -q '^## Intestazione del tool' "$new"; then
        skipped=$((skipped + 1))
        [ "$QUIET" = 0 ] && echo "SALTATA $base (resa non valida: scheda invariata)"
        continue
    fi

    if cmp -s "$card" "$new"; then
        aligned=$((aligned + 1))
        continue
    fi

    if [ "$WRITE" = 1 ]; then
        mkdir -p "$BK"
        cp -p "$card" "$BK/$stem.md" || { echo "ERRORE backup $card" >&2; exit 2; }
        cat "$new" > "$card" || { echo "ERRORE scrittura $card" >&2; exit 2; }
        written=$((written + 1))
        [ "$QUIET" = 0 ] && echo "AGGIORNATA Guide/Tools/$stem.md (backup in $BK/)"
    else
        drift=$((drift + 1))
        [ "$QUIET" = 0 ] && echo "DIVERGENTE Guide/Tools/$stem.md"
    fi
done

[ "$QUIET" = 1 ] || echo "---------------------------------"
echo "schede: $aligned allineate · $drift divergenti · $written scritte · $skipped saltate · $missing assenti"
if [ "$WRITE" = 1 ]; then
    echo "tools_docs: OK (le schede divergenti sono state riscritte)"
    exit 0
fi
if [ "$drift" -gt 0 ]; then
    echo "tools_docs: DRIFT (rilancia con --write per allinearle)"
    exit 1
fi
echo "tools_docs: allineate"
exit 0
