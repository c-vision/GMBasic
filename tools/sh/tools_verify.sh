#!/usr/bin/env bash
# tools/sh/tools_verify.sh — verifiche statiche/build del contenuto di tools/.
#
# Provenienza: era `migration/verify.sh` (unico punto d'ingresso dei controlli di
# migration/, quando i tool erano sparsi in tools-unclassified/). Dopo la consolidazione
# del 2026-10-29 esiste UNA sola sede (`tools/` alla radice) e migration/tools-unclassified
# non esiste piu': i gruppi che la usavano (build:audit, audit) sono stati rimossi —
# restano i due che hanno senso sul layout attuale.
#
# Uso:
#   bash tools/sh/tools_verify.sh            # tutti i gruppi (build + statici + pacchetti + conteggi)
#   bash tools/sh/tools_verify.sh --list     # elenca i gruppi e ciò che verificano
#   bash tools/sh/tools_verify.sh --no-syntax   # salta i controlli statici
#   bash tools/sh/tools_verify.sh --no-pkgcache # salta il controllo della cache pacchetti
#   bash tools/sh/tools_verify.sh --no-pkgdylib # salta il controllo delle dylib dei pacchetti
#   bash tools/sh/tools_verify.sh --no-readme   # salta i conteggi della sezione Structure
#   bash tools/sh/tools_verify.sh --no-termini  # salta il gate terminologico del manuale
#   bash tools/sh/tools_verify.sh --no-surface  # salta il gate delle superfici del manuale
#   bash tools/sh/tools_verify.sh --c99native DIR  # compila un'altra copia del C99Native
#                                         # (serve a validare il gruppo build)
#
# Exit: 0 = nessun FAIL · 1 = almeno un FAIL · 2 = nessun FAIL ma un gruppo non
#       eseguibile per setup mancante (es. cc assente) o uso errato.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOOLS="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT="$(cd "$TOOLS/.." && pwd)"

RUN_SYNTAX=1
RUN_CACHE=1
RUN_DYLIB=1
RUN_README=1
RUN_TERMINI=1
RUN_SURFACE=1
C99NATIVE="$ROOT/GMBasic/native/gmb_c99native"
while [ $# -gt 0 ]; do
    case "$1" in
        --list)
            cat <<'EOF'
