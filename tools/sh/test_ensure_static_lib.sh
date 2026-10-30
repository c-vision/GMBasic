#!/usr/bin/env bash
# test_ensure_static_lib.sh — verifica le DUE garanzie anti-stale dei test C:
#   1) `ensure_static_lib()` di tools/sh/test_minitests_c99native (lib statiche linkate by-path:
#      compiler/emitter, db, LLM) — casi bash, qui sotto;
#   2) `VmHostFreshness.Ensure()` (host `gmb_vm_host` della VM/C, tests/Shared/VmHostFreshness.cs)
#      — casi xUnit in VmHostFreshnessTests.cs, pilotati da questo stesso tool.
# Tutto SENZA toccare build reali, LLMEnv o il mutex di build.
#
# Perché esiste: quella funzione decide se ricostruire le static lib linkate BY-PATH
# (compiler/emitter, db, LLM). Un errore lì si manifesta come "test verde su codice vecchio"
# oppure come test bloccati per una lib LLM rotta — entrambi costosi. Qui i rami si
# ricontrollano in pochi secondi dopo ogni modifica.
#
# Come: le funzioni vengono ESTRATTE DAL FILE REALE (per marker di nome, non per numero di
# riga) e sorbite in una subshell con build dir FINTA in una directory temporanea, senza
# CMakeCache.txt → `cmake --build` fallisce davvero (exit != 0), ma su un albero usa-e-getta.
#
# Nota: il blocco estratto contiene `ENSURE_POLICY=hard` come default, quindi la policy va
# impostata DOPO il `source` (altrimenti il default sovrascrive quella del test).
#
# Uso: tools/sh/test_ensure_static_lib.sh [--source <script>] [--no-cs]
#   --source <script>  verifica un altro file (es. una copia mutata, per provare che il test
#                      sappia FALLIRE). Default: tools/test_minitests_c99native.
#   --no-cs            solo i casi bash (i casi C# costano un `dotnet test`).
# macOS: la funzione usa `stat -f` (come il resto del repo).
set -u
ROOT="$(cd "$(dirname "$0")/../../GMBasic" && pwd)"
# I TOOL vivono nella radice del MONOREPO (`tools/`), non più in GMBasic/tools.
MONO="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$MONO/tools/sh/test_minitests_c99native"
RUN_CS=1
while [ $# -gt 0 ]; do
  case "$1" in
    --source) SRC="${2:-}"; shift 2 ;;
    --no-cs) RUN_CS=0; shift ;;
    *) shift ;;
  esac
