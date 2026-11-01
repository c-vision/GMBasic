#!/usr/bin/env python3
"""Parità fra i due motori BSP (managed e C) — resa della STESSA app, stesso stato.

I due motori eseguono la stessa pagina precompilata: il managed in una VM P-Code/IL dentro
il processo .NET, il C in una VM/C (`libgmb_bsp`, P/Invoke). Lo stato di
agenti/team/workflow/memoria è UNO SOLO (la VM/C lo legge/scrive dall'hook `gmb_state_hook.h`
registrato dal client C#): questo strumento lo verifica end-to-end.

Cosa fa: per ogni motore imposta `bspEngine` in `.webapp.toml`, riavvia l'app, scarica le
pagine, normalizza i campi dinamici (timestamp, uptime) e confronta. Ripristina la
configurazione originale alla fine (anche in caso di errore).

Uso:
    tools/sh/webapp_up.sh start
    python3 tools/py/bsp_engine_parity.py index.bsp canvas.bsp wasm.bsp

Uscita: 0 = tutte identiche, 1 = almeno una differenza (stampa il diff).
"""
import argparse
import difflib
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.request

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
APP_DIR = os.path.join(REPO, "shared", "webapp")
TOML = os.path.join(APP_DIR, ".webapp.toml")
UP = os.path.join(REPO, "tools", "sh", "webapp_up.sh")

# Campi che cambiano a ogni esecuzione indipendentemente dal motore.
NORMALIZERS = [
    (re.compile(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})?"), "<TS>"),
    (re.compile(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}"), "<TS>"),
    (re.compile(r"\b\d{2}:\d{2}:\d{2}\b"), "<TIME>"),
    (re.compile(r"\bup \d+s\b"), "up <N>s"),
    (re.compile(r'\bpid \d+\b'), "pid <N>"),
]


def normalize(text: str) -> str:
    for rx, repl in NORMALIZERS:
        text = rx.sub(repl, text)
    return text


def sh(*args: str, timeout: int = 240) -> subprocess.CompletedProcess:
    return subprocess.run(args, cwd=REPO, capture_output=True, text=True, timeout=timeout)


def set_engine(engine: str) -> None:
    src = open(TOML, encoding="utf-8").read()
    out, n = re.subn(r'(?m)^bspEngine\s*=\s*"[^"]*"', f'bspEngine = "{engine}"', src)
    if n == 0:
        raise SystemExit("bspEngine non trovato in .webapp.toml")
    open(TOML, "w", encoding="utf-8").write(out)


def restart_and_wait(url: str) -> None:
    sh(UP, "restart")
    for _ in range(60):
        try:
            with urllib.request.urlopen(url.rstrip("/") + "/api/health", timeout=2):
                return
        except Exception:
            time.sleep(0.5)
    raise SystemExit("l'app non risponde dopo il restart")


def fetch(url: str) -> str:
    with urllib.request.urlopen(url, timeout=60) as r:
        return r.read().decode("utf-8", "replace")


def main() -> int:
    ap = argparse.ArgumentParser(description="Parità resa motore BSP managed vs C")
    ap.add_argument("pages", nargs="+", help="pagine (es. index.bsp canvas.bsp)")
    ap.add_argument("--url", default="http://127.0.0.1:8090", help="base URL dell'app")
    ap.add_argument("--diff-lines", type=int, default=12)
    args = ap.parse_args()

    backup = TOML + ".parity-bak"
    shutil.copy(TOML, backup)
    failures = 0
    try:
        bodies: dict[str, dict[str, str]] = {}
        for engine in ("managed", "c"):
            set_engine(engine)
            restart_and_wait(args.url)
            bodies[engine] = {p: normalize(fetch(args.url.rstrip("/") + "/" + p)) for p in args.pages}
            print(f"reso con motore {engine}: {', '.join(args.pages)}")

        for page in args.pages:
            a, b = bodies["managed"][page], bodies["c"][page]
            if a == b:
                print(f"{page:16s} OK    ({len(b)} byte, identiche)")
                continue
            failures += 1
            print(f"{page:16s} DIFF  (managed {len(a)} byte, C {len(b)} byte)")
            diff = difflib.unified_diff(a.splitlines(), b.splitlines(),
                                        fromfile="managed", tofile="c", lineterm="", n=1)
            for i, line in enumerate(diff):
                if i >= args.diff_lines:
                    print("   …")
                    break
                print("   " + line)
    finally:
        shutil.move(backup, TOML)
        sh(UP, "restart")

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