Gruppi di verifica di tools/

  build     Compila il backend C99Native (src/gmb_c99_emit*.c + driver) con cc -c in una
            dir temporanea. Intercetta le rotture di build prima dei test. Non configura
            CMake e non scrive oggetti nel repo.
  statici   Sintassi di tutti gli script di tools/: bash -n su ogni .sh e compile()
            su ogni .py. Non esegue nulla, non tocca il repo, nessun artefatto.
  opzioni   Confronta il MIRROR C# del registro opzioni (OptionCoreTable in IlRuntime.cs) col
            canonico C (g_opt_core[] in gmb_opts.c): nomi, ordine e descrizioni devono essere
            identici, perché `AI LIST OPTIONS` è una superficie delle 4 gambe. Al primo scarto
            stampa il diff esatto della voce (testo C, testo C#, primo carattere diverso).
            Esegue tools/sh/check_option_registry.sh e legge solo il suo esito.
  cache     Confronta la cache utente dei pacchetti (~/.gmbasic/packages) col workspace:
            un pacchetto INSTALLATO e rimasto indietro fa fallire il gate. Serve a non
            rincorrere gli errori confusi da cache stale (export "non definito",
            "la procedura 'X' ... è già definita", semantica diversa dal repo).
            I pacchetti NON installati non contano: non sono "stale", mancano.
  dylib     Confronta le lib dei pacchetti (lib/<rid>/*.dylib|.so|.dll) con i loro
            sorgenti C: una lib piu' vecchia di src/*.c o include/*.h non e' stata
            ricostruita e fa fallire il gate (l'API BASIC c'e' ma il simbolo nuovo manca).
  readme    Confronta le cifre della sezione Structure di tools/README.md con i file
            reali sul disco (basic/ *.bas, c/ *.c, sh/ *.sh + harness senza estensione,
            data/ file, fixtures/ *.bas): una cifra stantia fa fallire il gate, e una
            cartella con cifra e senza regola di conteggio è un FAIL esplicito.
  termini   Forme vietate del manuale fissate in docs/manual/stile.md (numero di backend,
            parity5, C99Backend, superficie 982, PUL open source, percorsi morti, cache
            legacy). Esegue tools/sh/check_manual_terms.sh e legge solo il suo esito.
  superfici Capitoli rigenerati in docs/manual/capitoli: tabelle con riga di intestazione,
            nessuna riga `|` isolata, blocchi di codice non vuoti e sempre chiusi. Esegue
            tools/sh/check_manual_surface.sh e legge solo il suo esito.

Controlli NON coperti qui (esistono come script propri, invocabili singolarmente):
  tools/sh/check_file_size.sh          dimensioni file + escape illegali BASIC
  tools/sh/vaultgate.sh                ogni nota del vault linkata da un indice (vault esterno, exit 1 sulle orfane)
  tools/sh/check_minitests.sh          richiede la CLI nativa buildata e N run MiniTest
  tools/sh/test_minitests_parity       parity 4 gambe (chirurgica: un file per volta)
EOF
            exit 0 ;;
        --no-syntax) RUN_SYNTAX=0; shift ;;
        --no-pkgcache) RUN_CACHE=0; shift ;;
        --no-pkgdylib) RUN_DYLIB=0; shift ;;
        --no-readme) RUN_README=0; shift ;;
        --no-termini) RUN_TERMINI=0; shift ;;
        --no-surface) RUN_SURFACE=0; shift ;;
        --c99native) C99NATIVE="${2:-}"; shift 2 ;;
        -h|--help)   awk 'NR>1 && /^#/ {sub(/^# ?/,""); print; next} NR>1 {exit}' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "opzione sconosciuta: $1 (usa --list)" >&2; exit 2 ;;
    esac
done

[ -d "$TOOLS" ] || { echo "FATAL: tools/ non trovato: $TOOLS" >&2; exit 2; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmb_migration_verify.XXXXXX")"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

RESULTS="$WORK/results"; : > "$RESULTS"
FAILS=0
SETUPS=0

record() {  # record <PASS|FAIL|SKIP|INFO> <nome> <dettaglio>
    printf '%s\t%s\t%s\n' "$1" "$2" "$3" >> "$RESULTS"
    case "$1" in
        FAIL) FAILS=$((FAILS + 1)) ;;
        SKIP) SETUPS=$((SETUPS + 1)) ;;
    esac
}

# ── gruppo: build ─────────────────────────────────────────────────────────────
# Compila il C99Native (emitter + driver) a oggetto in una dir temporanea: non usa CMake,
# non tocca le build dir condivise (contesa con altre AI) e non lascia artefatti nel repo.
group_build_c99native() {
    if ! command -v cc >/dev/null 2>&1; then
        record SKIP "build:C99Native" "cc non nel PATH"
        return
    fi
    if [ ! -d "$C99NATIVE/src" ]; then
        record SKIP "build:C99Native" "sorgenti non trovati: $C99NATIVE/src"
        return
    fi

    local inc=(-I"$C99NATIVE/include"
               -I"$C99NATIVE/../gmb_compiler_c/frontend/include"
               -I"$C99NATIVE/../gmb_runtime_c/include"
               -I"$C99NATIVE/../gmb_bytecode/include")
    local objdir="$WORK/obj"
    mkdir -p "$objdir"

    local n=0 bad=0 f detail=""
    while IFS= read -r f; do
        n=$((n + 1))
        if ! cc -c "${inc[@]}" "$f" -o "$objdir/$(basename "$f" .c).o" 2>"$WORK/cc.err"; then
            bad=$((bad + 1))
            detail="$detail  $(basename "$f"): $(grep -m1 -E 'error' "$WORK/cc.err" || head -1 "$WORK/cc.err")"$'\n'
        fi
    done < <(find "$C99NATIVE/src" "$C99NATIVE/driver" -type f -name '*.c' | sort)

    if [ "$bad" -eq 0 ]; then
        record PASS "build:C99Native" "$n file compilati (emitter + driver)"
    else
        detail="${detail%$'\n'}"
        record FAIL "build:C99Native" "$bad/$n file in errore"$'\n'"$detail"
    fi
}

# ── gruppo: statici ───────────────────────────────────────────────────────────
group_syntax() {
    local total=0 bad=0 f rel msg detail=""
    while IFS= read -r f; do
        total=$((total + 1))
        if ! bash -n "$f" 2>"$WORK/syntax.err"; then
            bad=$((bad + 1))
            rel="$(printf '%s' "$f" | sed "s|^$TOOLS/||")"
            msg="$(head -1 "$WORK/syntax.err" | sed "s|$TOOLS/||g")"
            detail="$detail  $rel: ${msg#"$rel": }"$'\n'
        fi
    done < <(find "$TOOLS" -type f -name '*.sh' | sort)

    local py=0 pybad=0
    while IFS= read -r f; do
        py=$((py + 1))
        pybad=$((pybad + 1))
        rel="$(printf '%s' "$f" | sed "s|^$TOOLS/||")"
        detail="$detail  $rel: tool Python vietato (policy: niente Python in tools/)"$'\n'
    done < <(find "$TOOLS" -type f -name '*.py' | sort)

    local sb="$total script shell"
    [ "$py" -gt 0 ] && sb="$sb, $py Python (VIETATI)"
    if [ "$bad" -eq 0 ] && [ "$pybad" -eq 0 ]; then
        record PASS "statici" "$sb — sintassi valida"
    else
        detail="${detail%$'\n'}"          # niente riga vuota in coda
        record FAIL "statici" "$sb — $((bad + pybad)) con errori di sintassi"$'\n'"$detail"
    fi
}

# ── gruppo: registro opzioni (mirror C# ↔ canonico C) ──
# `AI LIST OPTIONS` è una superficie delle 4 gambe e il MiniTest `ai/ai_list_options` ne
# confronta il testo: `OptionCoreTable` (IlRuntime.cs) deve replicare `g_opt_core[]`
# (gmb_opts.c) nome per nome, nello stesso ordine e con lo stesso testo. La voce
# `debug.memory` è già mancata due volte e il difetto si vedeva solo eseguendo la parità.
# Il confronto e il diff esatto della voce sono del tool dedicato: qui si legge solo l'esito.
group_option_registry() {
    local tool="$SCRIPT_DIR/check_option_registry.sh" out rc
    if [ ! -f "$tool" ]; then
        record SKIP "opzioni:registro" "check_option_registry.sh non trovato in $SCRIPT_DIR"
        return
    fi
    out="$(bash "$tool" --quiet 2>&1)"; rc=$?
    case "$rc" in
        0) record PASS "opzioni:registro" "OptionCoreTable identica a g_opt_core[] (nomi, ordine, descrizioni)" ;;
        1) record FAIL "opzioni:registro" "mirror C# divergente dal canonico C"$'\n'"$out" ;;
        *) record SKIP "opzioni:registro" "gate non eseguibile (rc=$rc)"$'\n'"$out" ;;
    esac
}

# ── gruppo: cache dei pacchetti ───────────────────────────────────────────────
# La cache utente (`~/.gmbasic/packages`) è una COPIA dei pacchetti del workspace: se resta
# indietro non si ha "libreria vecchia" ma errori che sembrano bug del compilatore (un export
# nuovo risulta "non definito", un `.inc` vecchio convive col nuovo e l'emissione fallisce con
# "la procedura 'X' con N parametri è già definita"). Il confronto lo fa il tool dedicato
# (copia verificata con diff -r); qui si legge solo il suo esito.
#
# Cosa fa fallire: un pacchetto presente in cache E nel workspace che non coincide col repo.
# Cosa NON fa fallire: cache assente, nessun pacchetto installato, pacchetti in cache che non
# esistono nel workspace (installazioni del client: non c'è nulla con cui confrontarli).
group_pkgcache() {
    local tool="$SCRIPT_DIR/gmb_pkg_cache_sync.sh"
    local cache="$HOME/.gmbasic/packages"
    if [ ! -f "$tool" ]; then
        record SKIP "cache:pacchetti" "gmb_pkg_cache_sync.sh non trovato in $SCRIPT_DIR"
        return
    fi
    if [ ! -d "$cache" ]; then
        record INFO "cache:pacchetti" "nessuna cache utente ($cache): niente da confrontare"
        return
    fi

    local names=() d pkg
    for d in "$cache"/*/; do
        [ -d "$d" ] || continue
        pkg="$(basename "$d")"
        [ -d "$ROOT/packages/$pkg" ] || continue
        names+=("$pkg")
    done
    if [ "${#names[@]}" -eq 0 ]; then
        record INFO "cache:pacchetti" "nessun pacchetto del workspace installato in cache"
        return
    fi

    local out rc
    out="$(bash "$tool" --check --quiet --cache "$cache" "${names[@]}" 2>&1)"; rc=$?
    if [ "$rc" -eq 0 ]; then
        record PASS "cache:pacchetti" "${#names[@]} installati, allineati al workspace"
        return
    fi
    local drift listing
    drift="$(printf '%s\n' "$out" | grep -c '^DIVERGE' || true)"
    listing="$(printf '%s\n' "$out" | grep '^DIVERGE' | sed 's/^/  /')"
    record FAIL "cache:pacchetti" "$drift da copiare — allinea con: bash tools/sh/gmb_pkg_cache_sync.sh"$'\n'"$listing"
}

# ── gruppo: freschezza delle dylib dei pacchetti ──────────────────────────────
# Una `lib/<rid>/lib<pkg>.*` PIU' VECCHIA dei sorgenti C del pacchetto è una lib non
# ricostruita: dopo l'ultima modifica non è stato rieseguito `build.sh`. Il sintomo è
# subdolo — l'API BASIC c'è (l'`.inc` aggiornato) ma il simbolo nuovo manca, oppure il
# comportamento è quello vecchio, e l'errore arriva lontano come "simbolo non trovato".
# Confronto: mtime di OGNI lib contro il PIU' RECENTE fra `src/*.c` e `include/*.h`.
# Pacchetti senza sorgenti C (solo BASIC) sono ignorati; con sorgenti ma senza lib
# buildata producono un INFO (non è un errore: la lib si costruisce quando serve).
group_pkgdylibs() {
    local root="$ROOT/packages"
    if [ ! -d "$root" ]; then
        record SKIP "dylib:pacchetti" "cartella packages/ assente"
        return
    fi
    # Il confronto mtime lo fa il tool condiviso (stessa implementazione usata dall'hook
    # pre-commit): qui si legge solo il suo esito. Output TSV: CHECKED / NOLIB / STALE.
    local tool="$SCRIPT_DIR/gmb_pkg_libcheck.sh"
    if [ ! -f "$tool" ]; then
        record SKIP "dylib:pacchetti" "gmb_pkg_libcheck.sh non trovato in $SCRIPT_DIR"
        return
    fi

    local out checked=0 stale=0 nolib=0 tag a b c rel n
    out="$(bash "$tool" 2>&1)"
    local -a infos=() fails=()
    while IFS=$'\t' read -r tag a b c; do
        case "$tag" in
            CHECKED) checked=$((checked + 1)) ;;
            NOLIB)   nolib=$((nolib + 1)); infos+=("$a"$'\t'"$b") ;;
            STALE)   stale=$((stale + 1)); fails+=("$a"$'\t'"$b"$'\t'"$c") ;;
        esac
    done <<< "$out"

    local i
    for i in ${infos[@]+"${infos[@]}"}; do
        rel="${i%%$'\t'*}"
        n="${i##*$'\t'}"
        record INFO "dylib:$rel" "sorgenti C ($n file) ma nessuna lib buildata"
    done

    if [ "$checked" -eq 0 ] && [ "$stale" -eq 0 ]; then
        [ "$nolib" -eq 0 ] && record INFO "dylib:pacchetti" "nessun pacchetto con sorgenti C"
        return
    fi
    if [ "$stale" -eq 0 ]; then
        record PASS "dylib:pacchetti" "$checked pacchetti, lib più recenti dei sorgenti C"
        return
    fi

    local detail="" rest lib newf
    for i in ${fails[@]+"${fails[@]}"}; do
        rel="${i%%$'\t'*}"; rest="${i#*$'\t'}"
        lib="${rest%%$'\t'*}"; newf="${rest#*$'\t'}"
        detail="$detail  $rel: $lib più vecchia di $newf"$'\n'
    done
    detail="${detail%$'\n'}"
    record FAIL "dylib:pacchetti" "$stale lib da ricostruire — esegui packages/<nome>/<ver>/build.sh"$'\n'"$detail"
}

