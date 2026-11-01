#!/usr/bin/env bash
# tools/sh/repl_no_csharp_proof.sh — PROVA F5: la REPL C vive senza il mondo C#.
#
#   tools/sh/repl_no_csharp_proof.sh [worktree]
#
# Costruisce la REPL (e il compilatore C + gmb_vm_host) in un **worktree temporaneo**
# con `GMBasicSharp/` RIMOSSO, esegue la suite dei comandi del mondo C e verifica che:
#   - RUN (vmc), EVAL, AST, DASM, COMPILE, FIXTURES, SETUP, TRUST, CACHE funzionino;
#   - IL e PCODE dichiarino esplicitamente l'assenza dell'engine C# (mai un finto successo).
# Il worktree è separato: nessun file del checkout condiviso viene toccato (AGENTS §7.6).
# È un build completo (minuti), non una suite di test: si lancia quando serve la prova.
set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
wt=${1:-/tmp/gmb-f5-proof-$$}
logdir=$(mktemp -d /tmp/gmb-f5-logs.XXXXXX)

cleanup() {
    git -C "$root" worktree remove --force "$wt" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "== F5: worktree $wt (HEAD di $(git -C "$root" rev-parse --short HEAD)) =="
git -C "$root" worktree add --detach "$wt" HEAD >/dev/null || { echo "?worktree add fallito"; exit 1; }
rm -rf "$wt/GMBasicSharp"
[ -d "$wt/GMBasicSharp" ] && { echo "?GMBasicSharp non rimosso"; exit 1; }

echo "== build (runtime, REPL, gmb, gmb_vm_host) =="
cmake -S "$wt/GMBasic/native/gmb_runtime_c" -B "$wt/GMBasic/native/gmb_runtime_c/build" \
      -DCMAKE_BUILD_TYPE=Release >"$logdir/rt-conf.log" 2>&1 || { echo "?config runtime"; tail -20 "$logdir/rt-conf.log"; exit 1; }
cmake -S "$wt/GMBasic/native/repl" -B "$wt/GMBasic/native/repl/build" \
      -DCMAKE_BUILD_TYPE=Release >"$logdir/repl-conf.log" 2>&1 || { echo "?config repl"; tail -20 "$logdir/repl-conf.log"; exit 1; }
cmake --build "$wt/GMBasic/native/repl/build" --target gmb_repl -j4 >"$logdir/repl-build.log" 2>&1 \
    || { echo "?build repl"; tail -30 "$logdir/repl-build.log"; exit 1; }

cmake -S "$wt/GMBasic/native/gmb_compiler_c" -B "$wt/GMBasic/native/gmb_compiler_c/build" \
      -DCMAKE_BUILD_TYPE=Release >"$logdir/gmb-conf.log" 2>&1 || { echo "?config gmb"; tail -20 "$logdir/gmb-conf.log"; exit 1; }
cmake --build "$wt/GMBasic/native/gmb_compiler_c/build" -j4 >"$logdir/gmb-build.log" 2>&1 \
    || { echo "?build gmb"; tail -30 "$logdir/gmb-build.log"; exit 1; }

cmake -S "$wt/GMBasic/native/gmb_vm_c" -B "$wt/GMBasic/native/gmb_vm_c/build" \
      -DCMAKE_BUILD_TYPE=Release >"$logdir/vmc-conf.log" 2>&1 || { echo "?config vmc"; tail -20 "$logdir/vmc-conf.log"; exit 1; }
# solo l'host: i selfcheck del runtime non servono alla prova
cmake --build "$wt/GMBasic/native/gmb_vm_c/build" --target gmb_vm_host -j4 >"$logdir/vmc-build.log" 2>&1 \
    || { echo "?build vmc"; tail -30 "$logdir/vmc-build.log"; exit 1; }

echo "== suite comandi (C) =="
sample=$(mktemp /tmp/gmb-f5-XXXX.bas)
cat > "$sample" <<'EOF'
SUB Main()
  PRINT "ciao"; 6*7
END SUB
EOF

cd "$wt/GMBasic" || exit 1
out=$(printf 'RUN %s\nEVAL PRINT 6*7\nAST %s\nDASM %s\nCOMPILE %s\nFIXTURES LIST\nSETUP\nTRUST LIST\nCACHE LIST\nIL %s\nPCODE %s\nQUIT\n' \
      "$sample" "$sample" "$sample" "$sample" "$sample" "$sample" \
      | ./native/repl/build/gmb_repl -s 2>&1 \
      | sed -E 's/\x1b\[[0-9;?]*[A-Za-z]//g')
rc=$?
printf '%s\n' "$out" > "$logdir/suite.txt"

fails=0
check() {
    if printf '%s' "$out" | grep -qE "$1"; then echo "PASS  $2"; else echo "FAIL  $2"; fails=$((fails+1)); fi
}
check 'ciao 42'                                   "RUN (vmc) esegue"
check '^ *42 *$'                                  "EVAL snippet"
check '; AST di'                                  "AST"
check '; Disassembly'                             "DASM"
check 'compilato ->'                              "COMPILE"
check 'manifest:'                                 "FIXTURES"
check 'cache librerie:'                           "SETUP"
check 'keyring:'                                  "TRUST"
check 'IL: engine C# non disponibile'             "IL dichiara l'assenza (non degrada)"
check 'PCODE: engine C# non disponibile'          "PCODE dichiara l'assenza (non degrada)"

rm -f "$sample"
if [ "$fails" -eq 0 ]; then
    echo "== F5 OK: REPL completa senza GMBasicSharp/ =="
    exit 0
fi
echo "== F5: $fails verifiche fallite — log in $logdir =="
exit 1
