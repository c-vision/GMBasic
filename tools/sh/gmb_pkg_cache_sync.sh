#!/usr/bin/env bash
# GMBASIC — Allinea la cache dei pacchetti (`~/.gmbasic/packages`) al workspace del monorepo.
#
# PERCHE'. La cache utente e' una COPIA dei pacchetti: quando il pacchetto evolve nel repo
# (export nuovi, `.inc` che passa da `UBOUND(p)` a `LEN(p)`, dylib ricostruita) la copia in
# cache resta indietro. Il sintomo non e' "libreria vecchia", sono errori molto piu' confusi:
#   - un export nuovo risulta "non definito" anche se il repo lo contiene;
#   - un `.inc` vecchio convive con quello nuovo e l'emitter fallisce con
#     "la procedura 'X' con N parametri e' gia' definita (stessa firma)";
#   - gli esempi girano su una semantica diversa da quella del repo.
# `gmb pkg install` non serve in questo caso: richiede un registry raggiungibile. La copia
# canonica e' `<monorepo>/packages/<nome>/<versione>/`, quindi l'allineamento corretto e' una
# copia VERIFICATA (questo script), non un download.
#
# Uso:
#   tools/sh/gmb_pkg_cache_sync.sh                  # allinea TUTTI i pacchetti del workspace
#   tools/sh/gmb_pkg_cache_sync.sh blockchain       # solo i pacchetti indicati
#   tools/sh/gmb_pkg_cache_sync.sh --check          # nessuna scrittura: exit 1 se diverge (CI)
#   tools/sh/gmb_pkg_cache_sync.sh --dry-run        # mostra cosa verrebbe copiato
#   tools/sh/gmb_pkg_cache_sync.sh --rebuild        # ricostruisce (build.sh) le lib STALE, poi allinea
#   tools/sh/gmb_pkg_cache_sync.sh --cache DIR      # cache alternativa (default ~/.gmbasic/packages)
#   tools/sh/gmb_pkg_cache_sync.sh --quiet          # solo problemi e riepilogo
#
# Garanzie:
#   - la copia e' verificata con `diff -r`: se non e' identica l'esito e' ERRORE, non "fatto";
#   - prima di sostituire una voce di cache esistente ne fa un BACKUP completo in
#     ${TMPDIR:-/tmp}/gmbasicmonorepo-backup/gmbasic-packages/<nome>-<ver>-<timestamp>/
#     con il CONTENUTO del pacchetto alla radice (niente livello <ver>: e' gia' nel nome;
#     per ripristinare si ricopia il contenuto in <cache>/<nome>/<ver>/).
#     (fuori da /tmp quando TMPDIR e' valorizzata: una copia di pacchetto dentro la cartella
#     dei sorgenti viene scansionata dalla discovery degli IMPORT e produce il "gia' definita");
#   - i pacchetti presenti in cache ma NON nel workspace non vengono toccati.
#   - gli artefatti di build/esecuzione (`bin/`, `work/`, `obj/`, `.build/`, `.git/`,
#     `node_modules/`) sono IGNORATI in confronto e copia: eseguire un sample li crea
#     dentro il pacchetto del workspace e non sono contenuto del pacchetto.
#
# REBUILD (--rebuild). L'ordine "build della lib, poi copia in cache" era da ricordare a mano:
# la build cambia il binario e la cache resta con quello vecchio (caso avif/1.0.0: lib fresca nel
# repo, cache divergente, hook rosso). Con --rebuild il comando fa entrambe le cose: chiede a
# gmb_pkg_libcheck.sh quali lib sono STALE, esegue packages/<nome>/<ver>/build.sh per ognuna,
# poi copia. Una build fallita ferma tutto (exit 2 + ultime righe del log): non copia una lib
# vecchia facendo finta di niente. Scrive, quindi non si combina con --check.
#
# Exit code: 0 = cache allineata (o allineata dallo script),
#            1 = divergenze rilevate in `--check` (nessuna scrittura),
#            2 = uso errato, pacchetto assente dal workspace, oppure errore di copia/backup/build.
set -u