# ── gruppo: conteggi della sezione Structure del README ───────────────────────
# Le cifre accanto alle cartelle in `tools/README.md` invecchiano in silenzio: erano ferme
# a 28/5/27 mentre sul disco c'erano 47 `.bas`, 10 `.c` e 59 script. Qui si confrontano col
# disco, così una cifra sbagliata è un FAIL e non una nota che nessuno rilegge.
# La regola di conteggio di ogni cartella vive in `count_dir` (una sola sede) e la stessa
# convenzione è scritta sotto il blocco Structure del README:
#   basic/ *.bas · c/ *.c · sh/ *.sh + harness senza estensione (parity4,
#   test_minitests_c99native) · data/ file · fixtures/ *.bas
#   (i `gmb_torch*.o` di fixtures/ sono artefatti di build, non fixture)
# Una cartella che dichiara una cifra SENZA una regola qui non passa in silenzio: è un FAIL.
count_dir() {   # count_dir <cartella> -> numero su stdout; exit 1 se la regola manca
    case "$1" in
        basic)    find "$TOOLS/basic" -type f -name '*.bas' 2>/dev/null | wc -l ;;
        c)        find "$TOOLS/c" -type f -name '*.c' 2>/dev/null | wc -l ;;
        sh)       echo $(( $(find "$TOOLS/sh" -type f -name '*.sh' 2>/dev/null | wc -l) \
                          + $(find "$TOOLS/sh" -maxdepth 1 -type f ! -name '*.*' -perm -u+x 2>/dev/null | wc -l) )) ;;
        data)     find "$TOOLS/data" -type f 2>/dev/null | wc -l ;;
        fixtures) find "$TOOLS/fixtures" -type f -name '*.bas' 2>/dev/null | wc -l ;;
        *)        return 1 ;;
    esac
}

