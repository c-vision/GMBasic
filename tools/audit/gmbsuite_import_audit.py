#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-PUL-1.1
"""gmbsuite_import_audit.py — audit dell'import delle librerie gmbsuite nel monorepo.

Perche' esiste
--------------
Le librerie C di `gmbsuite` vengono portate in `GMBasicMonoRepo` una alla volta (FFI in
`shared/libs/`, pacchetti in `packages/`, oppure implementate come builtin). Questo tool
verifica, simbolo per simbolo, che **nulla sia stato tralasciato**: sorgenti, header,
export BASIC `.inc` e funzioni C definite.

Sezioni del report
------------------
1. `migrated_pair`  — copia storica (`gmbsuite-implemented`) vs copia viva nel monorepo:
                      header, file sorgente e simboli definiti. `GAP` = manca qualcosa.
                      Include i pacchetti (`netlb`, `redis`, ...).
2. `ffi_internal`   — ogni `shared/libs/<nome>`: alias `.inc` e header devono essere
                      DEFINITI nei sorgenti; se il dylib e' gia' costruito, gli alias
                      devono anche essere ESPORTATI dal binario (nm).
3. `package_internal` — ogni `packages/<nome>`: come sopra; `NO-C-SOURCE` = pacchetto
                      con export `.inc` e nessun sorgente C versionato (solo header e
                      binario precompilato).
4. `project_inventory` — ogni progetto di `gmbsuite`: inventario header/definizioni e
                      match per NOME contro la superficie del monorepo (registry stdlib +
                      nomi/alias `.inc` + header lib + funzioni C `gmb_*`/`*_*` del
                      runtime). Il match per nome e' EURISTICO: serve al triage, la prova
                      di completezza e' nelle sezioni 1-3.

Uso
---
    python3 tools/audit/gmbsuite_import_audit.py \
        --gmbsuite ~/Documents/Projects/GMBSuite \
        --implemented ~/Documents/Projects/gmbsuite-implemented \
        --monorepo . \
        [--tsv tools/reports/gmbsuite-import-audit.tsv] \
        [--md tools/reports/gmbsuite-import-audit.md]

Exit code: 0 = nessuna lacuna nelle sezioni esatte (1-3), 1 = lacune trovate.
Nessuna variabile d'ambiente: solo argomenti.
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re
import subprocess
import sys

# ── parsing C ────────────────────────────────────────────────────────────────────────────

def _strip_c(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"(?m)^[ \t]*#.*$", "", text)
    return text


# Prototipo: "<tipo...> nome(args);" — prende l'ULTIMO identificatore prima di '('.
_DECL = re.compile(
    r"(?m)^[ \t]*([A-Za-z_][A-Za-z0-9_ \t\*]*?)\b([A-Za-z_]\w*)[ \t]*\(([^;{}]*)\)[ \t]*;"
)
_DEF = re.compile(
    r"(?m)^[A-Za-z_][A-Za-z0-9_ \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{"
)
_NOT_NAMES = {
    "if", "for", "while", "switch", "sizeof", "return", "do", "else",
    # tipi che il parser puo' scambiare per nomi (macro/function-pointer)
    "void", "int", "char", "long", "short", "float", "double", "unsigned", "signed",
    "const", "struct", "union", "enum", "bool", "_Bool", "size_t", "ssize_t",
    "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t",
    "uint64_t", "intptr_t", "uintptr_t", "ptrdiff_t", "FILE",
}


def header_symbols(paths: list[str]) -> set[str]:
    out: set[str] = set()
    for path in paths:
        try:
            text = _strip_c(open(path, encoding="utf-8", errors="ignore").read())
        except OSError:
            continue
        for m in _DECL.finditer(text):
            name = m.group(2)
            if name not in _NOT_NAMES:
                out.add(name)
    return out


def defined_symbols(paths: list[str]) -> set[str]:
    out: set[str] = set()
    for path in paths:
        try:
            text = _strip_c(open(path, encoding="utf-8", errors="ignore").read())
        except OSError:
            continue
        for m in _DEF.finditer(text):
            name = m.group(1)
            if name not in _NOT_NAMES:
                out.add(name)
    return out


# ── parsing .inc (export BASIC / FFI) ────────────────────────────────────────────────────

_INC_ALIAS = re.compile(
    r'DECLARE\s+FUNCTION\s+([A-Za-z_]\w*)[^\n]*?ALIAS\s+"([A-Za-z_]\w*)"', re.I
)
_INC_PLAIN = re.compile(r'DECLARE\s+FUNCTION\s+([A-Za-z_]\w*)[^\n]*?LIB\s+"', re.I)
_INC_NS = re.compile(r"^\s*NAMESPACE\s+([A-Za-z_.]\w*)", re.M | re.I)


def inc_symbols(paths: list[str]) -> tuple[set[str], set[str], set[str]]:
    basic: set[str] = set()
    aliases: set[str] = set()
    namespaces: set[str] = set()
    for path in paths:
        try:
            text = open(path, encoding="utf-8", errors="ignore").read()
        except OSError:
            continue
        for line in text.splitlines():
            m = _INC_ALIAS.search(line)
            if m:
                basic.add(m.group(1))
                aliases.add(m.group(2))
                continue
            m = _INC_PLAIN.search(line)
            if m:
                # Senza ALIAS il simbolo C e' il nome dichiarato com'e' (ABI "uppercase"
                # usata da alcuni pacchetti: `DECLARE FUNCTION CONNECT LIB "redis"` -> CONNECT).
                basic.add(m.group(1))
                aliases.add(m.group(1))
        for m in _INC_NS.finditer(text):
            namespaces.add(m.group(1))
    return basic, aliases, namespaces


# ── superfici / normalizzazione ─────────────────────────────────────────────────────────

PREFIXES = [
    "gm_special_", "gm_linalg_", "gm_vector_", "gm_numerical_", "gm_core_",
    "gm_vec_", "gm_num_", "gm_",
    "gmbstats_", "gs_stats2_", "gs_stats_", "gs_dist_", "gs_sim_", "gs_regression_", "gs_",
    "gc_", "gor_", "gnn_", "nn_", "gfx_", "gg_", "gsc_", "h5d_", "h5_",
    "gmbmedia_", "gmbgrpc_", "gmbamqp_", "gmbnats_", "gmbkafka_", "gmbmqtt_",
    "gmbprotobuf_", "gmbnetlb_", "gmbredis_", "redis_", "mm_", "gmb_",
]


def normalize(name: str) -> str:
    low = name.lower()
    for pref in PREFIXES:
        if low.startswith(pref):
            return low[len(pref):]
    return low


def load_registry(monorepo: str) -> set[str]:
    path = os.path.join(monorepo, "shared/spec/registry/stdlib-signatures.json")
    try:
        data = json.load(open(path, encoding="utf-8"))
    except (OSError, ValueError):
        return set()
    return {p["name"].lower() for p in data.get("procedures", []) if p.get("name")}


def monorepo_name_universe(monorepo: str) -> set[str]:
    inc_paths = (
        glob.glob(os.path.join(monorepo, "shared/include/*.inc"))
        + glob.glob(os.path.join(monorepo, "packages/**/*.inc"), recursive=True)
    )
    basic, aliases, _ = inc_symbols(inc_paths)
    universe = {n.lower() for n in basic} | {a.lower() for a in aliases} | load_registry(monorepo)

    # Symboli C del runtime/backend (per i builtin implementati in C: `gmb_erf` → `erf`).
    c_roots = [
        glob.glob(os.path.join(monorepo, "GMBasic/native/gmb_runtime_c/src/**/*.c"), recursive=True),
        glob.glob(os.path.join(monorepo, "GMBasic/native/gmb_vm_c/*.c")),
        glob.glob(os.path.join(monorepo, "GMBasic/native/gmb_db_c/src/**/*.c"), recursive=True),
        glob.glob(os.path.join(monorepo, "shared/libs/*/src/**/*.c"), recursive=True),
        glob.glob(os.path.join(monorepo, "shared/libs/*/include/**/*.h"), recursive=True),
        glob.glob(os.path.join(monorepo, "packages/**/*.h"), recursive=True),
    ]
    c_names: set[str] = set()
    for path in (p for group in c_roots for p in group):
        try:
            text = _strip_c(open(path, encoding="utf-8", errors="ignore").read())
        except OSError:
            continue
        for m in _DEF.finditer(text):
            if m.group(1) not in _NOT_NAMES:
                c_names.add(m.group(1))
    universe |= {normalize(n) for n in c_names}
    return universe


# ── filesystem ──────────────────────────────────────────────────────────────────────────

def src_files(root: str) -> list[str]:
    out: list[str] = []
    for pattern in ("src/*.c", "src/**/*.c", "*/src/*.c", "*/src/**/*.c", "*.c"):
        out += glob.glob(os.path.join(root, pattern), recursive=True)
    # esclude sorgenti VENDORIZZATI di terze parti (.build/) e la cache contenuti
    return sorted(p for p in set(out) if "/.build/" not in p and "/.content/" not in p)


def hdr_files(root: str) -> list[str]:
    out: list[str] = []
    for pattern in ("include/*.h", "include/**/*.h", "*/include/*.h", "*/include/**/*.h", "*.h"):
        out += glob.glob(os.path.join(root, pattern), recursive=True)
    return sorted(p for p in set(out) if "/.build/" not in p and "/.content/" not in p)


def file_set(root: str, exts: tuple[str, ...]) -> set[str]:
    out: set[str] = set()
    for pattern in ("src/**/*", "include/**/*", "*.*"):
        for path in glob.glob(os.path.join(root, pattern), recursive=True):
            if os.path.isfile(path) and path.lower().endswith(exts):
                out.add(os.path.relpath(path, root))
    return out


def dylib_exports(lib_dir: str) -> set[str] | None:
    """Simboli esportati dal dylib gia' costruito (se presente). None se assente/nm fallisce."""
    candidates = glob.glob(os.path.join(lib_dir, "*.dylib")) + glob.glob(os.path.join(lib_dir, "*.so"))
    for lib in candidates:
        try:
            out = subprocess.run(["nm", "-gU", lib], capture_output=True, text=True, timeout=60)
        except (OSError, subprocess.SubprocessError):
            continue
        if out.returncode != 0:
            continue
        syms = set()
        for line in out.stdout.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[-2] in ("T", "D", "B"):
                syms.add(parts[-1].lstrip("_"))
        return syms
    return None