MONO="$(cd "$(dirname "$0")/../.." && pwd)"
PKGROOT="$MONO/packages"
CACHE="${HOME}/.gmbasic/packages"
TMPBASE="${TMPDIR:-/tmp}"
BKROOT="${TMPBASE%/}/gmbasicmonorepo-backup/gmbasic-packages"
# Cartelle di ARTEFATTI (build/esecuzione): NON sono contenuto del pacchetto. Eseguire un
# sample crea `samples/bin/` e `samples/work/` DENTRO il pacchetto del workspace (`**/bin/`
# e' gia' in .gitignore): se entrassero nel confronto, ogni esecuzione di un sample farebbe
# risultare la cache "divergente" e il gate fallirebbe per finti motivi.
DIFF_EXCL=(-x bin -x work -x obj -x .build -x .git -x node_modules)


MODE=sync     # sync | check | dry
QUIET=0
NAMES=()
REBUILD=0
# Le lib STALE le sa solo gmb_pkg_libcheck.sh: stessa regola del gate e dell'hook, una sola sede.
LIBCHECK="$MONO/tools/sh/gmb_pkg_libcheck.sh"

usage() {
    awk 'NR>1 && /^#/ {sub(/^# ?/, ""); print; next} NR>1 {exit}' "$0"
}

verb() { [ "$QUIET" -eq 1 ] || echo "$*"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --check)   MODE=check ;;
        --dry-run) MODE=dry ;;
        --rebuild) REBUILD=1 ;;
        --cache)   if [ $# -lt 2 ]; then echo "opzione --cache senza valore" >&2; exit 2; fi
                   CACHE="$2"; shift ;;
        --quiet)   QUIET=1 ;;
        -h|--help) usage; exit 0 ;;
        -*)        echo "opzione sconosciuta: $1" >&2; exit 2 ;;
        *)         NAMES+=("$1") ;;
    esac
    shift
done

[ -d "$PKGROOT" ] || { echo "ERRORE: cartella dei pacchetti assente ($PKGROOT)" >&2; exit 2; }
CACHE="${CACHE%/}"

# Una cartella di versione: inizia con una cifra e contiene un punto (0.1.0, 1.5.6).
# Serve a NON scambiare per versione le sottocartelle di servizio (include/, src/, test/).
is_version_dir() {
    case "$1" in
        [0-9]*.[0-9]*) return 0 ;;
        *)             return 1 ;;
    esac
}

same_tree() { diff -r "${DIFF_EXCL[@]}" "$1" "$2" >/dev/null 2>&1; }

copy_tree() {   # copy_tree <src> <dst> : copia esatta di src, senza gli artefatti
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --delete --exclude=bin/ --exclude=work/ --exclude=obj/ --exclude=.build/ --exclude=.git/ --exclude=node_modules/ "$1/" "$2/"
    else
        rm -rf "$2" && mkdir -p "$2" && cp -R "$1/." "$2/"
    fi
}