group_readme_counts() {
    local readme="$TOOLS/README.md"
    if [ ! -f "$readme" ]; then
        record SKIP "readme:conteggi" "tools/README.md assente"
        return
    fi

    # Ogni riga della sezione Structure che dichiara una cifra `(N)`: `  nome/  descrizione (N)  e.g. …`
    local rows line d want got bad=0 detail="" seen=0
    rows="$(grep -E '^[[:space:]]*[a-z][a-z0-9_-]*/[[:space:]].*\([0-9]+\)' "$readme" 2>/dev/null)"
    while IFS= read -r line; do
        [ -n "$line" ] || continue
        d="$(printf '%s\n' "$line" | sed -n 's/^[[:space:]]*\([a-z][a-z0-9_-]*\)\/[[:space:]].*/\1/p')"
        [ -n "$d" ] || continue
        seen=$((seen + 1))
        want="$(printf '%s\n' "$line" | sed -n 's/^[^(]*(\([0-9][0-9]*\)).*/\1/p')"
        if ! got="$(count_dir "$d")"; then
            bad=$((bad + 1))
            detail="$detail  $d/: cifra dichiarata ma nessuna regola di conteggio in count_dir"$'\n'
            continue
        fi
        got="$(printf '%s' "$got" | tr -d ' ')"
        if [ "$want" != "$got" ]; then
            bad=$((bad + 1))
            detail="$detail  $d/: README ($want) diverso dal disco ($got)"$'\n'
        fi
    done <<< "$rows"

    if [ "$seen" -eq 0 ]; then
        record SKIP "readme:conteggi" "nessuna cifra dichiarata nella sezione Structure"
        return
    fi
    if [ "$bad" -eq 0 ]; then
        record PASS "readme:conteggi" "$seen cartelle allineate ai file su disco"
    else
        detail="${detail%$'\n'}"
        record FAIL "readme:conteggi" "$bad voci stantie nella sezione Structure — aggiorna tools/README.md"$'\n'"$detail"
    fi
}