def collect_projects(gmbsuite: str) -> list[str]:
    skip = {"betting", "tools", "config", "docs", "data", ".git", ".opencode"}
    projects = []
    for name in sorted(os.listdir(gmbsuite)):
        d = os.path.join(gmbsuite, name)
        if not os.path.isdir(d) or name in skip:
            continue
        if (os.path.isdir(os.path.join(d, "include")) and os.path.isdir(os.path.join(d, "src"))) \
           or glob.glob(os.path.join(d, "**/*.c"), recursive=True):
            projects.append(name)
    betting = os.path.join(gmbsuite, "betting")
    if os.path.isdir(betting):
        for name in sorted(os.listdir(betting)):
            d = os.path.join(betting, name)
            if os.path.isdir(d) and glob.glob(os.path.join(d, "**/*.c"), recursive=True):
                projects.append(f"betting/{name}")
    return projects


# ── audit ───────────────────────────────────────────────────────────────────────────────

MIGRATED_PAIRS = [
    ("media", "gmbmedia", "shared/libs/media"),
    ("grpc", "net/grpc", "shared/libs/grpc"),
    ("amqp", "net/amqp", "shared/libs/amqp"),
    ("nats", "net/nats", "shared/libs/nats"),
    ("kafka", "net/kafka", "shared/libs/kafka"),
    ("mqtt", "net/mqtt", "shared/libs/mqtt"),
    ("protobuf", "net/protobuf", "shared/libs/protobuf"),
]
MIGRATED_PACKAGES = [
    ("netlb", "net/gmbnet/libs/netlb", "packages/netlb"),
    ("redis", "net/gmbnet/libs/redis", "packages/redis"),
]


