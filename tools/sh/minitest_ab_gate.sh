#!/usr/bin/env bash
# GMBASIC — gate A/B zig vs clang sul SINGOLO MiniTest toccato (pre-commit).
#
# PERCHE'. Il toolchain canonico e' zig (`tools/sh/zcc`); `--cc cc` e' la gamba di CONFRONTO
# col clang di sistema. Le divergenze fra i due — un warning che zig promuove a errore, un
# `incompatible-pointer-types` che Apple clang nasconde, un ordinamento di link diverso — si
# scoprivano solo se qualcuno si ricordava di lanciare la gamba A/B a mano. L'unico controllo
# automatico era l'ordine dei link del tray dentro `tools/sh/test_minitests_c99native`. Qui il
# file toccato viene eseguito su ENTRAMBE le gambe e confrontato: la divergenza si vede al
# commit, non giorni dopo sul MiniTest di qualcun altro.
#
# COSA FA. Prende i file STAGED del commit; se nessuno e' un MiniTest termina subito (costo
# zero, nessun compilatore lanciato). Per ogni MiniTest toccato esegue il runner due volte —
# default (zig) e `--cc cc` — e confronta le righe di esito (`PASS <file> :: <output>|`,
# `FAIL <file> (...)`, `SKIP`) e ne ricava UNO di quattro verdetti, che non vanno confusi:
#   OK        le gambe concordano e il caso PASSA;
#   DIVERGE   le gambe danno esiti diversi (commit bloccato, con il diff delle due uscite);
#   FALLISCE  le gambe concordano NEL FALLIMENTO: nessuna divergenza zig/clang, ma il test e'
#             rotto o l'EXPECT e' sbagliato. Non e' un OK e blocca il commit: un gate che
#             stampasse "ok" qui certificherebbe un test rosso;
#   ERRORE    una gamba non ha eseguito NULLA (filtro o toolchain rotti): commit bloccato.
# Un caso SALTATO da entrambe le gambe non e' verificato e non blocca.
# Un `.bas` passato con un percorso che non si riconduce alla radice dei MiniTest — o che non
# esiste — NON e' un salto: e' un ERRORE d'invocazione (exit 2). Prima finiva in "salto … (non
# e' un MiniTest)" e il gate usciva 0: verde senza aver eseguito niente.
#
# LIMITI DICHIARATI.
#  - La gamba compila il file dal WORKING TREE, non dal blob staged: con uno stage parziale il
#    gate verifica cio' che e' su disco.
#  - I MiniTest con `' TAG: interactive` sono esclusi per progetto da entrambe le gambe; qui si
#    saltano PRIMA, altrimenti "saltato" diventerebbe "nessun file eseguito" (gate rosso).
#  - Il gate copre la gamba C (zig vs clang). La parita' IL/PCode/VM-C resta a
#    `tools/sh/test_minitests_parity.sh` e non e' compito di questo script.
#
# Uso:
#   tools/sh/minitest_ab_gate.sh install      # aggiunge il blocco al pre-commit (resto intatto)
#   tools/sh/minitest_ab_gate.sh uninstall    # rimuove solo il blocco
#   tools/sh/minitest_ab_gate.sh status       # dice se il blocco e' installato
#   tools/sh/minitest_ab_gate.sh run          # i MiniTest staged del commit (lo invoca l'hook)
#   tools/sh/minitest_ab_gate.sh run --staged <path...>   # prova, senza toccare l'index
#   tools/sh/minitest_ab_gate.sh check <file...>          # prova diretta, senza git (accetta
#                                                        shared/minitests/<dominio>/<file>.bas,
#                                                        <dominio>/<file>.bas o un assoluto;
#                                                        un .bas non valido = ERRORE, exit 2)
#   tools/sh/minitest_ab_gate.sh check --cc <cmd> <file...>
#
# Exit: 0 = nessuna divergenza (o nulla da fare) · 1 = divergenza / gamba muta · 2 = uso errato.
set -u

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
MONO="$(cd "$SELF_DIR/../.." && pwd)"
RUNNER="$SELF_DIR/test_minitests_c99native"
PLAIN_CC="cc"          # la gamba A/B: clang di sistema (Apple su macOS)
# Radice dei MiniTest: serve a verificare che il file passato a `check` ESISTA davvero (un needle
# che non corrisponde a nulla fa girare il runner su zero casi, cioe' un altro falso verde).
MINI_ROOT="${GMB_MINI_ROOT:-$MONO/shared/minitests}"
TMPD="$(mktemp -d "${TMPDIR:-/tmp}/minitest_ab.XXXXXX")" || exit 2
EXECUTED=0     # file realmente eseguiti su ENTRAMBE le gambe
BAD_INVOCATION=0  # file passati ma non riconducibili a un MiniTest: uso errato, non un salto
STRICT_PATH=0     # 1 solo in `check`: li' il percorso lo scrive l'utente e sbagliarlo deve fallire
SKIPPED=0      # file saltati (interattivi, non-MiniTest): non contano come verifica
DIVERGED=0     # le due gambe danno esiti DIVERSI
ACCORD_FAIL=0   # le due gambe concordano, ma nel FALLIMENTO: non e' un test verificato
trap 'rm -rf "$TMPD"' EXIT

