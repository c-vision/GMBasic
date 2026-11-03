#!/usr/bin/env python3
"""sample_audit.py — CENSIMENTO dei Sample (shared/samples/**), non dei MiniTest.

Contratto (Guida Test/Sample §7.3): un Sample è un esempio COMPLETO e realistico che
INSEGNA un uso concreto (≥2 domini, scenario riconoscibile), senza `' EXPECT:`.
Non tutti i file sotto shared/samples sono Sample: `Modules/**` contiene moduli importati
(nessun entry point), `Fixtures/**` dati, `packages/**` artefatti dei pacchetti.

Classificazione emessa:
- SURFACE     : tag `' SURFACE:` in testa (drill di superficie, da riscrivere o accettare)
- MODULO      : sotto Modules/ o senza punto d'ingresso (file importati)
- FIXTURE     : sotto Fixtures/
- PACKAGE     : sotto packages/
- DIDATTICO   : ≥ CAMPIONE_RIGHE righe e ≥2 IMPORT (candidato scenario)
- CORTO       : < CAMPIONE_RIGHE righe (candidato a revisione/accorpamento)
- ENV         : categoria dipendente da rete/UI/modelli (oracolo non deterministico)

Uso (dalla root del monorepo):
    python3 tools/sample_audit.py [--root shared/samples] [--out tools/reports/sample_audit.txt]
"""
import argparse
import collections
import glob
import os
import re

CAMPIONE_RIGHE = 20
ENV_CATEGORIES = {
    "Ai", "AiLLM", "Audio", "Browser", "Computer", "Connectors", "Http", "Network",
    "cua", "graphics", "windows", "Controls", "Console", "GameDev", "Team", "Web",
    "mvc",
}


def classify(path: str, root: str):
    rel = os.path.relpath(path, root)
    cat = rel.split(os.sep)[0]
    text = open(path, encoding="utf-8", errors="replace").read()
    lines = text.splitlines()
    imports = set(re.findall(r"^IMPORT\s+([\w.]+)", text, re.M))
    tags = []
    if lines and lines[0].startswith("' SURFACE:"):
        tags.append("SURFACE")
    if cat in ("Modules", "Fixtures", "packages") or "include" in rel.split(os.sep):
        tags.append({"Modules": "MODULO", "Fixtures": "FIXTURE", "packages": "PACKAGE"}.get(
            cat, "SUPPORTO"))
    if any(re.match(r"^\s*'\s*EXPECT:", l) for l in lines):
        tags.append("EXPECT")
    if len(lines) < CAMPIONE_RIGHE:
        tags.append("CORTO")
    if cat in ENV_CATEGORIES:
        tags.append("ENV")
    if not tags and len(imports) >= 2:
        tags.append("DIDATTICO")
    return {
        "path": rel,
        "cat": cat,
        "lines": len(lines),
        "imports": sorted(imports),
        "tags": tags,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default="shared/samples")
    ap.add_argument("--out", default="tools/reports/sample_audit.txt")
    args = ap.parse_args()

    files = sorted(glob.glob(os.path.join(args.root, "**", "*.bas"), recursive=True))
    rows = [classify(f, args.root) for f in files]
    by_cat = collections.Counter(r["cat"] for r in rows)
    by_tag = collections.Counter(t for r in rows for t in r["tags"])

    out = []
    out.append("SAMPLE AUDIT — shared/samples/**/*.bas")
    out.append(f"totale file: {len(rows)}   categorie: {len(by_cat)}")
    out.append("")
    out.append("== per categoria ==")
    for cat, n in sorted(by_cat.items(), key=lambda kv: (-kv[1], kv[0])):
        out.append(f"  {cat:<20} {n}")
    out.append("")
    out.append("== per classificazione (un file può avere più tag) ==")
    for tag, n in sorted(by_tag.items(), key=lambda kv: (-kv[1], kv[0])):
        out.append(f"  {tag:<12} {n}")
    out.append("")

    def section(title, tag):
        out.append(f"== {title} ==")
        sel = [r for r in rows if tag in r["tags"]]
        for r in sel:
            out.append(f"  {r['path']:<70} righe={r['lines']:<4} import={len(r['imports'])}")
        out.append(f"  ({len(sel)} file)")
        out.append("")

    section("SURFACE (drill di superficie)", "SURFACE")
    section("EXPECT nei Sample (violazione del contratto)", "EXPECT")
    section("CORTO (< %d righe, revisione/accorpamento)" % CAMPIONE_RIGHE, "CORTO")
    section("ENV (rete/UI/modelli: oracolo non deterministico)", "ENV")
    section("DIDATTICO (≥2 domini, ≥ %d righe)" % CAMPIONE_RIGHE, "DIDATTICO")

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    open(args.out, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print("\n".join(out[:12]))
    print(f"\nscritto {args.out}")


if __name__ == "__main__":
    main()
