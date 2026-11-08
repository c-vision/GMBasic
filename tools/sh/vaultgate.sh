#!/usr/bin/env bash
# tools/sh/vaultgate.sh — gate: ogni nota del vault è linkata da almeno un indice.
#
# Uso:
#   bash tools/sh/vaultgate.sh                      # tutto il vault; exit 1 se resta un'orfana
#   bash tools/sh/vaultgate.sh --list               # elenca TUTTE le orfane (una per riga)
#   bash tools/sh/vaultgate.sh --scope Guide/Tools  # limita le note a una cartella (ripetibile)
#   bash tools/sh/vaultgate.sh --quiet              # solo la riga di esito
#   bash tools/sh/vaultgate.sh --vault DIR          # altro vault
#   bash tools/sh/vaultgate.sh --index FILE         # indice esplicito (ripetibile, disattiva l'auto)
#   bash tools/sh/vaultgate.sh --min-links N        # link minimi perché una nota sia indice (default 3)
#   bash tools/sh/vaultgate.sh --gen-index DIR      # (ri)genera DIR/_Indice-<...>.md, idempotente
#
# --gen-index: riscrive l'indice della cartella (ricorsivo, stesso formato degli indici esistenti).
# Se il contenuto calcolato è identico al file sul disco NON lo tocca — e la data nel file resta
# quella della prima generazione, così due run di fila producono lo stesso byte. Se invece il
# contenuto cambia, la versione precedente viene copiata in backup sotto
# ${TMPDIR}/gmbasicmonorepo-backup/vaultgate/<data>/<cartella>/ prima di essere sostituita.
#
# Default del vault: ~/Documents/AiWiki/01-Projects/GMBasicMonoRepo (il vault è per-macchina:
# non c'è un path relativo al repo che valga sempre). Nessuna variabile d'ambiente: i parametri
# passano solo dai flag.
#
# CHE COS'È UN "INDICE": una nota il cui BASENAME contiene il token Index/Indice delimitato
# da non-lettere — `_Index`, `_Index_GMBasicMonoRepo`, `Guide/_Index`, `Tools-Indice`,
# `GPM-Index`, `MLX-Modelli-Indice`, `Test-Index-and-Runners` — **e che linka davvero**
# (almeno --min-links, default 3, wikilink): il token da solo non basta, altrimenti
# `tools/.../libs_index_publish.md` (che cita 1-2 note) sarebbe un "indice" per il nome, e un
# indice che non linka nulla non copre nulla (`Knowledge/Skills-GMBasic-Architettura-e-Indice.md`).
# Per un vault di prova piccolo: --min-links 1.
# Indice LOCALE di cartella: vale in ogni cartella (anche Bugs/ o Todo/) la nota il cui basename
# COMINCIA col token, con un eventuale prefisso di underscore — `_Indice-Bugs`, `_Indice-Todo`,
# `Index` — perché una cartella di contenuto ha il suo indice accanto alle note che raccoglie.
# Il token in MEZZO al nome vale solo fuori dalle cartelle di contenuto: in Bugs/ Todo/ Memory/
# Decisions/ Hard-Lessons/ Piani/ è il titolo di un bug/nota (`(solved) - ...-Index-...`), non un
# indice. `_archive/` è escluso anche dall'insieme delle note (archivio storico: non va indicizzato).
# Gli indici NON sono conteggiati fra le note da coprire: sono radici, non contenuto.
#
# COSA CONTA COME LINK: `[[nota]]`, `[[nota|alias]]`, `[[nota#sezione]]`, `[testo](nota.md)`,
# anche con `%20` al posto degli spazi; il confronto è per basename senza `.md`.
#
# LIMITE NOTO (riportato in esito): se due note hanno lo stesso basename (oggi 7 casi, es.
# `README`, `Infrastruttura-GMBasic`, `FFI-Guida-Utente`) un link alla prima "copre" anche la
# seconda: il gate non può distinguerle. Vengono contate come AMBIGUE, non come orfane.
#
# Exit: 0 = nessuna orfana · 1 = almeno un'orfana · 2 = uso errato, vault assente, nessun indice.
set -uo pipefail

VAULT="$HOME/Documents/AiWiki/01-Projects/GMBasicMonoRepo"
DO_LIST=0
QUIET=0
SCOPES=()
IDX_OVERRIDE=()
GEN_INDEXES=()
MIN_LINKS=3

