#!/usr/bin/env python3
"""sample_minitest_gaps.py — casi d'uso dei SAMPLE non previsti dai MiniTest canonici.

Per ogni Sample taggato `' SURFACE:` estrae le chiamate `NS.FUNC(...)` (nome + arità) e le
confronta con il file canonico del namespace (`shared/minitests/<ns>/<ns>_all.bas`):
un caso d'uso è "non previsto" se nel canonico la stessa funzione compare solo con
un'arità diversa o non compare affatto. Nomi confrontati senza distinzione di maiuscole.

Uso: python3 tools/sample_minitest_gaps.py [--out tools/reports/sample_minitest_gaps.txt]
"""
import argparse
import glob
import os
import re

ROOT = "shared"
CALL_START = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(")


def ns_file(ns: str) -> str:
    d = ns.lower()
    return os.path.join(ROOT, "minitests", d, d.replace(".", "_") + "_all.bas")


def _arity(text: str, open_idx: int) -> int:
    """Numero di argomenti di una chiamata, contando solo le virgole di primo livello."""
    depth, commas, i, n = 0, 0, open_idx, len(text)
    quote = None
    while i < n:
        c = text[i]
        if quote:
            if c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                break
        elif c == "," and depth == 1:
            commas += 1
        i += 1
    body = text[open_idx + 1:i].strip()
    return 0 if not body else commas + 1


def imports(text: str):
    """alias -> namespace canonico (IMPORT X AS Y; IMPORT X)."""
    al = {}
    for m in re.finditer(r"^\s*IMPORT\s+([\w.]+)(?:\s+AS\s+(\w+))?", text, re.M):
        ns, alias = m.group(1).upper(), m.group(2)
        al[alias.upper() if alias else ns] = ns
    return al


def calls(text: str):
    al = imports(text)
    out = set()
    for m in CALL_START.finditer(text):
        ns, fn = m.group(1).upper(), m.group(2).lower()
        out.add((al.get(ns, ns), fn, _arity(text, m.end() - 1)))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="tools/reports/sample_minitest_gaps.txt")
    args = ap.parse_args()

    canon = {}
    rows = []
    for f in sorted(glob.glob(os.path.join(ROOT, "samples", "**", "*.bas"), recursive=True)):
        text = open(f, encoding="utf-8", errors="replace").read()
        if not text.startswith("' SURFACE:"):
            continue
        for ns, fn, ar in sorted(calls(text)):
            path = ns_file(ns)
            if path not in canon:
                canon[path] = calls(open(path, encoding="utf-8", errors="replace").read()) \
                    if os.path.exists(path) else set()
            have = {(n, f2, a) for (n, f2, a) in canon[path] if n == ns}
            if (ns, fn, ar) in have:
                continue
            same_fn = sorted({a for (n, f2, a) in canon[path] if n == ns and f2 == fn})
            kind = "ARITA" if same_fn else "ASSENTE"
            rows.append((os.path.relpath(f, ROOT), ns, fn, ar, kind,
                         ",".join(str(a) for a in same_fn)))

    lines = ["SAMPLE → MINITEST: casi d'uso non previsti dai canonici", ""]
    lines.append(f"casi: {len(rows)}   sample coinvolti: {len({r[0] for r in rows})}")
    lines.append("")
    lines.append("== ARITA (stessa funzione, arità diversa nel canonico) ==")
    for r in rows:
        if r[4] == "ARITA":
            lines.append(f"  {r[0]:<62} {r[1]}.{r[2]}({r[3]})  canonico: {r[5]}")
    lines.append("")
    lines.append("== ASSENTE (funzione non usata nel canonico) ==")
    for r in rows:
        if r[4] == "ASSENTE":
            lines.append(f"  {r[0]:<62} {r[1]}.{r[2]}({r[3]})")
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    open(args.out, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("\n".join(lines[:8]))
    print(f"\nscritto {args.out}")


if __name__ == "__main__":
    main()