MARK_BEGIN="# >>> gmb minitest A/B (tools/sh/minitest_ab_gate.sh) >>>"
MARK_END="# <<< gmb minitest A/B <<<"

usage() { awk 'NR>1 && /^#/ {sub(/^# ?/, ""); print; next} NR>1 {exit}' "$0"; }

hook_path() {
    local d
    d="$(cd "$MONO" && git rev-parse --git-path hooks 2>/dev/null)" || return 1
    case "$d" in /*) ;; *) d="$MONO/$d" ;; esac
    printf '%s/pre-commit' "$d"
}

block_text() {
    cat <<'BLOCK'
# >>> gmb minitest A/B (tools/sh/minitest_ab_gate.sh) >>>
# Se il commit tocca MiniTest: il file toccato gira su zig (canonico) e su `--cc cc` e le due
# uscite devono coincidere. Una divergenza fra i due toolchain blocca il commit.
_gmb_root=$(git rev-parse --show-toplevel 2>/dev/null || true)
if [ -n "$_gmb_root" ] && [ -f "$_gmb_root/tools/sh/minitest_ab_gate.sh" ]; then
  bash "$_gmb_root/tools/sh/minitest_ab_gate.sh" run || exit 1
fi
# <<< gmb minitest A/B <<<
BLOCK
}

cmd_install() {
    local hook tmp
    hook="$(hook_path)" || { echo "ERRORE: $MONO non e' un repository git" >&2; return 2; }
    mkdir -p "$(dirname "$hook")"
    tmp="$(mktemp "${TMPDIR:-/tmp}/gmbhook.XXXXXX")" || return 2
    if [ -f "$hook" ]; then
        # Conserva TUTTO il resto (no_python_guard, blocco della cache dei pacchetti),
        # scartando solo un blocco precedente di questo stesso gate.
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
        echo "nessun blocco A/B da rimuovere ($hook)"
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

# needle_of <path> — il needle (percorso relativo alla radice dei MiniTest, es. `misc/max.bas`)
# del file, SE e' un MiniTest che esiste davvero; altrimenti niente. Il filtro del runner e' una
# SOTTOSTRINGA di quel percorso: un needle che non corrisponde a un file reale fa eseguire al
# runner zero casi (in passato: falso verde). Tre passi, ognuno con un solo motivo per fallire:
needle_of() {
    local p="${1#"$MONO"/}"           # assoluto -> relativo al repo
    case "$p" in */minitests/*) p="${p#*minitests/}" ;; esac   # shared/minitests/x, assoluti
    case "$p" in */*.bas) ;; *) return 1 ;; esac               # solo <dir>/<file>.bas
    [ -f "$MINI_ROOT/$p" ] || return 1                          # e deve esistere
    printf '%s' "$p"
}

# Un MiniTest interattivo e' escluso da entrambe le gambe per progetto (`' TAG: interactive`):
# qui si salta prima, per non scambiare "saltato" con "nessun file eseguito".
is_interactive() {
    local p="$1"
    [ -f "$p" ] || return 1
    head -5 "$p" | grep -q "TAG: interactive"
}

# run_leg <compilatore|""> <needle> <out> — esegue UNA gamba sul singolo file.
# Il runner prende il filtro solo da GMB_MINI_ONLY (variabile PREESISTENTE del runner).
run_leg() {
    local cc="$1" needle="$2" out="$3"
    if [ -n "$cc" ]; then
        GMB_MINI_ONLY="$needle" bash "$RUNNER" --cc "$cc" > "$out" 2>&1
    else
        GMB_MINI_ONLY="$needle" bash "$RUNNER" > "$out" 2>&1
    fi
    printf '%s' "$?"
}

# result_lines <out> — le righe di esito, che sono cio' che i due toolchain devono avere uguale.
# Header (Compilatore:, tempi), log del broker e rumore del motore cambiano per costruzione.
result_lines() { grep -E '^(PASS|FAIL|SKIP) ' "$1" 2>/dev/null || true; }

