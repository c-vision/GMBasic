#!/usr/bin/env python3
"""pe_deps.py — elenca le DLL importate da un PE (EXE/DLL), senza tool esterni.

Serve a rispondere a una domanda concreta: "cosa deve stare ACCANTO a questa DLL perché
carichi su Windows?" — cioè a costruire il bundle `runtime` per win-arm64/win-x64.

Uso:
    python3 tools/c/pe_deps.py <file.dll> [--system] [--all]

Senza flag stampa solo le dipendenze NON di sistema (quelle da impacchettare).
"""
import struct, sys, os

SYSTEM = {
    "kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
    "ole32.dll", "oleaut32.dll", "ws2_32.dll", "wsock32.dll", "winmm.dll",
    "version.dll", "imm32.dll", "setupapi.dll", "dinput8.dll", "dxguid.dll",
    "crypt32.dll", "bcrypt.dll", "secur32.dll", "ncrypt.dll", "wintrust.dll",
    "avrt.dll", "oleacc.dll", "msvcrt.dll", "ucrtbase.dll", "shlwapi.dll",
    "api-ms-win-crt-runtime-l1-1-0.dll", "api-ms-win-crt-stdio-l1-1-0.dll",
    "api-ms-win-crt-heap-l1-1-0.dll", "api-ms-win-crt-string-l1-1-0.dll",
    "api-ms-win-crt-math-l1-1-0.dll", "api-ms-win-crt-convert-l1-1-0.dll",
    "api-ms-win-crt-locale-l1-1-0.dll", "api-ms-win-crt-time-l1-1-0.dll",
    "api-ms-win-crt-filesystem-l1-1-0.dll", "api-ms-win-crt-environment-l1-1-0.dll",
    "api-ms-win-crt-utility-l1-1-0.dll", "api-ms-win-core-synch-l1-2-0.dll",
    "api-ms-win-core-file-l1-1-0.dll", "api-ms-win-core-handle-l1-1-0.dll",
    "api-ms-win-core-libraryloader-l1-2-0.dll", "api-ms-win-core-processenvironment-l1-1-0.dll",
    "api-ms-win-core-errorhandling-l1-1-0.dll", "powrprof.dll", "pdh.dll",
}


def is_system(name: str) -> bool:
    """DLL di sistema o API-set virtuale (`api-ms-win-*`, `ext-ms-*`): NON vanno impacchettate.
    Gli API set non sono file: il loader li risolve tramite lo schema del sistema."""
    low = name.lower()
    return low in SYSTEM or low.startswith("api-ms-win-") or low.startswith("ext-ms-")


def imports(path):
    d = open(path, "rb").read()
    e = struct.unpack_from("<I", d, 0x3C)[0]
    if d[e:e + 4] != b"PE\0\0":
        raise ValueError("non è un PE")
    nsec = struct.unpack_from("<H", d, e + 6)[0]
    opt = e + 24
    pe32p = struct.unpack_from("<H", d, opt)[0] == 0x20B
    ddoff = opt + (112 if pe32p else 96)
    imp_rva, _ = struct.unpack_from("<II", d, ddoff + 8)   # entry 1 = import table
    secs = []
    so = opt + struct.unpack_from("<H", d, e + 20)[0]
    for i in range(nsec):
        vsz, va, rsz, raw = struct.unpack_from("<IIII", d, so + i * 40 + 8)
        secs.append((va, max(vsz, rsz), raw))

    def r2o(rva):
        for va, sz, raw in secs:
            if va <= rva < va + sz:
                return raw + (rva - va)
        return None

    out = []
    if not imp_rva:
        return out
    o = r2o(imp_rva)
    while True:
        ilt, ts, fc, name_rva, iat = struct.unpack_from("<IIIII", d, o)
        if not name_rva:
            break
        no = r2o(name_rva)
        out.append(d[no:d.index(b"\0", no)].decode(errors="replace"))
        o += 20
    return out


def check_dir(d):
    """Verifica un bundle: ogni import non di sistema deve esistere nella cartella."""
    have = {f.lower() for f in os.listdir(d)}
    missing = []
    for f in sorted(os.listdir(d)):
        if not f.lower().endswith((".dll", ".exe", ".so")):
            continue
        try:
            deps = imports(os.path.join(d, f))
        except Exception:
            continue
        for dep in deps:
            if is_system(dep):
                continue
            if dep.lower() not in have:
                missing.append((f, dep))
    return missing


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    if not args:
        print(__doc__)
        return 2
    if "--check" in flags:
        bad = 0
        for d in args:
            missing = check_dir(d)
            print(f"{d}: {len(missing)} dipendenze non soddisfatte")
            for f, dep in missing:
                print(f"    {f} → {dep}")
            bad += len(missing)
        return 1 if bad else 0

    rc = 0
    for f in args:
        try:
            deps = imports(f)
        except Exception as ex:
            print(f"{f}: ERRORE {ex}")
            rc = 1
            continue
        keep = deps if "--all" in flags else [x for x in deps if not is_system(x)]
        print(f"{os.path.basename(f)}")
        for x in keep:
            print(f"    {x}")
        if not keep:
            print("    (solo DLL di sistema)")
    return rc


if __name__ == "__main__":
    sys.exit(main())