# ── gruppo: terminologia del manuale ──────────────────────────────────────────
# Le forme vietate del libro sono fissate in docs/manual/stile.md e verificate da
# check_manual_terms.sh; qui si legge solo il suo esito, così un solo comando copre il
# testo del manuale e le sue superfici generate.
group_manual_terms() {
    local tool="$SCRIPT_DIR/check_manual_terms.sh"
    if [ ! -f "$tool" ]; then
        record SKIP "termini:manuale" "check_manual_terms.sh non trovato in $SCRIPT_DIR"
        return
    fi
    if [ ! -d "$ROOT/docs/manual" ]; then
        record INFO "termini:manuale" "docs/manual assente: nessun testo da controllare"
        return
    fi
    local out rc
    out="$(bash "$tool" 2>&1)"; rc=$?
    if [ "$rc" -eq 0 ]; then
        record PASS "termini:manuale" "$(printf '%s\n' "$out" | head -1)"
        return
    fi
    record FAIL "termini:manuale" "$(printf '%s\n' "$out" | grep -v '^OK:' | sed 's/^/  /' | head -20)"
}

# ── gruppo: superfici del manuale ─────────────────────────────────────────────
# I capitoli rigenerati da manual_surface.sh devono restare markdown valido: tabelle con
# intestazione, nessuna riga `|` isolata, blocchi di codice non vuoti e sempre chiusi.
group_manual_surface() {
    local tool="$SCRIPT_DIR/check_manual_surface.sh"
    if [ ! -f "$tool" ]; then
        record SKIP "superfici:manuale" "check_manual_surface.sh non trovato in $SCRIPT_DIR"
        return
    fi
    if [ ! -d "$ROOT/docs/manual/capitoli" ]; then
        record INFO "superfici:manuale" "docs/manual/capitoli assente: nessun capitolo da controllare"
        return
    fi
    local out rc head found
    out="$(bash "$tool" 2>&1)"; rc=$?
    if [ "$rc" -eq 0 ]; then
        record PASS "superfici:manuale" "$(printf '%s\n' "$out" | head -1)"
        return
    fi
    head="$(printf '%s\n' "$out" | grep -m1 '^FALLITO:')"
    found="$(printf '%s\n' "$out" | grep -v '^FALLITO:' | sed 's/^/  /' | head -20)"
    found="${found%$'\n'}"
    record FAIL "superfici:manuale" "${head:-difetti strutturali}"$'\n'"$found"
}

