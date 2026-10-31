#!/usr/bin/env python3
"""Parità fra i due motori BSP (managed e C) per una pagina.

Il motore managed (VM P-Code/IL nel processo C#) e il motore C (`libgmb_bsp`, VM/C via
P/Invoke) eseguono la STESSA pagina precompilata (sidecar `<pagina>.bsp.chunk`). Il motore C è
più veloce ma NON vede lo stato del runtime managed (agenti, team, workflow, memoria AI): una
pagina che li interroga rende diverso. Questo strumento dice, pagina per pagina, se il
passaggio al motore C è sicuro.

Uso:
    # app attiva con la pagina su "managed"
    tools/sh/webapp_up.sh start
    python3 tools/py/bsp_engine_parity.py --url http://127.0.0.1:8090 canvas.bsp wasm.bsp

Uscita: 0 = tutte identiche (a meno di campi dinamici normalizzati), 1 = almeno una differenza.

Prima del confronto i sidecar vanno generati (li rigenera lo stesso motore C su richiesta, ma
per un confronto "pulito" conviene la precompilazione esplicita):

    GMBASIC_ROOT="$PWD/GMBasic" dotnet GMBasicSharp/REPL/bin/Debug/net10.0/REPL.dll \
        -noconsole -bsp shared/webapp/pages
"""
import argparse
import ctypes
import difflib
import os
import re
import subprocess
import sys
import urllib.request

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LIB_DEFAULT = os.path.join(REPO, "GMBasic", "native", "gmb_vm_c", "build")


def find_lib(explicit: str | None) -> str:
    if explicit:
        return explicit
    names = ["libgmb_bsp.dylib", "libgmb_bsp.so", "gmb_bsp.dll"]
    for name in names:
        for base in (LIB_DEFAULT, os.path.dirname(os.path.abspath(__file__))):
            p = os.path.join(base, name)
            if os.path.exists(p):
                return p
    raise SystemExit(
        "libgmb_bsp non trovata: compilala con\n"
        "  cmake --build GMBasic/native/gmb_vm_c/build --target gmb_bsp"
    )


def repl_dll() -> str | None:
    """Percorso del REPL (precompilazione `-bsp` dei sidecar)."""
    for cfg in ("Debug", "Release"):
        p = os.path.join(REPO, "GMBasicSharp", "REPL", "bin", cfg, "net10.0", "REPL.dll")
        if os.path.exists(p):
            return p
    return None


def precompile(pages_dir: str) -> bool:
    """Genera i sidecar `<pagina>.bsp.chunk` (li esegue il motore C)."""
    dll = repl_dll()
    if dll is None:
        print("REPL non compilato: esegui `dotnet build GMBasicSharp/REPL/REPL.csproj`", file=sys.stderr)
        return False
    r = subprocess.run(["dotnet", dll, "-noconsole", "-bsp", pages_dir],
                       capture_output=True, text=True, timeout=600)
    for line in (r.stdout or "").splitlines():
        if "Pre-compilation:" in line:
            print(line)
    return r.returncode == 0


def load_engine(path: str):
    lib = ctypes.CDLL(path)
    lib.gmb_bsp_shared_abi.restype = ctypes.c_int
    lib.gmb_bsp_shared_init.restype = None
    lib.gmb_bsp_shared_serve.restype = ctypes.c_int
    lib.gmb_bsp_shared_serve.argtypes = [
        ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(ctypes.c_int),
    ]
    lib.gmb_bsp_shared_free.argtypes = [ctypes.c_char_p]
    abi = lib.gmb_bsp_shared_abi()
    if abi != 1:
        raise SystemExit(f"ABI del motore C = {abi}, attesa 1")
    lib.gmb_bsp_shared_init()
    return lib


