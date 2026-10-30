#!/usr/bin/env bash
# GMBASIC — Gate dimensioni file (task 6.0.1 della PROPOSAL-dispatch-refactor).
#
# Ogni file di dispatch/logica nei 4 backend deve stare sotto le 1200 righe,
# tranne i file tabellari puri (mappe/enum/dati) e i casi documentati dove
# spezzare danneggia la leggibilità.
#
# Uso:
#   tools/sh/check_file_size.sh            # check su tutti i file backend (fail se > 1200)
#   tools/sh/check_file_size.sh --list     # elenca solo i file sopra soglia (no fail)
#   tools/sh/check_file_size.sh --quiet    # output minimo, exit code per CI
#
# Exit code: 0 = ok, 1 = file sopra soglia OPPURE escape illegali nelle stringhe BASIC.
#
# Dal 2026-10-28 questo script e' il gate SORGENTE unico: oltre alle dimensioni esegue
# anche tools/basic/find_illegal_escapes.bas (backslash dentro un letterale BASIC -> `div` fra
# stringhe -> "Invalid operand types in BinaryInst"). Il verdetto degli escape e' stampato
# su una riga sua, perche' l'exit code qui e' gia' rosso per i monolitici preesistenti e
# da solo non distinguerebbe le due cause. `--list` resta solo-dimensioni.

LIMIT=1200
ROOT="$(cd "$(dirname "$0")/../../GMBasic" && pwd)"
# I TOOL vivono nella radice del MONOREPO (`tools/`), non più in GMBasic/tools.
MONO="$(cd "$(dirname "$0")/../.." && pwd)"
LIST_ONLY=0
QUIET=0

for arg in "$@"; do
    case "$arg" in
        --list) LIST_ONLY=1 ;;
        --quiet) QUIET=1 ;;
    esac
done

# File che possono eccedere le 1200 righe (tabellari puri o split dannoso documentato).
# Ogni voce è un glob relativo alla root; quando il file scende sotto soglia la voce si rimuove.
#
# ECCEZIONI DOCUMENTATE (v. PROPOSAL §4 — direttiva 1200):
#  - *CodeEmitter.cs / *Processor.cs (core): il file PRINCIPALE che contiene il dispatcher
#    statement/builtin. La direttiva interna di questi file impone "core ~5000, nuovi domini
#    in file ≤1500" — il core dispatcher NON va forzato sotto 1200 (splittarlo danneggia la
#    leggibilità del dispatch centrale). I DOMINI (partial) devono invece stare ≤1200.
#  - *tokenkind* / enum: file tabellari puri (dati), esenti dalla soglia.
ALLOW_OVER_LIMIT=(
    "common/IlBackendRuntime/PCode/PCodeProcessor.cs"
    "common/IlBackendCodegen/IlProgramCompiler.cs"
    "native/gmb_vm_c/gmb_vm_core.c"
    "native/gmb_compiler_c/src/gmb_c99_emit.c"
    "native/gmb_compiler_c/src/gmb_c99_emit_builtin.c"
    "common/IlBackendRuntime/IlRuntime.cs"
    "common/IlBackendRuntime/IlGpmRuntime.cs"
    "native/gmb_compiler_c/src/gmb_tokenkind.c"
    "common/IlBackendRuntime/PCode/PCodeProcessor.Linalg.cs"
    "native/gmb_compiler_c/src/gmb_lexer.c"
    "common/IlBackendRuntime/PCode/PCodeProcessor.Syscalls.cs"
    "common/IlBackendRuntime/PCode/PCodeProcessor.Builtins.cs"
    "common/IlBackendCodegen/Partial/IlProgramCompiler.Builtins.cs"
    "native/gmb_compiler_c/src/compiler/gmb_compiler.c"
    "native/gmb_vm_c/gmb_gpm.c"
    # ── aggiunte 2026-10-29 (gate reso onesto dopo lo split di gmb_vm_tui.c) ──────────────
    # `gmb_builtin.c` è IL dispatcher dei builtin della VM/C (equivalente C di
    # PCodeProcessor.Builtins.cs, già in elenco): la sua lunghezza è la somma degli handler per
    # dominio, non un file monolitico da splittare alla cieca.
    "native/gmb_vm_c/gmb_builtin.c"
    # `gmb_syscall_db.c` è una TABELLA pura {codice, handler} (dati, zero logica): rientra
    # nell'eccezione "file tabellari puri" come *tokenkind*.
    "native/gmb_vm_c/gmb_syscall_db.c"
)

# Elenco esteso dei file di dispatch/logica dei 4 backend. Quando un file viene
# splittato sotto soglia, lo si rimuove da qui (la riga resta nel piano 6.x).
FILES=()
while IFS= read -r f; do
    FILES+=("$f")
