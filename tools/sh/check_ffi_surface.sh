#!/bin/bash
# check_ffi_surface.sh — verifica che i namespace migrati su libreria FFI (DECLARE ...
# LIB "xxx" in shared/include/<ns>.inc) NON abbiano piu' una superficie builtin /
# dispatch nelle 4 gambe (IL, PCodeVM, VM/C, C99Native) ne' nella StdLib C#.
#
# Una funzione migrata su libreria deve essere raggiungibile SOLO via FFI:
# nessun TokenKind, nessun BuiltInFunction, nessuna entry nei registry per-backend,
# nessuna implementazione nella StdLib C#.
#
# Uso:
#   tools/sh/check_ffi_surface.sh [--quiet] [<inc>...]
#     <inc>  nomi degli .inc in shared/include senza estensione
#            (default: trading metrics volatility fundamentals bonds derivatives
#             portfolio backtest)
# Exit: 0 = pulito · 1 = residui trovati · 2 = setup
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$(pwd)
QUIET=0
INCS=()
for a in "$@"; do
  case "$a" in
    --quiet) QUIET=1 ;;
    --*) echo "check_ffi_surface: opzione sconosciuta $a" >&2; exit 2 ;;
    *) INCS+=("$a") ;;
  esac
done
[ "${#INCS[@]}" -eq 0 ] && INCS=(trading metrics volatility fundamentals bonds derivatives portfolio backtest)

INC_DIR="$ROOT/shared/include"
REG_JSON="$ROOT/shared/spec/registry/canonical.registry.json"
BIF="$ROOT/GMBasicSharp/common/Compiler/BuiltInFunction.cs"
TK_CS="$ROOT/GMBasicSharp/common/Ast/TokenKind.cs"
KW_CS="$ROOT/GMBasicSharp/common/Lexer/Lexer.Keywords.cs"
KW_C="$ROOT/GMBasic/native/gmb_compiler_c/frontend/src/gmb_lexer.c"
TK_C="$ROOT/GMBasic/native/gmb_compiler_c/frontend/include/gmb_tokenkind.h"
REG_DIR="$ROOT/GMBasicSharp/common/BackendSupport/Registry"
STDLIB_DIRS="$ROOT/GMBasicSharp/common/StdLib $ROOT/GMBasicSharp/common/StdLib.Runtime"

fails=0
checks=0
note() { [ "$QUIET" -eq 0 ] && echo "$1"; }

for base in "${INCS[@]}"; do
  inc="$INC_DIR/$base.inc"
  if [ ! -f "$inc" ]; then
    echo "check_ffi_surface: $inc assente" >&2
    exit 2
  fi
  ns=$(grep -m1 "^NAMESPACE " "$inc" 2>/dev/null | awk '{print $2}')
  [ -z "$ns" ] && { echo "check_ffi_surface: $inc senza NAMESPACE" >&2; exit 2; }
  names=$(grep -E "^DECLARE (FUNCTION|SUB) " "$inc" | sed -E 's/^DECLARE (FUNCTION|SUB) ([A-Za-z0-9_]+) .*/\2/' | sort -u)
  n_names=$(echo "$names" | wc -l | tr -d ' ')
  f0=$fails

  # 1) superficie canonica: il namespace non deve esistere
  if [ -f "$REG_JSON" ]; then
    c=$(jq --arg ns "$ns" '[.procedures[] | select(.ns==$ns)] | length' "$REG_JSON" 2>/dev/null)
    checks=$((checks+1))
    [ "${c:-0}" != "0" ] && { note "FAIL $base: $c procedure canoniche nel namespace $ns"; fails=$((fails+1)); }
  fi

  # 2) registry per-backend (le 4 gambe)
  for f in "$REG_DIR"/*.cs; do
    [ -f "$f" ] || continue
    c=$(grep -cE "\"$ns\." "$f")
    checks=$((checks+1))
    [ "$c" != "0" ] && { note "FAIL $base: $c entry in $(basename "$f")"; fails=$((fails+1)); }
  done

  # 3) per ogni funzione dichiarata: nessuna traccia come builtin/keyword/StdLib
  while read -r nm; do
    [ -z "$nm" ] && continue
    up=$(echo "$nm" | tr 'a-z' 'A-Z')
    c=$(grep -cE "^[[:space:]]+$up = [0-9]+," "$BIF" 2>/dev/null); checks=$((checks+1))
    [ "$c" != "0" ] && { note "FAIL $base.$nm: membro BuiltInFunction $up"; fails=$((fails+1)); }
    c=$(grep -cE "\"$up\"" "$KW_CS" 2>/dev/null); checks=$((checks+1))
    [ "$c" != "0" ] && { note "FAIL $base.$nm: keyword C# $up"; fails=$((fails+1)); }
    c=$(grep -cE "TokenKind\.$nm\b" "$TK_CS" 2>/dev/null); checks=$((checks+1))
    [ "$c" != "0" ] && { note "FAIL $base.$nm: TokenKind C# $nm"; fails=$((fails+1)); }
    c=$(grep -cE "GMB_KEYWORD_ENTRY\(\"$up\"" "$KW_C" 2>/dev/null); checks=$((checks+1))
    [ "$c" != "0" ] && { note "FAIL $base.$nm: keyword C $up"; fails=$((fails+1)); }
    c=$(grep -cE "GMB_TOK_$nm\b" "$TK_C" 2>/dev/null); checks=$((checks+1))
    [ "$c" != "0" ] && { note "FAIL $base.$nm: TokenKind C GMB_TOK_$nm"; fails=$((fails+1)); }
    c=$(grep -rl "\b$up\b" $STDLIB_DIRS 2>/dev/null | grep -v "/obj/\|/bin/" | wc -l | tr -d ' '); checks=$((checks+1))
    [ "$c" != "0" ] && { note "FAIL $base.$nm: riferimento nella StdLib C#"; fails=$((fails+1)); }
  done <<< "$names"

  [ "$fails" -eq "$f0" ] && note "ok   $base: ns=$ns, $n_names funzioni FFI, nessuna superficie residua"
done

echo
if [ "$fails" -eq 0 ]; then
  echo "check_ffi_surface: OK ($checks controlli, 0 residui)"
  exit 0
fi
echo "check_ffi_surface: $fails RESIDUI su $checks controlli"
exit 1
