#!/usr/bin/env bash
# Test: i flag ritirati dell'audit (--c99, --registry-c99) devono rifiutare l'esecuzione
# (exit 2) e NON produrre alcun file — in particolare --registry-c99 non deve più poter
# rigenerare BackendSupport/Registry/C99.cs.
#
# Perché uno script e non xUnit: questo audit vive in migration/tools-unclassified (dir
# gitignored) e il suo audit.csproj NON è compilabile in place (`../../common` non esiste
# e contiene un <ProjectReference Include="../...csproj" /> letterale). Il test quindi
# compila il VERO Program.cs in una directory temporanea, con ProjectReference assoluti
# verso i progetti reali di GMBasicSharp, e verifica il comportamento del binario.
#
# Uso:  bash migration/tools-unclassified/audit/test_retired_flags.sh
#       bash ... --build-only                 compila soltanto (nessun test eseguito)
#       AUDIT_SRC=/path/Program.cs bash ...   per testare un'altra copia del sorgente
#       (serve a validare il test stesso: su una copia pre-rimozione deve FALLIRE).
# Exit: 0 = tutti i controlli passati; 1 = almeno un controllo fallito (inclusa build
#       rotta); 2 = non testabile (sorgente o progetti mancanti, binario non prodotto).
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"          # audit -> tools-unclassified -> migration -> root
SHARP="$ROOT/GMBasicSharp"

BUILD_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --build-only) BUILD_ONLY=1 ;;
        -h|--help)    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)            echo "opzione sconosciuta: $arg" >&2; exit 2 ;;
    esac
done