done
[ -f "$SRC" ] || { echo "sorgente non trovato: $SRC" >&2; exit 1; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmb_ensure_probe.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
FN="$WORK/fn.sh"

# ── estrazione per marker di nome ────────────────────────────────────────────────────────
s1=$(grep -n '^newest_src_mtime() {' "$SRC" | head -1 | cut -d: -f1)
s2=$(grep -n '^ensure_static_lib() {' "$SRC" | head -1 | cut -d: -f1)
if [ -z "$s1" ] || [ -z "$s2" ]; then
  echo "FAIL estrazione: newest_src_mtime()/ensure_static_lib() non trovati in $SRC" >&2
  echo "     (funzione rinominata? aggiorna questo test)" >&2
  exit 1
fi
e2=$(awk -v s="$s2" 'NR>s && /^}$/{print NR; exit}' "$SRC")
if [ -z "$e2" ]; then
  echo "FAIL estrazione: chiusura di ensure_static_lib() non trovata" >&2
  exit 1
fi
sed -n "${s1},${e2}p" "$SRC" > "$FN"
echo "funzioni estratte: righe $s1-$e2 di ${SRC#$ROOT/}"

pass=0; fail=0
# case_init <nome> [lib_assente]
#   src/ con un sorgente VECCHIO e build/ con una lib finta: così "lib più vecchia dei
#   sorgenti" è vero e il rebuild viene tentato (e fallisce: non c'è CMakeCache).
case_init() {
  local d="$WORK/$1"; mkdir -p "$d/src" "$d/build"
  printf 'int x;\n' > "$d/src/a.c"
  printf 'fake\n' > "$d/build/lib.a"
  touch -t 202001010000 "$d/src/a.c" "$d/build/lib.a"
  touch "$d/src/a.c"                      # sorgente più nuovo della lib
  [ "${2:-}" = "lib_assente" ] && rm -f "$d/build/lib.a"
  echo "$d"
}

# run <desc> <policy> <dir> <status_atteso> <sottostringa_attesa|""> [vuoto]
#   needle=""  → non si controlla il testo
#   "vuoto"     → l'output deve essere COMPLETAMENTE vuoto (nessun build tentato)
run() {
  local desc="$1" policy="$2" dir="$3" want="$4" needle="$5" mode="${6:-}"
  local out st ok="FAIL"
  out=$(bash -c 'source "$0"; ENSURE_POLICY="$1"; ensure_static_lib "$2" "$3" fake_target "$4"' \
        "$FN" "$policy" "$dir/build/lib.a" "$dir/build" "$dir/src" 2>&1); st=$?
  [ "$st" = "$want" ] && ok="PASS"
  if [ "$mode" = "vuoto" ] && [ -n "$out" ]; then ok="FAIL"; fi
  if [ -n "$needle" ]; then
    printf '%s\n' "$out" | grep -q "$needle" || ok="FAIL"
  fi
  if [ "$ok" = "PASS" ]; then pass=$((pass+1)); else fail=$((fail+1)); fi
  printf '%s %-42s status=%s (atteso %s)\n' "$ok" "$desc" "$st" "$want"
  [ "$ok" = "PASS" ] || printf '%s\n' "$out" | sed -e 's/^/      | /' | head -8
}

d=$(case_init soft_lib);      run "soft: build fallita, lib presente"   soft "$d" 0 "uso la lib esistente"
d=$(case_init hard_lib);      run "hard: build fallita, lib presente"   hard "$d" 1 "mi fermo"
d=$(case_init soft_nolib lib_assente); run "soft: lib assente"          soft "$d" 1 "mi fermo"

# fresco = lib PIÙ NUOVA del sorgente: nessun build, silenzio totale
d="$WORK/fresh"; mkdir -p "$d/src" "$d/build"
printf 'int x;\n' > "$d/src/a.c"; touch -t 202001010000 "$d/src/a.c"
printf 'fake\n' > "$d/build/lib.a"; touch "$d/build/lib.a"
run "fresco: nessun rebuild" soft "$d" 0 "" vuoto

# guardia "nessun sorgente": la dir passata non esiste → non si deve forzare nessun build
d="$WORK/no_src"; mkdir -p "$d/build"; printf 'fake\n' > "$d/build/lib.a"
run "sorgenti assenti: nessun rebuild" soft "$d" 0 "" vuoto

# ── 2) VmHostFreshness (C#): i casi xUnit ────────────────────────────────────────────────
# Il dll di test viene usato con --no-build quando è già più nuovo dei sorgenti: su un albero
# condiviso la build può restare in coda dietro un'altra sessione per minuti, e qui servono
# secondi. Watchdog obbligatorio (macOS non ha `timeout`): oltre il limite → SKIP, non FAIL.
watchdog() { # <secondi> <cmd...>  → status del comando, o 124 se scade
  local secs="$1"; shift
  "$@" >"$WORK/cs.log" 2>&1 &
  local p=$! i=0
  while kill -0 "$p" 2>/dev/null; do
    if [ "$i" -ge "$secs" ]; then kill -9 "$p" 2>/dev/null; return 124; fi
    sleep 1; i=$((i+1))
  done
  wait "$p"
}

# Percorsi dei test C#: sono in `GMBasicSharp/tests/` (non più `GMBasic/tests/`, dove il
# tool li cercava prima: la condizione era sempre falsa e il caso C# veniva SALTATO in silenzio).
CS_TESTS="$MONO/GMBasicSharp/tests"
CS_HELPER="$CS_TESTS/Shared/VmHostFreshness.cs"
CS_TESTFILE="$CS_TESTS/GMBASIC.IlBackend.Tests/VmHostFreshnessTests.cs"
if [ "$RUN_CS" = "1" ] && command -v dotnet >/dev/null 2>&1 && [ -f "$CS_TESTFILE" ]; then
  CS_PROJ="$CS_TESTS/GMBASIC.IlBackend.Tests/GMBASIC.IlBackend.Tests.csproj"
  CS_DLL="$CS_TESTS/GMBASIC.IlBackend.Tests/bin/Debug/net10.0/GMBASIC.IlBackend.Tests.dll"
  need_build=0
  if [ ! -f "$CS_DLL" ]; then need_build=1; else
    for s in "$CS_HELPER" "$CS_TESTFILE"; do
      [ "$CS_DLL" -ot "$s" ] && need_build=1
    done
  fi
  if [ "$need_build" = "1" ]; then
    echo "--- VmHostFreshness: build del progetto di test (watchdog 300s) ---"
    watchdog 300 dotnet build "$CS_PROJ" -v q --nologo; bst=$?
    if [ "$bst" -eq 124 ]; then
      echo "SKIP VmHostFreshness (xUnit): build oltre 300s (contesa di build su albero condiviso)"
      echo "     riprova: dotnet build tests/GMBASIC.IlBackend.Tests && tools/sh/test_ensure_static_lib.sh"
    elif [ "$bst" -ne 0 ]; then
      echo "FAIL VmHostFreshness (xUnit): build fallita"; tail -12 "$WORK/cs.log" | sed 's/^/      | /'
      fail=$((fail+1))
    fi
  fi
  if [ -f "$CS_DLL" ] && [ "${bst:-1}" -ne 124 ]; then
    watchdog 180 dotnet test "$CS_PROJ" --no-build --nologo -v q \
      --filter "FullyQualifiedName~VmHostFreshnessTests"; tst=$?
    if [ "$tst" -eq 0 ]; then
      line=$(grep -E "Superat[oi]!|Passed!" "$WORK/cs.log" | tail -1)
      echo "PASS VmHostFreshness (xUnit) :: ${line:-ok}"
      pass=$((pass+1))
    elif [ "$tst" -eq 124 ]; then
      echo "SKIP VmHostFreshness (xUnit): oltre 180s"
    else
      echo "FAIL VmHostFreshness (xUnit)"; tail -15 "$WORK/cs.log" | sed 's/^/      | /'
      fail=$((fail+1))
    fi
  fi
elif [ "$RUN_CS" = "1" ]; then
  # NON silenzioso (2026-10-29): la classe xUnit `VmHostFreshnessTests` non esiste più nel
  # progetto. La garanzia anti-stale lato C# è coperta da `tools/runners/test_vmrun/Program.cs`
  # (`VmHostFreshness.Ensure(root)`): qui si dichiara lo SKIP, senza fingere un PASS.
  if [ -f "$CS_HELPER" ]; then
    echo "SKIP VmHostFreshness (xUnit): classe VmHostFreshnessTests assente"
    echo "     (helper: $CS_HELPER · la garanzia C# è esercitata da tools/runners/test_vmrun)"
  else
    echo "FAIL VmHostFreshness (xUnit): né la classe né l'helper esistono ($CS_TESTS)"
    fail=$((fail+1))
  fi
fi

echo "---------------------------------"
echo "anti-stale (ensure_static_lib + VmHostFreshness): PASS=$pass FAIL=$fail"
[ "$fail" -eq 0 ]