done < <(cd "$ROOT" && find \
    common/IlBackendCodegen \
    common/IlBackendRuntime \
    common/IlBackendRuntime/PCode \
    native/gmb_vm_c \
    native/gmb_compiler_c/src \
    native/gmb_compiler_c/src/parser \
    \( -name '*.cs' -o -name '*.c' -o -name '*.h' \) \
    -not -path '*/build/*' -not -path '*/_deps/*' 2>/dev/null \
    | sort -u)

over=()
for f in "${FILES[@]}"; do
    n=$(wc -l < "$ROOT/$f" 2>/dev/null || echo 0)
    if [ "$n" -gt "$LIMIT" ]; then
        allowed=0
        for pat in "${ALLOW_OVER_LIMIT[@]}"; do
            if [[ "$f" == $pat ]]; then allowed=1; break; fi
        done
        if [ "$allowed" -eq 0 ]; then
            over+=("$n $f")
        elif [ "$QUIET" -eq 0 ]; then
            echo "(escluso, documentato) $n $f"
        fi
    fi
done

size_rc=0
if [ ${#over[@]} -eq 0 ]; then
    [ "$QUIET" -eq 0 ] && echo "OK: nessun file oltre $LIMIT righe."
else
    # ordina per numero decrescente
    IFS=$'\n' over=($(sort -rn <<<"${over[*]}")); unset IFS

    if [ "$LIST_ONLY" -eq 1 ] || [ "$QUIET" -eq 1 ]; then
        for o in "${over[@]}"; do
            echo "$o"
        done
        [ "$LIST_ONLY" -eq 1 ] && exit 0
        size_rc=1
    else
        echo "FALLITO: $(( ${#over[@]} )) file oltre $LIMIT righe:"
        echo ""
        printf '%-7s %s\n' "RIGHE" "FILE"
        printf '%-7s %s\n' "-----" "----"
        for o in "${over[@]}"; do
            echo "$o"
        done
        size_rc=1
    fi
fi

# --------------------------------------------------------------- gate 2: escape BASIC
# Nel layout del monorepo i sorgenti BASIC stanno in shared/ (samples, minitests) e non
# sotto il checkout: senza questo il gate coprirebbe 73 file su ~3.200. Se shared/ non
# c'e' (repo standalone) si scansiona il solo ROOT. Identico nelle due copie dello script.
scan_roots=("$ROOT")
[ -d "$ROOT/../shared/samples" ] && scan_roots+=("$ROOT/../shared")

# Il gate e' BASIC (tools/basic/find_illegal_escapes.bas), non piu' uno script Python: gira sulla
# gamba IL del runner. Se il runner non e' compilato il gate FALLISCE (non "pulisce" in
# silenzio): un gate che non puo' leggere i file non deve dire verde.
ILRUN="$MONO/tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll"
[ -f "$ILRUN" ] || ILRUN="$ROOT/../tools/runners/test_ilrun/bin/Debug/net10.0/test_ilrun.dll"
esc_rc=0
if [ ! -f "$ILRUN" ]; then
    echo "ESCAPE GATE FAIL: runner IL assente ($ILRUN). Compila: dotnet build tools/runners/test_ilrun"
    esc_rc=1
elif [ "$QUIET" -eq 1 ]; then
    # exit code del GATE preso direttamente (niente pipeline: $? sarebbe quello di grep).
    out=$(dotnet "$ILRUN" --engine=il "$MONO/tools/basic/find_illegal_escapes.bas" "${scan_roots[@]}" 2>/dev/null) || esc_rc=$?
    # il runner IL accoda "[OK] terminato senza eccezioni": fuori dai piedi, il report resta
    # identico a quello dello script originale (TOTALE + verdetto).
    out=$(printf '%s\n' "$out" | grep -v '^\[OK\] terminato senza eccezioni$')
    if [ "$esc_rc" -ne 0 ]; then
        echo "$out" | tail -3
        echo "ESCAPE GATE FAIL (dettaglio: dotnet $ILRUN --engine=il tools/basic/find_illegal_escapes.bas \".\")"
    fi
else
    echo ""
    echo "== gate 2: escape illegali nelle stringhe BASIC =="
    dotnet "$ILRUN" --engine=il "$MONO/tools/basic/find_illegal_escapes.bas" "${scan_roots[@]}" 2>/dev/null | grep -v '^\[OK\] terminato senza eccezioni$' | tail -3
    esc_rc=${PIPESTATUS[0]}
fi

[ "$size_rc" -eq 0 ] && [ "$esc_rc" -eq 0 ] && exit 0
exit 1