# Elenco dei pacchetti da trattare.
if [ ${#NAMES[@]} -gt 0 ]; then
    for n in "${NAMES[@]}"; do
        [ -d "$PKGROOT/$n" ] || { echo "ERRORE: pacchetto '$n' assente da $PKGROOT" >&2; exit 2; }
    done
    PKGS=("${NAMES[@]}")
else
    PKGS=()
    for d in "$PKGROOT"/*/; do
        [ -d "$d" ] || continue
        PKGS+=("$(basename "$d")")
    done
fi

# --rebuild: PRIMA della copia, ricostruisce con `build.sh` le lib che libcheck segnala STALE.
# L'ordine conta e non va ricordato: la build cambia il binario, e senza la copia la cache resta
# con quello vecchio (era il caso avif/1.0.0: lib fresca nel repo, cache divergente, hook rosso).
if [ "$REBUILD" -eq 1 ]; then
    if [ "$MODE" = check ]; then
        echo "ERRORE: --rebuild scrive (build + copia): non si combina con --check" >&2
        exit 2
    fi
    if [ ! -f "$LIBCHECK" ]; then
        echo "ERRORE: $LIBCHECK assente: non so quali lib ricostruire" >&2
        exit 2
    fi

    stale="$(bash "$LIBCHECK" --quiet ${PKGS[@]+"${PKGS[@]}"} 2>/dev/null)"
    targets="$(printf '%s\n' "$stale" | awk -F'\t' '$1=="STALE" {print $2}' | sort -u)"
    if [ -z "$targets" ]; then
        verb "rebuild: nessuna lib stale"
    fi
    for t in $targets; do
        bsh="$PKGROOT/$t/build.sh"
        if [ ! -f "$bsh" ]; then
            echo "ERRORE: lib stale in $t ma manca $bsh" >&2
            exit 2
        fi
        if [ "$MODE" = dry ]; then
            echo "rebuild $t  (bash packages/$t/build.sh)  [dry-run]"
            continue
        fi
        verb "rebuild $t ..."
        log="$(mktemp "${TMPDIR:-/tmp}/gmbbuild.XXXXXX")" || { echo "ERRORE: mktemp" >&2; exit 2; }
        if ! bash "$bsh" >"$log" 2>&1; then
            echo "ERRORE: build di $t fallita (bash packages/$t/build.sh) — ultime righe:" >&2
            tail -n 15 "$log" >&2
            exit 2
        fi
        rm -f "$log"
        verb "rebuild $t: ok"
    done
fi

synced=0; aligned=0; drift=0; errors=0

for pkg in "${PKGS[@]}"; do
    found=0
    for vdir in "$PKGROOT/$pkg"/*/; do
        [ -d "$vdir" ] || continue
        ver="$(basename "$vdir")"
        is_version_dir "$ver" || continue
        found=1
        src="${vdir%/}"
        dst="$CACHE/$pkg/$ver"

        if [ -d "$dst" ] && same_tree "$src" "$dst"; then
            aligned=$((aligned + 1))
            verb "ok      $pkg@$ver (gia' allineato)"
            continue
        fi

        case "$MODE" in
            check)
                drift=$((drift + 1))
                echo "DIVERGE $pkg@$ver  ($dst)"
                continue
                ;;
            dry)
                if [ -d "$dst" ]; then echo "copia   $pkg@$ver  (sostituisce $dst)";
                else echo "copia   $pkg@$ver  (nuovo $dst)"; fi
                synced=$((synced + 1))
                continue
                ;;
        esac

        if [ -d "$dst" ]; then
            ts="$(date +%Y%m%d-%H%M%S)"
            mkdir -p "$BKROOT" || { echo "ERRORE backup: $BKROOT" >&2; errors=$((errors + 1)); continue; }
            cp -R "$dst" "$BKROOT/$pkg-$ver-$ts" \
                || { echo "ERRORE backup di $dst" >&2; errors=$((errors + 1)); continue; }
            verb "backup  $dst -> $BKROOT/$pkg-$ver-$ts"
        fi

        mkdir -p "$dst" || { echo "ERRORE creazione $dst" >&2; errors=$((errors + 1)); continue; }
        if ! copy_tree "$src" "$dst"; then
            echo "ERRORE copia $pkg@$ver -> $dst" >&2
            errors=$((errors + 1))
            continue
        fi
        if same_tree "$src" "$dst"; then
            synced=$((synced + 1))
            verb "copia   $pkg@$ver -> $dst"
        else
            echo "ERRORE copia NON identica: $pkg@$ver (la cache resta divergente)" >&2
            errors=$((errors + 1))
        fi
    done
    if [ "$found" -eq 0 ]; then
        echo "ERRORE: nessuna cartella di versione in $PKGROOT/$pkg" >&2
        errors=$((errors + 1))
    fi
done

if [ "$MODE" = check ]; then
    if [ "$drift" -gt 0 ]; then
        echo "cache NON allineata: $drift da copiare, $aligned gia' allineate"
        exit 1
    fi
    echo "cache allineata: $aligned pacchetti"
    exit 0
fi

echo "copia: $synced, gia' allineati: $aligned, errori: $errors"
[ "$errors" -eq 0 ] || exit 2
exit 0