while [ $# -gt 0 ]; do
    case "$1" in
        --vault)  VAULT="${2:-}"; shift 2 ;;
        --scope)  IFS=',' read -r -a _s <<< "${2:-}"; for _x in "${_s[@]}"; do [ -n "$_x" ] && SCOPES+=("${_x%/}"); done; shift 2 ;;
        --index)  IDX_OVERRIDE+=("${2:-}"); shift 2 ;;
        --min-links) MIN_LINKS="${2:-}"; shift 2 ;;
        --gen-index) GEN_INDEXES+=("${2:?--gen-index vuole una cartella}"); shift 2 ;;

        --list)   DO_LIST=1; shift ;;
        --quiet)  QUIET=1; shift ;;
        -h|--help)
            awk 'NR>1 && /^#/ {sub(/^# ?/,""); print; next} NR>1 {exit}' "$0"
            exit 0 ;;
        *) echo "opzione sconosciuta: $1 (usa --help)" >&2; exit 2 ;;
    esac
done

[ -n "$VAULT" ] && [ -d "$VAULT" ] || { echo "ERRORE: vault non trovato: '$VAULT'" >&2; exit 2; }
VAULT="$(cd "$VAULT" && pwd)"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmb_vaultgate.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

# ── modo --gen-index: (ri)genera l'indice di una cartella, idempotente ────────
# Il nome del file si ricava dall'indice esistente della cartella (`_Indice-*.md`), altrimenti
# `_Indice-<percorso con / -> ->.md`; il formato è quello degli indici scritti a mano il 2026-11-06,
# così rigenerare non produce un diff di comodo.
if [ "${#GEN_INDEXES[@]}" -gt 0 ]; then
    BACKUP_ROOT="${TMPDIR:-/tmp}/gmbasicmonorepo-backup/vaultgate"
    rc=0
    for folder in "${GEN_INDEXES[@]}"; do
        dir="$VAULT/$folder"
        if [ ! -d "$dir" ]; then
            echo "ERRORE: cartella inesistente: $folder" >&2; rc=2; continue
        fi
        existing="$(find "$dir" -maxdepth 1 -type f -name '_Indice-*.md' | sort)"
        n_ex="$(printf '%s' "$existing" | grep -c . || true)"
        if [ "${n_ex:-0}" -gt 1 ]; then
            echo "ERRORE: più di un indice in $folder/: $(printf '%s' "$existing" | tr '\n' ' ')" >&2
            rc=2; continue
        fi
        if [ "${n_ex:-0}" -eq 1 ]; then
            name="$(basename "$existing")"
        else
            name="_Indice-$(printf '%s' "$folder" | sed 's|/|-|g').md"
        fi
        target="$dir/$name"
        top="$WORK/gen.top"; sub="$WORK/gen.sub"
        find "$dir" -maxdepth 1 -type f -name '*.md' ! -name "$name" | sed 's|.*/||' | sort > "$top"
        find "$dir" -mindepth 2 -type f -name '*.md' ! -name "$name" | sed "s|^$dir/||" | sort > "$sub"
        nsub="$(find "$dir" -mindepth 1 -type d | wc -l | tr -d ' ')"
        nstatus="$(grep -cE '^\((open|solved)\)' "$top" || true)"
        nlink=$(( $(wc -l < "$top") + $(wc -l < "$sub") ))
        today="$(date +%Y-%m-%d)"
        if [ -f "$target" ]; then
            old_date="$(sed -n 's/.*Generato il \([0-9][0-9-]*\).*/\1/p' "$target" | head -1)"
            [ -n "$old_date" ] && today="$old_date"
        fi
        {
            printf -- '---\ntags: [gmbasic, indice]\n---\n\n# %s — indice\n\n' "$(printf '%s' "$folder" | tr '/' ' ')"
            printf 'Indice delle **%s note** di `%s/` (ricorsivo). Generato il %s con `tools/sh/vaultgate.sh`:\n' "$nlink" "$folder" "$today"
            printf 'una nota nuova in questa cartella (o nelle sue sottocartelle) va aggiunta anche qui, altrimenti\n'
            printf 'il gate la segnala come orfana. I link usano il percorso dal vault: nessuna ambiguità con i\n'
            printf 'basename duplicati.\n\n'
            # sezioni: sottocartella, oppure stato per Bugs/Todo ((open)/(solved)/Altre note);
            # una cartella piatta senza stati resta un elenco unico, come gli indici scritti a mano.
            awk -v F="$folder" -v H="$nsub" -v NS="${nstatus:-0}" '
                {
                    rel = $0; k = split(rel, p, "/")
                    if (k > 1) sec = p[1] "/"
                    else if (rel ~ /^\(open\)/) sec = "(open)"
                    else if (rel ~ /^\(solved\)/) sec = "(solved)"
                    else sec = ""
                    if ((H > 0 || NS > 0) && sec != prev) {
                        if (prev != "") print ""
                        print "## " (sec == "" ? "Altre note" : sec)
                        print ""; prev = sec
                    }
                    t = rel; sub(/\.md$/, "", t); gsub(/\|/, "\\\\|", t)
                    print "- [[" F "/" t "]]"
                }' "$top" "$sub"
        } > "$WORK/gen.md"

        if [ -f "$target" ] && cmp -s "$WORK/gen.md" "$target"; then
            echo "invariato: $folder/$name ($nlink link)"
            continue
        fi
        if [ -f "$target" ]; then
            bk="$BACKUP_ROOT/$(date +%Y%m%d-%H%M%S)/$folder"
            mkdir -p "$bk" && cp "$target" "$bk/"
            echo "backup: $bk/$name"
        fi
        mv "$WORK/gen.md" "$target" && chmod 644 "$target"
        echo "scritto: $folder/$name ($nlink link)"
    done
    exit "$rc"
