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
#   bash tools/sh/tools_docs.sh --vault DIR     # altro vault (default: ~/Documents/AiWiki/01-Projects/GMBasicMonoRepo)
#   bash tools/sh/tools_docs.sh --quiet         # solo il riepilogo
#
# Copre `tools/basic/*.bas`, `tools/c/*.c`, `tools/sh/*.sh`, gli harness di `tools/sh` **senza
# estensione** (con shebang: `zcc`, `parity4`, `lxbuild`, …) e gli script `.pl`/`.awk` di
# `tools/sh` (header a `#`). I progetti .NET (`tools/audit`, `tools/registry-export`) non hanno
# un header di commento unico e restano fuori, come i dati (`tools/sh/macos-floor`).
#
# Due tool con lo STESSO stem (`x.c` + `x.sh` wrapper) condividono una scheda: si documenta il
# primo della lista (basic → c → sh, cioè l'implementazione) e il secondo è contato `alias`
# (prima la scheda "flappava" fra due header diversi a ogni giro).
# Una scheda scritta a mano (prosa, più ricca di un header) si dichiara col marcatore
# `scheda scritta a mano` e viene contata `manuale`, non rigenerata.
#
# Uscita: 0 tutte allineate (o scritte) · 1 divergenti, schede mancanti o da decidere · 2 uso/vault.
# Nessuna variabile d'ambiente: i parametri passano solo dai flag.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VAULT="$HOME/Documents/AiWiki/01-Projects/GMBasicMonoRepo"
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
        *.sh|*.pl|*.awk) echo sh ;;   # header a `#`: shell, perl e awk si leggono uguale
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
            else                    ok = ($0 ~ /^[ \t]*(\/\*|\*|\/\/)/)
            if (!ok) exit
            print
        }' "$1"
}

