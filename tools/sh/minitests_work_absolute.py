#!/usr/bin/env python3
"""minitests_work_absolute.py — rende assoluti i path `work/` dei MiniTest.

PERCHE'
`"work/..."` e' relativa alla CWD del processo. Gli harness del gate (C99Native, IL,
PCode, VM-C) impostano la CWD sulla cartella del .bas, quindi oggi gli artefatti
nascono accanto al programma; ma un eseguibile lanciato a mano da un'altra cartella
creerebbe `work/` li'. Questo strumento riscrive i literal di CODICE in

    GMBWORK$ = PROJECTFOLDER() + "/work"      (dichiarata in testa al programma)
    "work/x"          ->  GMBWORK$ + "/x"
    "work"            ->  GMBWORK$

COSA NON CONVERTE (di proposito, e lo dichiara nel report)
1. Path dentro una stringa JSON (`"{""workspace"":""work/wev1""}"`): non sono
   interpolabili con un'espressione senza riscrivere il JSON come concatenazione.
2. Path STAMPATI in output (`PRINT ... "work/..."` oppure una riga `' EXPECT:` che
   contiene `work/`): passando un path assoluto l'output mostrerebbe
   `/Users/<utente>/...` e l'oracolo diventerebbe dipendente dalla macchina, contro
   la regola dei MiniTest (output deterministico, confrontabile sulle 4 gambe).
Per quei file resta la forma relativa, che gli harness risolvono gia' nella cartella
del programma.

USO
    tools/sh/minitests_work_absolute.py                 # dry-run (default)
    tools/sh/minitests_work_absolute.py --apply         # riscrive (backup in /tmp)
    tools/sh/minitests_work_absolute.py --chdir --apply # i 48 restanti: CHDIR PROJECTFOLDER()

MODO --chdir
I file che restano relativi (EXPECT/PRINT/JSON, vedi sopra) ricevono una riga

    CHDIR PROJECTFOLDER()

dopo `SUB Main()` (o dopo l'ultimo `IMPORT` nei test top-level): i path restano
relativi, quindi l'output stampato NON cambia (l'oracolo resta portabile), ma si
risolvono sulla cartella del programma invece che sulla CWD del processo.
CHDIR e' implementato su tutte e 4 le gambe (C99Native `st_chdir` -> gmb_fs_chdir,
IL SetCurrentDirectory, PCode SysCall.CHDIR, VM/C syscall 268): verificato con
un probe che richiede l'effetto (CHDIR in sottocartella + FILEEXISTS relativo).
"""

from __future__ import annotations

import argparse
import os
import shutil
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "shared", "minitests"))
VAR = "GMBWORK$"
DECL = '  DIM %s AS STRING = PROJECTFOLDER() + "/work"' % VAR


def scan_literals(line: str):
    """Restituisce [(start, end, raw)] dei literal stringa BASIC della riga.

    Il dialetto raddoppia le virgolette dentro la stringa (""): la scansione le tratta
    come contenuto, non come fine del literal.
    """
    out = []
    i, n = 0, len(line)
    while i < n:
        if line[i] != '"':
            i += 1
            continue
        start = i
        i += 1
        while i < n:
            if line[i] == '"':
                if i + 1 < n and line[i + 1] == '"':
                    i += 2          # virgoletta raddoppiata = contenuto
                    continue
                i += 1              # chiusura del literal
                break
            i += 1
        out.append((start, i, line[start + 1:i - 1]))
    return out


def is_comment(line: str) -> bool:
    s = line.lstrip()
    return s.startswith("'")


def rewrite_line(line: str):
    """Riscrive i literal `work`/`work/...` in un'espressione. Ritorna (nuova, n)."""
    if is_comment(line):
        return line, 0
    pieces, last, hits = [], 0, 0
    for start, end, raw in scan_literals(line):
        if raw == "work":
            repl = VAR
        elif raw.startswith("work/"):
            rest = raw[5:]
            repl = '%s + "/%s"' % (VAR, rest) if rest else '%s + "/"' % VAR
        else:
            continue
        pieces.append(line[last:start])
        pieces.append(repl)
        last = end
        hits += 1
    if not hits:
        return line, 0
    pieces.append(line[last:])
    return "".join(pieces), hits


CHDIR_LINE = "  CHDIR PROJECTFOLDER()"


