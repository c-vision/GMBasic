#!/usr/bin/env python3
"""Phase 4 TokenKind gate — residual lexeme dispatch in the C# compiler/IL emitter.

Scansiona `GMBasicSharp/common/Compiler` e `GMBasicSharp/common/IlBackendCodegen` cercando
confronti per grafia (`.Lexeme.Equals("X")`, `.Lexeme == "X"`, `case "X":`) su nomi che
hanno già un TokenKind `Coll*` in `Ast/CollMembers.cs`.

Regola: il dispatch dei membri di collection deve usare `CollMembers.Is(token, kind)`
(TokenKind-first, fallback di frontiera); un confronto diretto su un nome mappato è un
residuo e fa fallire il check (exit 1).

Uso:
    python3 tools/audit/string_dispatch_scan.py [--all]

`--all` stampa anche i confronti per nomi senza TokenKind (frontiera/HTTP/UDT), utili per
la Phase 4 ma non bloccanti.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TARGETS = [
    ROOT / "GMBasicSharp/common/Compiler",
    ROOT / "GMBasicSharp/common/IlBackendCodegen",
]
COLL = ROOT / "GMBasicSharp/common/Ast/CollMembers.cs"

# Nomi che hanno già un TokenKind Coll* (Map di CollMembers.cs).
COLL_NAMES = {
    m.group(1).lower()
    for m in re.finditer(r'\["([^"]+)"\]\s*=\s*TokenKind\.Coll', COLL.read_text(encoding="utf-8"))
}

# Frontiera lecita: la grafia viene risolta qui (mappe lexer/parser/linker), non è dispatch.
FRONTIER_FILES = {
    "Compiler.Expressions.Functions.cs",
    "Compiler.Expressions.cs",
    "Compiler.Dbf.cs",
    "Compiler.Isam.cs",
    "TokenKind.cs",
    "CollMembers.cs",
}
# Nomi senza TokenKind dedicato (strutturali/HTTP/UDT/sintetici): non migrabili ora.
ALLOWED_NAMES = {
    "length",
    "columns",
    "number",
    "linenumber",
    "tostring",
    "savetofile",
    "symmetricexcept",
}

# Solo i membri (MethodName/Field/PropertyName): "SET"/"GET"/"ADD" sono anche verbi di
# sottocomando (CHART/CONTROL/PIXEL) e non vanno confusi col dispatch di collection.
MEMBER_RECEIVERS = {"MethodName", "Field", "PropertyName"}
PATTERNS = [
    re.compile(r'([A-Za-z_][\w\.]*?)\.Lexeme\.Equals\("([^"]+)"'),
    re.compile(r'([A-Za-z_][\w\.]*?)\.Lexeme\s*(?:==|!=)\s*"([^"]+)"'),
    re.compile(r'([A-Za-z_][\w\.]*?)\.Lexeme\.StartsWith\("([^"]+)"'),
]
CASE_PATTERN = re.compile(r'case\s+"([^"]+)"\s*:')

show_all = "--all" in sys.argv[1:]
residual: list[tuple[str, int, str]] = []
other: list[tuple[str, int, str]] = []

for target in TARGETS:
    for path in sorted(target.rglob("*.cs")):
        if path.name in FRONTIER_FILES:
            continue
        rel = str(path.relative_to(ROOT))
        for lineno, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for pat in PATTERNS:
                for m in pat.finditer(line):
                    receiver, name = m.group(1), m.group(2)
                    if receiver.split(".")[-1] not in MEMBER_RECEIVERS:
                        continue
                    low = name.lower()
                    if low in COLL_NAMES and low not in ALLOWED_NAMES:
                        residual.append((rel, lineno, name))
                    else:
                        other.append((rel, lineno, name))
            for m in CASE_PATTERN.finditer(line):
                other.append((rel, lineno, m.group(1)))

seen: set[tuple[str, int, str]] = set()
residual = [r for r in residual if not (r in seen or seen.add(r))]

print(f"CollMembers mappati: {len(COLL_NAMES)}")
print(f"Residui di dispatch per lexeme su membri Coll*: {len(residual)}")
for rel, lineno, name in residual:
    print(f"  {rel}:{lineno}: {name}")

if show_all:
    print(f"\nConfronti per lexeme non-Coll (frontiera/UDT/HTTP): {len(other)}")
    for rel, lineno, name in other[:200]:
        print(f"  {rel}:{lineno}: {name}")
    if len(other) > 200:
        print(f"  ... e altri {len(other) - 200}")

if residual:
    print("\nFALLITO: usare CollMembers.Is(token, TokenKind.Coll*).")
    sys.exit(1)
print("\nOK: nessun dispatch per lexeme su membri Coll*.")