AUDIT_SRC="${AUDIT_SRC:-$SCRIPT_DIR/Program.cs}"
[ -f "$AUDIT_SRC" ] || { echo "FATAL: sorgente dell'audit non trovato: $AUDIT_SRC" >&2; exit 2; }
[ -d "$SHARP/common/StdLib" ] || { echo "FATAL: progetti di GMBasicSharp non trovati in $SHARP" >&2; exit 2; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/gmb_audit_flags.XXXXXX")"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

PASS=0
FAIL=0
ok()   { PASS=$((PASS + 1)); printf 'PASS  %s\n' "$1"; }
bad()  { FAIL=$((FAIL + 1)); printf 'FAIL  %s\n' "$1"; [ -n "${2:-}" ] && printf '      %s\n' "$2"; return 0; }

# ── 1. build del vero Program.cs in una dir temporanea ─────────────────────────
BUILD="$WORK/build"
mkdir -p "$BUILD"
cp "$AUDIT_SRC" "$BUILD/Program.cs"
cat > "$BUILD/harness.csproj" <<EOF
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net10.0</TargetFramework>
    <LangVersion>preview</LangVersion>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
    <RootNamespace>Basic.Audit</RootNamespace>
    <AssemblyName>gmb_audit_undertest</AssemblyName>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="$SHARP/common/StdLib/StdLib.csproj" />
    <ProjectReference Include="$SHARP/common/Compiler/Compiler.csproj" />
    <ProjectReference Include="$SHARP/common/IlBackendRuntime/IlBackendRuntime.csproj" />
    <ProjectReference Include="$SHARP/common/BackendSupport/BackendSupport.csproj" />
  </ItemGroup>
</Project>
EOF

# cwd = dir del progetto: con `dotnet build <path>` da un'altra cwd MSBuild risolve male i
# ProjectReference relativi interni a GMBasicSharp (path assoluti prefissati di ".." per
# ogni segmento della dir di build → MSB3202 su tutti i progetti referenziati).
if ! BUILD_OUT="$(cd "$BUILD" && dotnet build harness.csproj -v q --nologo 2>&1)"; then
    # build rotta = FAIL (exit 1), non "non testabile": un errore di compilazione è un
    # difetto del codice, mentre exit 2 è riservato ai prerequisiti mancanti.
    BUILD_ERR="$(printf '%s\n' "$BUILD_OUT" | grep -m1 -E 'error [A-Z]+[0-9]+|error:' || echo 'errore di compilazione')"
    printf "FAIL  build dell'audit: %s\n" "$BUILD_ERR" >&2
    exit 1
fi
BIN="$BUILD/bin/Debug/net10.0/gmb_audit_undertest.dll"
[ -f "$BIN" ] || { echo "FATAL: binario non prodotto in $BIN" >&2; exit 2; }

# --build-only: si ferma dopo la compilazione (usato dal gruppo `build` di migration/verify.sh,
# così la rottura di build viene intercettata PRIMA dei test).
if [ "$BUILD_ONLY" -eq 1 ]; then
    echo "build ok: $(basename "$AUDIT_SRC")"
    exit 0
fi

# ── 2. sandbox con Root risolvibile (GMBASIC.sln in CWD) ──────────────────────
# Root = CWD quando contiene GMBASIC.sln (vedi Program.ResolveRoot). Serve per il
# controllo di non-regressione su --registry-c99native, che legge i sorgenti del repo.
SANDBOX="$WORK/root"
mkdir -p "$SANDBOX"
: > "$SANDBOX/GMBASIC.sln"
ln -s "$SHARP/common" "$SANDBOX/common"
ln -s "$ROOT/GMBasic/native" "$SANDBOX/native"

RC=0
OUT=""
run_audit() {                      # run_audit <args...>  -> RC, OUT
    OUT="$(cd "$SANDBOX" && dotnet "$BIN" "$@" 2>&1)"
    RC=$?
}

# ── 3. --registry-c99: rifiuta e non scrive ───────────────────────────────────
TARGET_DIR="$WORK/must_not_exist"
TARGET="$TARGET_DIR/Registry-C99.cs"
mkdir -p "$TARGET_DIR"
rm -f "$TARGET"

run_audit --registry-c99 --out "$TARGET"
[ "$RC" -eq 2 ] && ok "--registry-c99 esce con 2" || bad "--registry-c99 esce con 2" "exit effettivo: $RC"
[ ! -e "$TARGET" ] && ok "--registry-c99 non scrive il registry" || bad "--registry-c99 non scrive il registry" "creato: $TARGET"

# ── 4. --c99: rifiuta e non scrive manifest ───────────────────────────────────
MANIFEST="$TARGET_DIR/c99.json"

run_audit --c99 --json "$MANIFEST"
[ "$RC" -eq 2 ] && ok "--c99 esce con 2" || bad "--c99 esce con 2" "exit effettivo: $RC"
[ ! -e "$MANIFEST" ] && ok "--c99 non scrive il manifest" || bad "--c99 non scrive il manifest" "creato: $MANIFEST"
# usage: si forza con un flag inesistente, così il testo è sempre quello vero
# (non quello — eventualmente assente — prodotto dal flag sotto test).
run_audit --flag-inesistente
case "$OUT" in
    *"--c99"*)
        bad "l'usage non elenca più --c99" "l'usage lo elenca ancora: $(printf '%s' "$OUT" | head -1)" ;;
    *)  ok "l'usage non elenca più --c99" ;;
esac

# ── 5. non-regressione: --registry-c99native è ancora vivo ────────────────────
run_audit --registry-c99native
[ "$RC" -eq 0 ] && ok "--registry-c99native esce con 0" || bad "--registry-c99native esce con 0" "exit effettivo: $RC"
case "$OUT" in
    *"public static BackendImplementationRegistry CreateC99Native()"*)
        ok "--registry-c99native emette CreateC99Native" ;;
    *)  bad "--registry-c99native emette CreateC99Native" "output inatteso: $(printf '%s' "$OUT" | head -3)" ;;
esac

# ── riepilogo ─────────────────────────────────────────────────────────────────
printf '\n%d passati, %d falliti\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ] || exit 1
exit 0
