#!/usr/bin/env python3
"""skills_attribution.py — build shared/skills/ATTRIBUTION.md + attribution.json.

WHY. The skills in `shared/skills/` were collected from local installations of other AI
tools (Claude Code, Codex, opencode, Gemini, ~/.agents/skills). They are THIRD-PARTY
material: before the repository is published, each one must be traceable to an author and a
license that allows redistribution. This script does not invent attributions: it reports
**only what the files themselves declare** (frontmatter `license:`, `author:`,
`metadata.author`, URLs) and classifies every skill as

    DECLARED-PERMISSIVE    a license that allows redistribution (MIT / Apache-2.0 / BSD / ...)
    DECLARED-PROPRIETARY   a license that forbids redistribution -> must leave the public tree
    UNDECLARED             no license in the file -> legally "all rights reserved"

Usage (from the repository root):
    python3 tools/py/skills_attribution.py           # writes the two files in shared/skills/
    python3 tools/py/skills_attribution.py --check   # exit 1 if non-redistributable skills exist

The output files are GENERATED: edit this script, not them.
"""
from __future__ import annotations

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SKILLS = ROOT / "shared" / "skills"
SKIP_DIRS = {"licenses"}   # testi di licenza, non skill

PERMISSIVE = {
    "mit": "MIT",
    "apache": "Apache-2.0",
    "bsd-3": "BSD-3-Clause",
    "bsd": "BSD",
    "isc": "ISC",
    "cc0": "CC0-1.0",
    "cc-by-4": "CC-BY-4.0",
    "cc-by": "CC-BY",
    "unlicense": "Unlicense",
}

# Firme di VENDOR riconoscibili (progetto noto da cui la skill è stata presa): solo
# prefissi inequivocabili, il resto non è deducibile in modo affidabile dal testo.
# Il match è sul PREFISSO del nome (nessuna euristica sul contenuto: il testo di una skill cita
# spesso gli strumenti, non l'autore).
VENDORS = [
    ("goose", "GooseWorks"),
    ("gsd", "GSD (~/.agents/skills)"),
]


def parse_frontmatter(text: str) -> dict[str, str]:
    """Frontmatter YAML piatto + un livello di annidamento (`metadata:`)."""
    m = re.match(r"^---\n(.*?)\n---", text, re.S)
    if not m:
        return {}
    out: dict[str, str] = {}
    parent = ""
    for line in m.group(1).splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        indented = line[:1] in (" ", "\t")
        if ":" not in line:
            continue
        k, v = line.split(":", 1)
        k, v = k.strip().lower(), v.strip().strip('"').strip("'")
        if not indented:
            parent = k
            if v:
                out[k] = v
            else:
                out.setdefault(k, "")
        elif parent:
            out[f"{parent}.{k}"] = v
    return out


def classify(name: str, text: str) -> dict:
    fm = parse_frontmatter(text)
    raw_license = (fm.get("license") or fm.get("metadata.license") or "").strip()
    low = raw_license.lower()

    status, license_name = "UNDECLARED", ""
    if raw_license:
        for key, canon in PERMISSIVE.items():
            if key in low:
                status, license_name = "DECLARED-PERMISSIVE", canon
                break
        else:
            if "proprietary" in low or "all rights reserved" in low:
                status, license_name = "DECLARED-PROPRIETARY", raw_license[:60]
            # "terms in LICENSE.txt" / "see LICENSE in repository root": la licenza non è nel
            # file -> non verificabile -> resta UNDECLARED.

    author = (fm.get("author") or fm.get("metadata.author") or "").strip()
    version = (fm.get("version") or fm.get("metadata.version") or "").strip()

    vendor = ""
    for key, label in VENDORS:
        if name.startswith(key):
            vendor = label
            break

    urls = re.findall(r"https?://[^\s)\]\"'<>`]+", text[:4000])
    home = (fm.get("homepage") or fm.get("metadata.homepage")
            or fm.get("repository") or fm.get("metadata.repository") or "").strip()

    return {
        "skill": name,
        "status": status,
        "license": license_name,
        "author": author,
        "version": version,
        "vendor": vendor,
        "homepage": home,
        "urls": sorted(set(urls))[:3],
    }