def audit(monorepo: str, gmbsuite: str, implemented: str) -> tuple[list[dict], dict]:
    rows: list[dict] = []
    summary: dict = {"gaps_exact": 0, "projects": {}}

    def row(section, project, source, item, status, detail=""):
        rows.append(dict(section=section, project=project, source=source, item=item,
                         status=status, detail=detail))

    def audit_pair(label, old_dir, new_dir):
        old_h = header_symbols(hdr_files(old_dir))
        new_h = header_symbols(hdr_files(new_dir))
        old_defs = defined_symbols(src_files(old_dir))
        new_defs = defined_symbols(src_files(new_dir))
        # `.inc` e `.gitignore` cambiano posto per contratto (export in shared/include/),
        # quindi non contano come file mancanti; gli alias .inc sono confrontati sotto.
        old_src = {p for p in file_set(old_dir, (".c", ".h", ".inc", ".bas", ".sh", ".md"))
                   if not p.endswith(".inc") and not p.endswith(".gitignore")}
        new_src = {p for p in file_set(new_dir, (".c", ".h", ".inc", ".bas", ".sh", ".md"))
                   if not p.endswith(".inc") and not p.endswith(".gitignore")}
        missing_h = sorted(old_h - new_h)
        missing_def = sorted(old_defs - new_defs)
        missing_files = sorted(old_src - new_src)
        old_inc = glob.glob(os.path.join(old_dir, "*.inc")) + glob.glob(os.path.join(old_dir, "include/*.inc"))
        _, old_aliases, _ = inc_symbols(old_inc)
        new_inc = glob.glob(os.path.join(new_dir, "*.inc")) + glob.glob(os.path.join(new_dir, "include/*.inc"))
        if old_aliases:  # l'.inc vive in shared/include/<lib>.inc: lo risolve chi chiama
            inc_name = {"opt": "optimize.inc", "or": "ortools.inc"}.get(
                label, f"{label}.inc")
            monorepo_inc = os.path.join(monorepo, "shared/include", inc_name)
            _, mon_aliases, _ = inc_symbols([monorepo_inc] if os.path.exists(monorepo_inc) else new_inc)
            missing_aliases = sorted(old_aliases - mon_aliases)
        else:
            missing_aliases = []
        gaps = bool(missing_h or missing_def or missing_files or missing_aliases)
        if gaps:
            summary["gaps_exact"] += 1
        row("migrated_pair", label, "summary",
            f"h old={len(old_h)}/new={len(new_h)} · def old={len(old_defs)}/new={len(new_defs)} "
            f"· file old={len(old_src)}/new={len(new_src)}",
            "GAP" if gaps else "OK",
            f"header_mancanti={len(missing_h)} def_mancanti={len(missing_def)} "
            f"file_mancanti={len(missing_files)} alias_mancanti={len(missing_aliases)}")
        for sym in missing_h:
            row("migrated_pair", label, "header", sym, "MISSING-HEADER",
                "dichiarato nella copia storica, non nel monorepo")
        for sym in missing_def:
            row("migrated_pair", label, "def", sym, "MISSING-DEF",
                "definito nella copia storica, non nel monorepo")
        for path in missing_files:
            row("migrated_pair", label, "file", path, "MISSING-FILE",
                "file della copia storica assente nel monorepo")
        for sym in missing_aliases:
            row("migrated_pair", label, "inc", sym, "MISSING-ALIAS",
                "export BASIC dello storico assente nel .inc del monorepo")
        if not gaps:
            return
        # dettaglio: se il monorepo ha il binario ma non i sorgenti
        if not new_defs and new_src:
            row("migrated_pair", label, "src", "-", "NO-C-SOURCE",
                "nel monorepo non c'e' nessun .c: la lib/pacchetto e' solo header + binario")

    # 1) coppie migrate: storica vs viva ------------------------------------------------
    for label, old_rel, new_rel in MIGRATED_PAIRS:
        audit_pair(label, os.path.join(implemented, old_rel), os.path.join(monorepo, new_rel))
    for label, old_rel, new_rel in MIGRATED_PACKAGES:
        audit_pair(label, os.path.join(implemented, old_rel), os.path.join(monorepo, new_rel))

    # 2) FFI libs: coerenza interna + nm -------------------------------------------------
    inc_dir = os.path.join(monorepo, "shared/include")
    for lib_dir in sorted(glob.glob(os.path.join(monorepo, "shared/libs/*"))):
        lib = os.path.basename(lib_dir)
        if lib == "win32":
            continue
        headers = header_symbols(hdr_files(lib_dir))
        defs = defined_symbols(src_files(lib_dir))
        inc_name = {"opt": "optimize.inc", "or": "ortools.inc"}.get(lib, f"{lib}.inc")
        inc_path = os.path.join(inc_dir, inc_name)
        basic, aliases, _ = inc_symbols([inc_path] if os.path.exists(inc_path) else [])
        exports = dylib_exports(lib_dir)
        undef_h = sorted(headers - defs)
        undef_a = sorted(aliases - defs)
        alias_no_header = sorted(aliases - headers)
        not_exported = sorted(aliases - exports) if exports is not None else []
        # La superficie PUBBLICA e' l'.inc (ABI): un header con dichiarazioni in piu'
        # (API interna/legacy) e' un WARN di igiene, non una lacuna dell'import.
        ok = not undef_a and not not_exported
        if not ok:
            summary["gaps_exact"] += 1
        row("ffi_internal", lib, os.path.relpath(lib_dir, monorepo),
            f"h={len(headers)} def={len(defs)} alias={len(aliases)}"
            f"{' exported=' + str(len(exports)) if exports is not None else ''}",
            "OK" if ok else "GAP",
            f"header_senza_def={len(undef_h)} alias_senza_def={len(undef_a)} "
            f"alias_senza_header={len(alias_no_header)} alias_non_esportati={len(not_exported)}")
        for sym in undef_h:
            row("ffi_internal", lib, "src", sym, "WARN-HEADER-NO-DEF",
                "dichiarato in header, non definito nei sorgenti (API interna/legacy?)")
        for sym in undef_a:
            row("ffi_internal", lib, inc_name, sym, "ALIAS-NO-DEF", "export BASIC senza definizione C")
        for sym in not_exported:
            row("ffi_internal", lib, "dylib", sym, "ALIAS-NOT-EXPORTED",
                "il dylib costruito non esporta il simbolo")

    # 3) packages: coerenza interna + nm ------------------------------------------------
    for pkg_dir in sorted(glob.glob(os.path.join(monorepo, "packages/*"))):
        if not os.path.isdir(pkg_dir):
            continue
        pkg = os.path.basename(pkg_dir)
        headers = header_symbols(hdr_files(pkg_dir))
        defs = defined_symbols(src_files(pkg_dir))
        incs = (glob.glob(os.path.join(pkg_dir, "*.inc"))
                + glob.glob(os.path.join(pkg_dir, "**/*.inc"), recursive=True))
        basic, aliases, _ = inc_symbols(incs)
        exports = dylib_exports(pkg_dir)
        undef_a = sorted(aliases - defs) if defs else []
        not_exported = sorted(aliases - exports) if exports is not None else []
        no_source = bool(aliases) and not defs
        undef_h = sorted(headers - defs) if defs else []
        ok = not no_source and not undef_a and not not_exported
        if not ok:
            summary["gaps_exact"] += 1
        status = "OK" if ok else ("NO-C-SOURCE" if no_source else "GAP")
        row("package_internal", pkg, os.path.relpath(pkg_dir, monorepo),
            f"h={len(headers)} def={len(defs)} alias={len(aliases)}"
            f"{' exported=' + str(len(exports)) if exports is not None else ''}",
            status,
            f"header_senza_def={len(undef_h)} alias_senza_def={len(undef_a)} "
            f"alias_non_esportati={len(not_exported)}")
        for sym in undef_h:
            row("package_internal", pkg, "src", sym, "WARN-HEADER-NO-DEF", "")
        for sym in undef_a:
            row("package_internal", pkg, "inc", sym, "ALIAS-NO-DEF", "")
        for sym in not_exported:
            row("package_internal", pkg, "lib", sym, "ALIAS-NOT-EXPORTED", "")

    # 4) inventario di OGNI progetto gmbsuite + match per nome -------------------------
    universe = monorepo_name_universe(monorepo)
    # seconda chiave senza underscore: `gs_dist_normal_pdf` → `normapdf` vs `NormalPdf`
    universe_flat = {n.replace("_", "") for n in universe}
    for project in collect_projects(gmbsuite):
        base = os.path.join(gmbsuite, project)
        headers = header_symbols(hdr_files(base))
        defs = defined_symbols(src_files(base))
        norm_names = {normalize(s) for s in headers}
        matched = {n for n in norm_names if n in universe or n.replace("_", "") in universe_flat}
        unmatched = sorted(norm_names - matched)
        summary["projects"][project] = {
            "headers": len(headers), "defined": len(defs),
            "matched": len(matched), "unmatched": len(unmatched),
        }
        row("project_inventory", project, base,
            f"h={len(headers)} def={len(defs)} match_nome={len(matched)} non_match={len(unmatched)}",
            "INFO", "")
        for sym in sorted(headers):
            nm = normalize(sym)
            is_match = nm in universe or nm.replace("_", "") in universe_flat
            row("project_inventory", project, "header", sym,
                "MATCH" if is_match else "UNMATCHED", f"norm={nm}")

    return rows, summary