# Scopo: si scartano lo shebang e la PRIMA riga di commento (il titolo "nome — descrizione"),
# poi si prende il primo PARAGRAFO "descrittivo": le righe consecutive di commento non vuoto
# fino alla riga di commento vuota. I paragrafi che sono solo USO/ENV (iniziano con `Uso:`,
# `Env:`, un percorso `tools/…`, oppure una riga di `===`) non sono uno scopo: si passa al
# successivo, e se non ce n'è nessuno si tiene il primo trovato (non si perde contenuto).
scopo_of() {  # <kind>  (header su stdin)
    awk -v kind="$1" -v q="'" '
        function body(l) {
            sub(/^[ \t]+/, "", l)
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
        function is_usage(p) {
            return (p ~ /^(Uso|uso|USO):/) || (p ~ /^(Env|env|ENV):/) \
                || (p ~ /^tools\//) || (p ~ /^===+/) || (p ~ /^-+$/)
        }
        {
            l = body($0)
            if (l ~ /^!/) next                 # shebang
            if (!title) {                      # riga-titolo: "nome — descrizione"
                if (l == "") next
                title = 1
                t = l
                p = index(t, " — ")
                if (p > 0) titledesc = substr(t, p + length(" — "))
                else { p = index(t, " - "); if (p > 0) titledesc = substr(t, p + 3) }
                sub(/[.]*$/, "", titledesc)
                next
            }
            if (l == "") {
                if (n > 0) { np++; paras[np] = cur; n = 0; cur = "" }
                next
            }
            cur = (n > 0) ? cur " " l : l
            n++
        }
        END {
            if (n > 0) { np++; paras[np] = cur }
            # La riga-titolo (nome — descrizione) è la sintesi più affidabile: si preferisce
            # a un paragrafo che, in molti header, è un elenco puntato o un blocco di uso.
            if (titledesc != "" && !is_usage(titledesc)) { print titledesc; exit }
            for (i = 1; i <= np; i++) if (!is_usage(paras[i])) { print paras[i]; exit }
            if (titledesc != "") { print titledesc; exit }
            if (np >= 1) print paras[1]
        }'
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

aligned=0; drift=0; written=0; skipped=0; missing=0; manual=0; alias=0
SEEN=""
BK="${TMPDIR:-/tmp}/gmbasic-backup/tools-docs/$(date +%Y%m%d-%H%M%S)"

for f in "$ROOT"/tools/basic/*.bas "$ROOT"/tools/c/*.c "$ROOT"/tools/sh/*; do
    [ -f "$f" ] || continue
    base="$(basename "$f")"; stem="${base%.*}"
    # In tools/sh ci sono anche gli harness SENZA estensione (lxbuild, zcc, parity4, …): sono
    # tool, e si riconoscono dallo shebang. Restano fuori i dati (`macos-floor`) e i `.cmd`.
    case "$base" in
        *.bas|*.c|*.sh|*.pl|*.awk) ;;
        *.*) continue ;;
        *) head -1 "$f" | grep -q '^#!' || continue ;;
    esac
    [ -n "$ONLY" ] && case "$base" in *"$ONLY"*) ;; *) continue ;; esac

    card="$VAULT/Guide/Tools/$stem.md"
    # Due tool possono condividere lo stem (es. `x.c` + `x.sh` wrapper): una sola scheda.
    # Si documenta il PRIMO della lista (basic → c → sh, cioè l'implementazione) e il
    # secondo è un ALIAS dichiarato: senza questo la scheda "flappava" fra due header.
    if printf '%s\n' "$SEEN" | grep -qxF "$card"; then
        alias=$((alias + 1))
        [ "$QUIET" = 0 ] && echo "ALIAS $base (stessa scheda di un altro tool: Guide/Tools/$stem.md)"
        continue
    fi
    if [ ! -f "$card" ]; then
        missing=$((missing + 1))
        [ "$QUIET" = 0 ] && echo "SCHEDA ASSENTE $base (attesa: Guide/Tools/$stem.md)"
        continue
    fi
    SEEN="$SEEN$card
"
    # Le schede scritte a mano in prosa (più ricche di un header) si dichiarano con il
    # marcatore: senza, il tool le salterebbe e resterebbero indistinguibili da una scheda
    # rotta. Il marcatore si cerca SOLO fuori dall'«Intestazione del tool»: anche il sorgente
    # di QUESTO script ne parla, e la sua scheda lo cita verbatim (falso positivo trovato).
    if sed '/^## Intestazione del tool/,$d' "$card" | grep -qi 'scheda scritta a mano'; then
        manual=$((manual + 1))
        [ "$QUIET" = 0 ] && echo "MANUALE Guide/Tools/$stem.md (scheda scritta a mano: non rigenerata)"
        continue
    fi
    # Solo le schede che hanno entrambe le sezioni generate: le altre restano intatte.
    if ! grep -q '^## Scopo[[:space:]]*$' "$card" || ! grep -q '^## Intestazione del tool' "$card"; then
        skipped=$((skipped + 1))
        [ "$QUIET" = 0 ] && echo "SALTATA $base (scheda senza le sezioni generate)"
        continue
    fi

    case "$base" in
        *.sh|*.pl|*.awk) kind="sh" ;;   # header a `#`: shell, perl e awk si leggono uguale
        *.bas) kind="bas" ;;
        *.c)   kind="c" ;;
        *)     kind="sh" ;;             # harness senza estensione (già filtrato sullo shebang)
    esac
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
echo "schede: $aligned allineate · $drift divergenti · $written scritte · $skipped saltate · $missing assenti · $manual manuali · $alias alias"
if [ "$WRITE" = 1 ]; then
    echo "tools_docs: OK (le schede divergenti sono state riscritte)"
    exit 0
fi
if [ "$drift" -gt 0 ] || [ "$missing" -gt 0 ] || [ "$skipped" -gt 0 ]; then
    [ "$drift" -gt 0 ] && echo "tools_docs: DRIFT (rilancia con --write per allinearle)"
    [ "$missing" -gt 0 ] && echo "tools_docs: MANCANO $missing schede (una per tool in Guide/Tools/)"
    [ "$skipped" -gt 0 ] && echo "tools_docs: $skipped schede da decidere (sezioni generate assenti: manuale o template)"
    exit 1
fi
echo "tools_docs: allineate"
exit 0