# ── esecuzione ────────────────────────────────────────────────────────────────
# build prima dei test: una rottura di compilazione si vede subito e non sporca i test
group_build_c99native
group_option_registry            # mirror C# del registro opzioni ↔ g_opt_core[] (sempre: statico, a costo zero)
[ "$RUN_SYNTAX" -eq 1 ] && group_syntax
[ "$RUN_CACHE" -eq 1 ] && group_pkgcache
[ "$RUN_DYLIB" -eq 1 ] && group_pkgdylibs
[ "$RUN_TERMINI" -eq 1 ] && group_manual_terms
[ "$RUN_SURFACE" -eq 1 ] && group_manual_surface
[ "$RUN_README" -eq 1 ] && group_readme_counts

# ── riepilogo ─────────────────────────────────────────────────────────────────
printf '\n================= tools/verify =====================\n'
while IFS=$'\t' read -r status name detail; do
    printf '%-5s %-8s %s\n' "$status" "$name" "$detail"
done < "$RESULTS"
echo '---------------------------------------------------'
echo '==================================================='

if [ "$FAILS" -gt 0 ]; then
    echo "ESITO: FALLITO ($FAILS gruppi con FAIL)"
    exit 1
fi
if [ "$SETUPS" -gt 0 ]; then
    echo "ESITO: INCOMPLETO ($SETUPS gruppi non eseguibili)"
    exit 2
fi
echo "ESITO: OK"
exit 0
