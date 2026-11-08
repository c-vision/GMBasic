#!/usr/bin/env bash
# GMBASIC — hook pre-commit: pacchetti coerenti con la loro copia in cache e con le loro lib.
#
# PERCHE'. Due modi di committare un pacchetto che poi NON e' quello che gira, entrambi con
# sintomi che sembrano bug del compilatore:
#  1) CACHE. `~/.gmbasic/packages` e' una COPIA dei pacchetti del repo. Se resta indietro, un
#     export nuovo risulta "non definito", un `.inc` vecchio convive col nuovo e l'emissione
#     fallisce con "la procedura 'X' con N parametri e' gia' definita"; gli esempi girano con
#     semantica diversa dal repo.
#  2) LIB. Se non si riesegue `build.sh`, la `lib/<rid>/lib<nome>.*` resta la VECCHIA: l'API
#     BASIC c'e' (`.inc` aggiornato) ma il simbolo nuovo manca e l'errore arriva lontano come
#     "simbolo non trovato" o come valore sbagliato.
#
# COSA FA. Guarda i file STAGED del commit: se non toccano `packages/` termina subito
# (costo zero); se ne toccano, applica ai SOLI pacchetti coinvolti i due controlli canonici,
# entrambi gia' usati dal gate `tools/sh/tools_verify.sh`:
#  - cache: `tools/sh/gmb_pkg_cache_sync.sh --check` sui pacchetti INSTALLATI (uno mai
#    installato non e' "stale": semplicemente non c'e');
#  - lib:   `tools/sh/gmb_pkg_libcheck.sh` (lib piu' vecchia dei sorgenti C).
# Ogni divergenza blocca il commit con il comando da eseguire. Lo script vive in `tools/`
# perche' `.git/hooks/` non e' versionato: lì finisce solo un blocco marcato che richiama `run`.
#
# Uso:
#   tools/sh/gmb_pkg_cache_hook.sh install     # installa/aggiorna il blocco nel pre-commit
#   tools/sh/gmb_pkg_cache_hook.sh uninstall   # rimuove il blocco (resto dell'hook intatto)
#   tools/sh/gmb_pkg_cache_hook.sh status      # dice se il blocco e' installato
#   tools/sh/gmb_pkg_cache_hook.sh run         # esegue il controllo (lo invoca l'hook)
#   tools/sh/gmb_pkg_cache_hook.sh run --staged <path...>   # prova, senza toccare l'index
#
# Exit: 0 = nulla da controllare / tutto coerente · 1 = cache divergente o lib stale ·
#       2 = uso errato.
set -u

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
MONO="$(cd "$SELF_DIR/../.." && pwd)"
SYNC="$SELF_DIR/gmb_pkg_cache_sync.sh"
LIBCHECK="$SELF_DIR/gmb_pkg_libcheck.sh"
CACHE="${HOME}/.gmbasic/packages"

MARK_BEGIN="# >>> gmb pkg cache (tools/sh/gmb_pkg_cache_hook.sh) >>>"
MARK_END="# <<< gmb pkg cache <<<"

usage() { awk 'NR>1 && /^#/ {sub(/^# ?/, ""); print; next} NR>1 {exit}' "$0"; }