# check_one <file> <cc> — 0 = gambe concordi, 1 = divergenza o gamba muta.
check_one() {
    local f="$1" cc="$2" needle rc_zig rc_cc t0 t1 a b
    if ! needle="$(needle_of "$f")"; then
        # In `run` i percorsi arrivano da git: un `.bas` fuori dalla radice dei MiniTest
        # (samples/, tools/basic/) non e' un errore, si salta — altrimenti il pre-commit
        # bloccherebbe commit che non toccano MiniTest. In `check` il percorso lo scrive
        # l'utente: un `.bas` sbagliato deve fallire, non sparire tra i SALTATO.
        if [ "$STRICT_PATH" = 1 ] && [ "${f##*.}" = bas ]; then
            echo "ERRORE: '$f' non e' un MiniTest esistente sotto $MINI_ROOT." >&2
            echo "        forme accettate: shared/minitests/<dominio>/<file>.bas | <dominio>/<file>.bas" >&2
            BAD_INVOCATION=$((BAD_INVOCATION+1))
            return 1
        fi
        echo "salto $f (non e' un MiniTest)"
        SKIPPED=$((SKIPPED+1))
        return 0
    fi
    if is_interactive "$MINI_ROOT/$needle"; then
        echo "salto $needle (interattivo: escluso per progetto da entrambe le gambe)"
        SKIPPED=$((SKIPPED+1))
        return 0
    fi
    a="$TMPD/zig.$$.txt"; b="$TMPD/ab.$$.txt"
    t0=$(date +%s)
    rc_zig="$(run_leg "" "$needle" "$a")"
    t1=$(date +%s)
    rc_cc="$(run_leg "$cc" "$needle" "$b")"
    local a_res b_res
    a_res="$TMPD/zig.$$.res"; b_res="$TMPD/cc.$$.res"
    result_lines "$a" > "$a_res"; result_lines "$b" > "$b_res"

    # Gamba MUTA: il filtro non ha eseguito nulla (nome sbagliato, file non trovato). Un
    # confronto fra due vuoti sarebbe un falso verde.
    if [ ! -s "$a_res" ]; then
        echo "ERRORE: la gamba zig non ha eseguito NULLA per '$needle' (rc=$rc_zig)" >&2
        tail -5 "$a" | sed 's/^/    /' >&2
        return 1
    fi
    if [ ! -s "$b_res" ]; then
        echo "ERRORE: la gamba $cc non ha eseguito NULLA per '$needle' (rc=$rc_cc)" >&2
        tail -5 "$b" | sed 's/^/    /' >&2
        return 1
    fi

    EXECUTED=$((EXECUTED+1))
    if diff -q "$a_res" "$b_res" > /dev/null; then
        # Due gambe che CONCORDANO non sono un test che PASSA. Se il MiniTest fallisce su
        # entrambe, il gate non ha verificato nulla: stampare OK sarebbe un falso verde, e un
        # commit con dentro un test rosso passerebbe "certificato" dall'A/B.
        if grep -q '^FAIL ' "$a_res"; then
            ACCORD_FAIL=$((ACCORD_FAIL+1))
            echo "FALLISCE $needle  (zig e $cc falliscono ALLO STESSO MODO: nessuna divergenza)" >&2
            sed 's/^/  esito | /' "$a_res" >&2
            return 1
        fi
        if ! grep -q '^PASS ' "$a_res"; then
            # Nessun PASS: il caso e' stato saltato da entrambe le gambe (non "verificato").
            SKIPPED=$((SKIPPED+1))
            echo "SALTATO  $needle  (nessuna gamba ha eseguito il caso: $(head -1 "$a_res" | cut -c1-60))"
            return 0
        fi
        echo "OK       $needle  (zig $(( t1 - t0 ))s, $cc: $(head -1 "$a_res" | cut -c1-48))"
        return 0
    fi

    DIVERGED=$((DIVERGED+1))
    echo "DIVERGE  $needle  (zig vs $cc)" >&2
    echo "  --- zig (canonico) ---" >&2
    sed 's/^/  zig  | /' "$a_res" >&2
    echo "  --- $cc ---" >&2
    sed 's/^/  cc   | /' "$b_res" >&2
    echo "  diff:" >&2
    diff -u "$a_res" "$b_res" | sed 's/^/  /' >&2
    return 1
}