def render_c(lib, page_path: str, host: str | None, query: str | None = None):
    directory = os.path.dirname(os.path.abspath(page_path))
    url_path = "/" + os.path.basename(page_path)
    out = ctypes.c_char_p()
    status = ctypes.c_int()
    rc = lib.gmb_bsp_shared_serve(
        directory.encode(), url_path.encode(),
        query.encode() if query else None,
        host.encode() if host else None,
        ctypes.byref(out), ctypes.byref(status),
    )
    if rc != 1:
        raise RuntimeError(f"motore C: rc={rc} (sidecar assente o pagina non gestita)")
    body = (out.value or b"").decode("utf-8", "replace")
    lib.gmb_bsp_shared_free(out)
    return status.value, body


# Campi che cambiano a ogni esecuzione indipendentemente dal motore: normalizzati prima del
# confronto, altrimenti ogni pagina con data/ora/uptime risulterebbe "diversa".
NORMALIZERS = [
    (re.compile(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?Z?"), "<TS>"),   # ISO 8601
    (re.compile(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}"), "<TS>"),             # SQL/console
    (re.compile(r"\b\d{2}:\d{2}:\d{2}\b"), "<TIME>"),                          # ora nuda
    (re.compile(r"\bup \d+s\b"), "up <N>s"),                                   # uptime dello status
    (re.compile(r'"(uptime|elapsed|duration|ms|rps)"\s*:\s*[0-9.]+'), r'"\1":<N>'),
]


def normalize(text: str) -> str:
    for rx, repl in NORMALIZERS:
        text = rx.sub(repl, text)
    return text


def fetch(url: str) -> str:
    with urllib.request.urlopen(url, timeout=30) as r:
        return r.read().decode("utf-8", "replace")


def main() -> int:
    ap = argparse.ArgumentParser(description="Parità output motore BSP managed vs C")
    ap.add_argument("pages", nargs="+", help="nomi delle pagine (es. canvas.bsp wasm.bsp)")
    ap.add_argument("--url", default="http://127.0.0.1:8090", help="base URL dell'app (managed)")
    ap.add_argument("--pages-dir", default=os.path.join(REPO, "shared", "webapp", "pages"))
    ap.add_argument("--lib", default=None, help="percorso di libgmb_bsp")
    ap.add_argument("--diff-lines", type=int, default=12)
    args = ap.parse_args()

    lib = load_engine(find_lib(args.lib))
    host = args.url.split("://", 1)[-1].rstrip("/")
    failures = 0

    # Senza sidecar il motore C non ha nulla da eseguire: li si genera qui (una volta sola).
    missing = [p for p in args.pages
               if not os.path.exists(os.path.join(args.pages_dir, p) + ".chunk")]
    if missing:
        print(f"sidecar mancanti per {', '.join(missing)} → precompilazione")
        precompile(args.pages_dir)

    for page in args.pages:
        page_path = os.path.join(args.pages_dir, page)
        if not os.path.exists(page_path):
            print(f"{page:14s} MANCA {page_path}")
            failures += 1
            continue
        try:
            managed = normalize(fetch(args.url.rstrip("/") + "/" + page))
        except Exception as ex:
            print(f"{page:14s} ERRORE fetch managed: {ex}")
            failures += 1
            continue
        try:
            status, cbody = render_c(lib, page_path, host)
        except Exception as ex:
            print(f"{page:14s} ERRORE motore C: {ex}")
            failures += 1
            continue
        if status != 200:
            print(f"{page:14s} ERRORE motore C: status {status}: {cbody[:80]!r}")
            failures += 1
            continue
        c_norm = normalize(cbody)
        if status != 200 or managed == c_norm:
            print(f"{page:14s} OK   ({status}, {len(cbody)} byte, identiche)")
            continue
        failures += 1
        print(f"{page:14s} DIFF (managed {len(managed)} byte, C {len(cbody)} byte)"
              f" — la pagina legge stato del runtime managed?")
        diff = difflib.unified_diff(
            managed.splitlines(), c_norm.splitlines(),
            fromfile="managed", tofile="c", lineterm="", n=1)
        for i, line in enumerate(diff):
            if i >= args.diff_lines:
                print("   …")
                break
            print("   " + line)

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
