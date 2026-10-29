#!/usr/bin/env bash
# registry-export — P0 del monorepo: rigenera TUTTI i derivati del registry dalla StdLib.
# Nessun Python: solo il tool C# (registry-export) e i generatori C#/C già presenti.
#
# Uso: tools/registry-export/update.sh
#      tools/registry-export/update.sh --check   (solo verifica, exit 1 su drift)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
cd "$ROOT"

if [ "${1:-}" = "--check" ]; then
  echo "gate: ID enum (TokenKind / BuiltInFunction / tokenkind C)..."
  dotnet run --project "$HERE" -- --check-enum-ids
  echo "verifica derivati..."
  dotnet run --project "$HERE" -- --check
  echo "OK — derivati allineati e idempotenti."
  exit 0
fi

echo "gate: ID enum (TokenKind / BuiltInFunction / tokenkind C)..."
dotnet run --project "$HERE" -- --check-enum-ids

echo "rigenero i derivati (canonical, inventory, CSV, lookup C/C#, coverage, pconst, tokenkind C, DB)..."
dotnet run --project "$HERE" -- --all

echo "gate: idempotenza (una seconda passata non deve produrre drift)..."
dotnet run --project "$HERE" -- --check
echo "OK — derivati aggiornati e idempotenti."