fi

# ── 1. note del vault (archivio e cartelle di servizio escluse) ────────────────
find "$VAULT" -type f -name '*.md' \
     -not -path '*/.git/*' -not -path '*/.obsidian/*' \
     -not -path "$VAULT/_archive/*" \
     | sort > "$WORK/notes.abs"
NOTES_ALL="$(wc -l < "$WORK/notes.abs" | tr -d ' ')"
[ "$NOTES_ALL" -gt 0 ] || { echo "ERRORE: nessuna nota .md in $VAULT" >&2; exit 2; }

# scope: prefissi (relativi al vault) su cui vale l'invariante; gli indici restano globali
awk -v V="$VAULT/" '{
    rel = $0; sub("^" V, "", rel); print rel
}' "$WORK/notes.abs" > "$WORK/notes.rel"

if [ "${#SCOPES[@]}" -gt 0 ]; then
    printf '%s\n' "${SCOPES[@]}" > "$WORK/scopes.txt"
    awk 'NR==FNR { n++; p[n] = $0; next }
         { for (i = 1; i <= n; i++) if (index($0, p[i]) == 1) { print; next } }' \
        "$WORK/scopes.txt" "$WORK/notes.rel" > "$WORK/inscope.txt"
else
    cp "$WORK/notes.rel" "$WORK/inscope.txt"
fi
NOTES="$(wc -l < "$WORK/inscope.txt" | tr -d ' ')"

# ── 2. indici (auto-detect sul vault intero, o --index espliciti) ──────────────
case "$MIN_LINKS" in ''|*[!0-9]*) echo "ERRORE: --min-links vuole un numero: '$MIN_LINKS'" >&2; exit 2 ;; esac

if [ "${#IDX_OVERRIDE[@]}" -gt 0 ]; then
    printf '%s\n' "${IDX_OVERRIDE[@]}" > "$WORK/idx.rel"
else
    awk '{
        rel = $0
        n = split(rel, part, "/")
        top = part[1]
        base = tolower(part[n]); sub(/\.md$/, "", base)
        # indice locale di cartella: il token apre il nome -> vale in ogni cartella
        if (base ~ /^_*(index|indice)([^a-z]|$)/) { print rel; next }
        # token in mezzo al nome: in una cartella di contenuto e il titolo di un bug/nota
        if (top == "Bugs" || top == "Todo" || top == "Memory" || top == "Decisions" \
            || top == "Hard-Lessons" || top == "Piani") next
        if (base ~ /(^|[^a-z])(index|indice)([^a-z]|$)/) print rel
    }' "$WORK/notes.rel" | sort > "$WORK/idx.cand"
    # il token non basta: un indice linka. Sotto soglia è una nota che parla d'indici.
    : > "$WORK/idx.rel"
    while IFS= read -r rel; do
        c="$(grep -o '\[\[' "$VAULT/$rel" 2>/dev/null | wc -l | tr -d ' ')"
        [ "${c:-0}" -ge "$MIN_LINKS" ] && printf '%s\n' "$rel" >> "$WORK/idx.rel"
    done < "$WORK/idx.cand"
    sort -o "$WORK/idx.rel" "$WORK/idx.rel"
fi
NIDX="$(wc -l < "$WORK/idx.rel" | tr -d ' ')"
if [ "$NIDX" -eq 0 ]; then
    echo "ERRORE: nessun indice trovato (serve una nota con token Index/Indice, o --index FILE)" >&2
    exit 2
fi
while IFS= read -r rel; do
    [ -f "$VAULT/$rel" ] || { echo "ERRORE: indice inesistente: $rel" >&2; exit 2; }
done < "$WORK/idx.rel"