def write_tsv(path: str, rows: list[dict]) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("section\tproject\tsource\titem\tstatus\tdetail\n")
        for r in rows:
            f.write("\t".join(str(r[k]).replace("\t", " ") for k in
                              ("section", "project", "source", "item", "status", "detail")) + "\n")


def write_md(path: str, rows: list[dict], summary: dict) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    migrated = [r for r in rows if r["section"] == "migrated_pair" and r["source"] == "summary"]
    ffi = [r for r in rows if r["section"] == "ffi_internal" and r["source"].startswith("shared/")]
    pkg = [r for r in rows if r["section"] == "package_internal" and r["source"].startswith("packages/")]
    with open(path, "w", encoding="utf-8") as f:
        f.write("# Audit import gmbsuite → GMBasicMonoRepo\n\n")
        f.write(f"Lacune esatte (sezioni 1–3): **{summary['gaps_exact']}**\n\n")
        f.write("## 1. Copie migrate (storico vs vivo)\n\n")
        f.write("| lib/pacchetto | esito | header/def/file | dettaglio |\n|---|---|---|---|\n")
        for r in migrated:
            f.write(f"| {r['project']} | {r['status']} | {r['item']} | {r['detail']} |\n")
        f.write("\n## 2. Lib FFI del monorepo — coerenza interna + binario\n\n")
        f.write("| lib | esito | h/def/alias | note |\n|---|---|---|---|\n")
        for r in ffi:
            f.write(f"| {r['project']} | {r['status']} | {r['item']} | {r['detail']} |\n")
        f.write("\n## 3. Pacchetti del monorepo\n\n")
        if pkg:
            f.write("| pacchetto | esito | h/def/alias | note |\n|---|---|---|---|\n")
            for r in pkg:
                f.write(f"| {r['project']} | {r['status']} | {r['item']} | {r['detail']} |\n")
        else:
            f.write("_nessun pacchetto_\n")
        f.write("\n## 4. Inventario progetti gmbsuite (match per nome: euristico)\n\n")
        f.write("| progetto | header | definiti | match nome | non match |\n|---|---:|---:|---:|---:|\n")
        for proj, st in summary["projects"].items():
            f.write(f"| {proj} | {st['headers']} | {st['defined']} | {st['matched']} | {st['unmatched']} |\n")
        f.write("\n> Il match per nome usa registry stdlib + nomi/alias `.inc` + header lib + "
                "funzioni C del runtime. Serve al triage: la prova di completezza è nelle sezioni 1–3.\n")