def plan_chdir(path: str):
    """Ritorna (righe_nuove, 1) oppure (None, motivo)."""
    with open(path, encoding="utf-8") as fh:
        lines = fh.read().split("\n")

    if any(CHDIR_LINE.strip() in l for l in lines):
        return None, "CHDIR PROJECTFOLDER() gia' presente"
    if any(VAR in l for l in lines):
        return None, "gia' convertito a %s" % VAR
    if not any('"work' in l and not is_comment(l) for l in lines):
        return None, "nessun path work/ relativo nel codice"

    main_at = last_import = None
    for idx, line in enumerate(lines):
        stripped = line.strip()
        if main_at is None and stripped.upper().startswith("SUB MAIN"):
            main_at = idx
        if stripped.upper().startswith("IMPORT "):
            last_import = idx
    at = main_at + 1 if main_at is not None else (last_import + 1 if last_import is not None else 0)
    lines.insert(at, CHDIR_LINE)
    return lines, 1


def plan(path: str):
    """Ritorna (righe_nuove, n_rewrite) oppure (None, motivo_di_esclusione)."""
    with open(path, encoding="utf-8") as fh:
        lines = fh.read().split("\n")

    for idx, line in enumerate(lines, 1):
        if line.startswith("' EXPECT:") and "work/" in line:
            return None, "EXPECT stampa work/ (riga %d)" % idx
    for idx, line in enumerate(lines, 1):
        if not is_comment(line) and "PRINT" in line and '"work' in line:
            return None, "PRINT di un path work/ (riga %d)" % idx
    for idx, line in enumerate(lines, 1):
        if '""work' in line:
            return None, "path dentro una stringa JSON (riga %d)" % idx

    new_lines, total = [], 0
    main_at = None
    last_import = None
    for idx, line in enumerate(lines):
        new, hits = rewrite_line(line)
        total += hits
        new_lines.append(new)
        stripped = line.strip()
        if main_at is None and stripped.upper().startswith("SUB MAIN"):
            main_at = idx
        if stripped.upper().startswith("IMPORT "):
            last_import = idx

    if not total:
        return None, "nessun literal da convertire"
    # Il controllo va fatto sulle righe ORIGINALI: la dichiarazione aggiunta sotto
    # contiene essa stessa il nome della variabile.
    if any(VAR in l for l in lines):
        return None, "la variabile %s e' gia' presente" % VAR

    at = main_at + 1 if main_at is not None else (last_import + 1 if last_import is not None else 0)
    new_lines.insert(at, DECL)
    return new_lines, total


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true", help="scrive i file (default: dry-run)")
    ap.add_argument("--chdir", action="store_true",
                    help="modo CHDIR: aggiunge CHDIR PROJECTFOLDER() ai file lasciati relativi")
    args = ap.parse_args()
    planner = plan_chdir if args.chdir else plan

    if not os.path.isdir(ROOT):
        print("cartella minitest non trovata: %s" % ROOT)
        return 1

    stamp = time.strftime("%Y%m%d-%H%M%S")
    backup = "/tmp/gmb-backup-%s-%s" % ("chdir" if args.chdir else "workabs", stamp)
    changed, skipped = [], []

    for dirpath, _dirs, names in os.walk(ROOT):
        for name in sorted(names):
            if not name.endswith(".bas"):
                continue
            path = os.path.join(dirpath, name)
            with open(path, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            if '"work' not in text:
                continue
            lines, info = planner(path)
            rel = os.path.relpath(path, ROOT)
            if lines is None:
                skipped.append((rel, info))
                continue
            changed.append((rel, info))

    print("file da modificare: %d" % len(changed))
    print("file lasciati relativi: %d" % len(skipped))
    for rel, info in skipped:
        print("  - %-52s %s" % (rel, info))

    if not args.apply:
        print("\n(dry-run: rilancia con --apply per scrivere)")
        return 0

    for rel, _info in changed:
        src = os.path.join(ROOT, rel)
        dst = os.path.join(backup, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
    for rel, _info in changed:
        src = os.path.join(ROOT, rel)
        with open(src, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().split("\n")
        new_lines, _ = planner(src)
        with open(src, "w", encoding="utf-8") as fh:
            fh.write("\n".join(new_lines))

    print("\nscritti %d file; backup: %s" % (len(changed), backup))
    return 0


if __name__ == "__main__":
    sys.exit(main())