def main() -> int:
    rows = []
    for d in sorted(p for p in SKILLS.iterdir() if p.is_dir()):
        if d.name in SKIP_DIRS:      # cartelle di servizio (testi di licenza), non skill
            continue
        f = d / "SKILL.md"
        if not f.exists():
            rows.append({"skill": d.name, "status": "NO-SKILL.md", "license": "", "author": "",
                         "version": "", "vendor": "", "homepage": "", "urls": []})
            continue
        rows.append(classify(d.name, f.read_text(errors="ignore")))

    def n(status: str) -> int:
        return sum(1 for r in rows if r["status"] == status)

    permissive = [r for r in rows if r["status"] == "DECLARED-PERMISSIVE"]
    proprietary = [r for r in rows if r["status"] == "DECLARED-PROPRIETARY"]
    undeclared = [r for r in rows if r["status"] == "UNDECLARED"]
    broken = [r for r in rows if r["status"] == "NO-SKILL.md"]
    with_author = [r for r in permissive if r["author"]]

    def table(items: list[dict], with_author_col: bool = True) -> list[str]:
        if with_author_col:
            out = ["| skill | author | license | version |", "|---|---|---|---|"]
            for r in items:
                out.append(f"| `{r['skill']}` | {r['author'] or '—'} | {r['license']} "
                           f"| {r['version'] or '—'} |")
        else:
            out = ["| skill | declared | family |", "|---|---|---|"]
            for r in items:
                out.append(f"| `{r['skill']}` | {r['license'] or '—'} | {r['vendor'] or '—'} |")
        return out

    lines = [
        "# Skills — attribution and license status",
        "",
        "> **GENERATED** by [`tools/py/skills_attribution.py`](../../tools/py/skills_attribution.py) —",
        "> do not edit by hand. Re-run it after adding or removing a skill.",
        "",
        "The skills in this folder are **third-party material**, collected from local installs of other",
        "AI tools (Claude Code, Codex, opencode, Gemini, `~/.agents/skills`). This file records **only",
        "what each skill's own file declares** (`author`, `license`, `version`); nothing is inferred.",
        "",
        "## Status summary",
        "",
        "| status | meaning | n. |",
        "|---|---|---|",
        "| `DECLARED-PERMISSIVE` | declares a license that allows redistribution | "
        f"{n('DECLARED-PERMISSIVE')} |",
        "| `DECLARED-PROPRIETARY` | declares a license that **forbids** redistribution | "
        f"{n('DECLARED-PROPRIETARY')} |",
        "| `UNDECLARED` | no license in the file — legally **all rights reserved** | "
        f"{n('UNDECLARED')} |",
        "",
        f"Of the {len(permissive)} redistributable skills, **{len(with_author)}** also declare the",
        f"**author** (`author:` / `metadata.author:`); for the remaining {len(permissive) - len(with_author)}",
        "the author is not stated in the file.",
        "",
        "## Redistributable skills (declared permissive license)",
        "",
        "Each row keeps the license its own file declares. The **MIT** ones need the MIT text and",
        "the copyright line of the author named in the row: see",
        "[`licenses/MIT.txt`](licenses/MIT.txt).",
        "",
        *table(permissive),
        "",
        "## Skills that must NOT be published as they are",
        "",
        "These carry a **proprietary** license or **no license at all**. With no explicit grant,",
        "redistribution is not allowed: the entry is here so the decision is explicit (remove from",
        "the public tree, or obtain permission from the author) instead of accidental.",
        "",
        f"### Proprietary ({len(proprietary)})",
        "",
        *table(proprietary, with_author_col=False),
        "",
        f"### No license declared ({len(undeclared)})",
        "",
        *table(undeclared, with_author_col=False),
    ]
    if broken:
        lines += ["", f"### Missing `SKILL.md` ({len(broken)})", "",
                  *[f"- `{r['skill']}`" for r in broken]]

    (SKILLS / "ATTRIBUTION.md").write_text("\n".join(lines) + "\n")
    (SKILLS / "attribution.json").write_text(json.dumps(rows, indent=2) + "\n")

    print(f"skills: {len(rows)}")
    print(f"  DECLARED-PERMISSIVE  {n('DECLARED-PERMISSIVE'):3}  (con autore dichiarato: {len(with_author)})")
    print(f"  DECLARED-PROPRIETARY {n('DECLARED-PROPRIETARY'):3}")
    print(f"  UNDECLARED           {n('UNDECLARED'):3}")

    if "--check" in sys.argv:
        blocking = [r["skill"] for r in rows
                    if r["status"] in ("DECLARED-PROPRIETARY", "UNDECLARED", "NO-SKILL.md")]
        if blocking:
            print(f"\nATTENZIONE: {len(blocking)} skill non ridistribuibili nel tree:", file=sys.stderr)
            for s in blocking[:10]:
                print("  " + s, file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