def main() -> int:
    ap = argparse.ArgumentParser(description="Audit import gmbsuite → monorepo")
    home = os.path.expanduser("~")
    ap.add_argument("--gmbsuite", default=os.path.join(home, "Documents/Projects/GMBSuite"))
    ap.add_argument("--implemented", default=os.path.join(home, "Documents/Projects/gmbsuite-implemented"))
    ap.add_argument("--monorepo", default=os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))
    ap.add_argument("--tsv", default=None)
    ap.add_argument("--md", default=None)
    args = ap.parse_args()

    rows, summary = audit(os.path.abspath(args.monorepo), os.path.abspath(args.gmbsuite),
                          os.path.abspath(args.implemented))
    tsv = args.tsv or os.path.join(args.monorepo, "tools/reports/gmbsuite-import-audit.tsv")
    md = args.md or os.path.join(args.monorepo, "tools/reports/gmbsuite-import-audit.md")
    write_tsv(tsv, rows)
    write_md(md, rows, summary)

    print(f"report: {md}")
    print(f"tsv:    {tsv}")
    print(f"lacune esatte: {summary['gaps_exact']}")
    for proj, st in summary["projects"].items():
        print(f"  {proj}: h={st['headers']} def={st['defined']} "
              f"match={st['matched']} non_match={st['unmatched']}")
    return 0 if summary["gaps_exact"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