hook_path() {
    local d
    d="$(cd "$MONO" && git rev-parse --git-path hooks 2>/dev/null)" || return 1
    case "$d" in /*) ;; *) d="$MONO/$d" ;; esac
    printf '%s/pre-commit' "$d"
}

block_text() {
    cat <<'BLOCK'
# >>> gmb pkg cache (tools/sh/gmb_pkg_cache_hook.sh) >>>
# Se il commit tocca packages/: la cache dei pacchetti deve essere allineata al workspace
# e la lib del pacchetto non deve essere piu' vecchia dei suoi sorgenti C (build.sh).
_gmb_root=$(git rev-parse --show-toplevel 2>/dev/null || true)
if [ -n "$_gmb_root" ] && [ -f "$_gmb_root/tools/sh/gmb_pkg_cache_hook.sh" ]; then
  bash "$_gmb_root/tools/sh/gmb_pkg_cache_hook.sh" run || exit 1
fi
# <<< gmb pkg cache <<<
BLOCK
}

cmd_install() {
    local hook tmp
    hook="$(hook_path)" || { echo "ERRORE: $MONO non e' un repository git" >&2; return 2; }
    mkdir -p "$(dirname "$hook")"
    tmp="$(mktemp "${TMPDIR:-/tmp}/gmbhook.XXXXXX")" || return 2
    if [ -f "$hook" ]; then
        # Conserva TUTTO il resto (es. no_python_guard), scartando un blocco precedente.
        awk -v b="$MARK_BEGIN" -v e="$MARK_END" '$0==b{skip=1;next} $0==e{skip=0;next} !skip' "$hook" > "$tmp"
    fi
    if [ ! -s "$tmp" ]; then printf '#!/usr/bin/env bash\n' > "$tmp"; fi
    printf '\n' >> "$tmp"
    block_text >> "$tmp"
    mv "$tmp" "$hook" || return 2
    chmod 755 "$hook"
    echo "installato: $hook"
}

cmd_uninstall() {
    local hook tmp
    hook="$(hook_path)" || return 2
    if [ ! -f "$hook" ] || ! grep -qF "$MARK_BEGIN" "$hook"; then
        echo "nessun blocco della cache da rimuovere ($hook)"
        return 0
    fi
    tmp="$(mktemp "${TMPDIR:-/tmp}/gmbhook.XXXXXX")" || return 2
    awk -v b="$MARK_BEGIN" -v e="$MARK_END" '$0==b{skip=1;next} $0==e{skip=0;next} !skip' "$hook" > "$tmp"
    mv "$tmp" "$hook" || return 2
    chmod 755 "$hook"
    echo "rimosso il blocco da $hook"
}

cmd_status() {
    local hook
    hook="$(hook_path)" || return 1
    if [ -f "$hook" ] && grep -qF "$MARK_BEGIN" "$hook"; then
        echo "hook INSTALLATO: $hook"
    else
        echo "hook NON installato ($hook): usa '$0 install'"
        return 1
    fi
}

# cmd_run <path...>  — i path arrivano dai file staged; l'opzione --staged serve ai test
# per non toccare l'index del repository (condiviso con altre sessioni).
# Due controlli, entrambi SOLO sui pacchetti toccati dal commit; entrambi riportati anche
# quando falliscono insieme (un giro solo, tutti i problemi in una volta).
cmd_run() {
    local staged_paths="" pkgs="" inst="" n out st rc=0
    if [ "${1:-}" = "--staged" ]; then
        shift
        staged_paths="$(printf '%s\n' "$@")"
    else
        staged_paths="$(cd "$MONO" && git diff --cached --name-only --diff-filter=ACMR 2>/dev/null)"
    fi
    [ -n "$staged_paths" ] || return 0

    pkgs="$(printf '%s\n' "$staged_paths" | awk -F/ '$1=="packages" && NF>=3 {print $2}' | sort -u)"
    [ -n "$pkgs" ] || return 0

    # 1) cache utente = copia del workspace (solo per i pacchetti presenti in cache)
    for n in $pkgs; do
        [ -d "$CACHE/$n" ] && inst="$inst $n"
    done
    inst="$(printf '%s' "$inst" | sed 's/^ *//')"
    if [ -n "$inst" ]; then
        if [ ! -f "$SYNC" ]; then
            echo "ATTENZIONE: $SYNC assente: controllo della cache saltato." >&2
        else
            out="$(bash "$SYNC" --check --quiet --cache "$CACHE" $inst 2>&1)"; st=$?
            if [ "$st" -ne 0 ]; then
                rc=1
                {
                    echo ""
                    echo "ERRORE: cache dei pacchetti NON allineata al workspace."
                    printf '%s\n' "$out" | grep '^DIVERGE' | sed 's/^DIVERGE/  -/'
                    echo ""
                    echo "La cache e' una copia: se resta indietro gli errori sembrano bug del compilatore"
                    echo "(export \"non definito\", \"la procedura 'X' ... e' gia' definita\"). Allinea e ricommitta:"
                    echo "  bash tools/sh/gmb_pkg_cache_sync.sh"
                } >&2
            fi
        fi
    fi

    # 2) lib del pacchetto non piu' vecchia dei sorgenti C (build.sh rieseguito)
    if [ ! -f "$LIBCHECK" ]; then
        echo "ATTENZIONE: $LIBCHECK assente: controllo delle lib saltato." >&2
        return "$rc"
    fi
    out="$(bash "$LIBCHECK" --quiet --root "$MONO" $pkgs 2>&1)"; st=$?
    if [ "$st" -ne 0 ]; then
        rc=1
        {
            echo ""
            echo "ERRORE: lib dei pacchetti NON ricostruita dopo la modifica ai sorgenti C."
            printf '%s\n' "$out" | awk -F'\t' '$1=="STALE" {printf "  - %s: %s più vecchia di %s\n", $2, $3, $4}'
            echo ""
            echo "La lib e' quella vecchia: il simbolo nuovo manca e l'errore arriva lontano"
            echo "(\"simbolo non trovato\", comportamento vecchio, ABI disallineata). Ricostruisci e ricommitta:"
            printf '%s\n' "$out" | awk -F'\t' '$1=="STALE" {print $2}' | sort -u | sed 's|^|  bash packages/|; s|$|/build.sh|'
            echo "  oppure in un solo comando (build + riallineamento cache):"
            echo "  bash tools/sh/gmb_pkg_cache_sync.sh --rebuild"
        } >&2
    fi
    return "$rc"
}

case "${1:-}" in
    install)            cmd_install ;;
    uninstall)          cmd_uninstall ;;
    status)             cmd_status ;;
    run)                shift; cmd_run "$@" ;;
    -h|--help)          usage; exit 0 ;;
    *)                  usage; exit 2 ;;
esac