# ── 3. link emessi dagli indici, normalizzati a basename senza .md ────────────
: > "$WORK/links.raw"
while IFS= read -r rel; do
    f="$VAULT/$rel"
    grep -oE '\[\[[^]]+\]\]' "$f" 2>/dev/null \
        | sed -e 's/^\[\[//' -e 's/\]\]$//' -e 's/\\|.*//' -e 's/|.*//' >> "$WORK/links.raw"
    grep -oE '\]\([^)]*\.md[^)]*\)' "$f" 2>/dev/null \
        | sed -e 's/^](//' -e 's/)$//' -e 's/^<//' -e 's/>$//' >> "$WORK/links.raw"
done < "$WORK/idx.rel"

awk '{
    t = $0
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", t)
    sub(/^\.\//, "", t)
    sub(/[?#].*$/, "", t)
    n = split(t, p, "/"); b = p[n]
    sub(/\.md$/, "", b)
    gsub(/%20/, " ", b)
    if (b != "") print b
}' "$WORK/links.raw" | sort -u > "$WORK/linked.txt"
NLINKED="$(wc -l < "$WORK/linked.txt" | tr -d ' ')"

# ── 4. orfane: basename della nota assente da ogni indice ─────────────────────
# gli indici sono esclusi: un indice non deve linkare se stesso per essere "coperto"
awk 'NR == FNR { idx[$0] = 1; next } !($0 in idx)' "$WORK/idx.rel" "$WORK/inscope.txt" > "$WORK/check.txt"
NOTES="$(wc -l < "$WORK/check.txt" | tr -d ' ')"
awk '{ rel = $0; base = rel; sub(/.*\//, "", base); sub(/\.md$/, "", base);
       printf "%s\t%s\n", base, rel }' "$WORK/check.txt" > "$WORK/notes.tsv"
awk -F'\t' 'NR == FNR { seen[$0] = 1; next } !($1 in seen) { print $2 }' \
    "$WORK/linked.txt" "$WORK/notes.tsv" > "$WORK/orphans.txt"
NORPH="$(wc -l < "$WORK/orphans.txt" | tr -d ' ')"

# ambigue: basename ripetuto nel vault e coperto da un indice (link non distinguibile)
cut -f1 "$WORK/notes.tsv" | sort | uniq -d > "$WORK/dup.txt"
awk 'NR == FNR { seen[$0] = 1; next } ($0 in seen)' "$WORK/linked.txt" "$WORK/dup.txt" > "$WORK/dup.covered"
NDUP=0; [ -s "$WORK/dup.covered" ] && NDUP="$(wc -l < "$WORK/dup.covered" | tr -d ' ')"

# ── 5. esito ─────────────────────────────────────────────────────────────────
if [ "$QUIET" -eq 0 ]; then
    echo "vault: $VAULT"
    echo "indici: $NIDX — $(paste -sd, "$WORK/idx.rel" | sed 's/,/, /g')"
    [ "${#SCOPES[@]}" -gt 0 ] && echo "scope: ${SCOPES[*]}"
    echo "note: $NOTES (vault: $NOTES_ALL, escluso _archive/ e i $NIDX indici) · basename coperti: $NLINKED"
    if [ "$NORPH" -gt 0 ]; then
        echo "orfane: $NORPH"
        awk '{ d = $0; if (sub(/\/[^\/]*$/, "", d) == 0) d = "(root)"; print d }' "$WORK/orphans.txt" \
            | sort | uniq -c | sort -rn | sed 's/^/  /'
        if [ "$DO_LIST" -eq 1 ]; then
            echo "--- orfane (una per riga) ---"
            sed 's/^/  /' "$WORK/orphans.txt"
        else
            echo "  (usa --list per l'elenco completo)"
        fi
    fi
    [ "$NDUP" -gt 0 ] && echo "ambigue: $NDUP basename duplicati coperti da un indice (link non distinguibile): $(paste -sd, "$WORK/dup.covered" | sed 's/,/, /g')"
fi

if [ "$NORPH" -gt 0 ]; then
    [ "$QUIET" -eq 0 ] && echo "ESITO: FALLITO — $NORPH note non linkate da nessun indice"
    [ "$QUIET" -eq 1 ] && echo "vaultgate: $NOTES note, $NORPH orfane, $NIDX indici — FALLITO"
    exit 1
fi
[ "$QUIET" -eq 0 ] && echo "ESITO: OK — ogni nota è linkata da almeno un indice"
[ "$QUIET" -eq 1 ] && echo "vaultgate: $NOTES note, 0 orfane, $NIDX indici — OK"
exit 0