# cmd_check <files...> — nessun git: serve per provare il gate a mano (o da un altro gate).
cmd_check() {
    local cc="$PLAIN_CC" f rc=0 n=0
    STRICT_PATH=1        # invocazione manuale: un percorso sbagliato e' un errore, non un salto
    if [ "${1:-}" = "--cc" ]; then cc="${2:-}"; shift 2; fi
    [ $# -gt 0 ] || { echo "ERRORE: check richiede almeno un file" >&2; return 2; }
    for f in "$@"; do
        check_one "$f" "$cc" || rc=1
    done
    if [ "$BAD_INVOCATION" -gt 0 ]; then
        echo "gate A/B: $BAD_INVOCATION file non verificabili — il percorso passato non e' un MiniTest valido (vedi ERRORE sopra)" >&2
        return 2
    fi
    if [ "$rc" -eq 0 ]; then
        echo "gate A/B: $EXECUTED passati su entrambe le gambe, zig == $cc (${SKIPPED:-0} saltati)"
    elif [ "$ACCORD_FAIL" -gt 0 ] && [ "$DIVERGED" -eq 0 ]; then
        echo "gate A/B: $ACCORD_FAIL falliti su entrambe le gambe — nessuna divergenza zig/$cc" >&2
    elif [ "$DIVERGED" -gt 0 ] && [ "$ACCORD_FAIL" -eq 0 ]; then
        echo "gate A/B: $DIVERGED divergenze zig/$cc" >&2
    else
        echo "gate A/B: $DIVERGED divergenze, $ACCORD_FAIL falliti su entrambe le gambe" >&2
    fi
    return "$rc"
}

# cmd_run [--staged <path...>] — i MiniTest del commit. Nessun MiniTest toccato = costo zero.
cmd_run() {
    local staged f rc=0
    if [ "${1:-}" = "--staged" ]; then
        shift
        staged="$(printf '%s\n' "$@")"
    else
        staged="$(cd "$MONO" && git diff --cached --name-only --diff-filter=ACMR 2>/dev/null)"
    fi
    [ -n "$staged" ] || return 0

    # Lista su file: un `case` dentro una command substitution multilinea viene rifiutato da
    # bash 3.2 (quello di macOS) con "syntax error near unexpected token".
    : > "$TMPD/list"
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        # needle_of fa gia' i tre controlli (forma <dir>/<file>.bas, esistenza sotto la radice,
        # percorso ricondotto): un `.bas` fuori dai MiniTest qui non arriva.
        needle_of "$f" > /dev/null || continue
        printf '%s\n' "$MONO/$f" >> "$TMPD/list"
    done <<EOF
$staged
EOF
    [ -s "$TMPD/list" ] || return 0

    echo "gate A/B (zig canonico vs $PLAIN_CC) sui MiniTest toccati:"
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        check_one "$f" "$PLAIN_CC" || rc=1
    done < "$TMPD/list"
    if [ "$rc" -ne 0 ]; then
        echo "" >&2
        if [ "$DIVERGED" -gt 0 ]; then
            echo "COMMIT BLOCCATO ($DIVERGED divergenze): le gambe non concordano. Il C e' lo" >&2
            echo "stesso, quindi la differenza e' di compilazione/link: va risolta nel codice o nel" >&2
            echo "gate A/B, non ignorata. Rieseguire a mano:  tools/sh/minitest_ab_gate.sh check <file>" >&2
        fi
        if [ "$ACCORD_FAIL" -gt 0 ]; then
            echo "COMMIT BLOCCATO ($ACCORD_FAIL MiniTest falliti su ENTRAMBE le gambe): qui non c'e'" >&2
            echo "nessuna divergenza zig/clang — il test e' rotto o ha un EXPECT sbagliato, e il gate" >&2
            echo "A/B non puo' certificare un caso che fallisce anche sul toolchain canonico." >&2
            echo "Sistemare il MiniTest, oppure non committarlo." >&2
        fi
        return 1
    fi
    echo "gate A/B: $EXECUTED MiniTest eseguiti su entrambe le gambe, zig == $PLAIN_CC (${SKIPPED:-0} saltati)"
    return 0
}

[ -f "$RUNNER" ] || { echo "ERRORE: runner assente ($RUNNER)" >&2; exit 2; }

cmd="${1:-}"; [ $# -gt 0 ] && shift
case "$cmd" in
    install)   cmd_install ;;
    uninstall) cmd_uninstall ;;
    status)    cmd_status ;;
    run)       cmd_run "$@" ;;
    check)     cmd_check "$@" ;;
    ""|-h|--help|help) usage; exit 0 ;;
    *) echo "minitest_ab_gate.sh: comando sconosciuto: $cmd (install/uninstall/status/run/check)" >&2; exit 2 ;;
esac
