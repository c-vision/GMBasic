#!/usr/bin/env python3
"""Genera le tabelle C di HPACK (RFC 7541) dai dati ufficiali dell'RFC.

Perché un generatore invece di scrivere 257 codici a mano: la tabella Huffman di HPACK è
grande e un singolo bit sbagliato produce header corrotti solo su alcuni valori (bug
difficilissimo da vedere a occhio). Così la tabella si rigenera e si confronta con l'RFC.

Uso:
    python3 tools/py/gen_hpack_huffman.py            # riscrive i due header in-place
    python3 tools/py/gen_hpack_huffman.py --check    # verifica che i file siano allineati

Scarica https://www.rfc-editor.org/rfc/rfc7541.txt (cache locale in /tmp) e produce:
  - GMBasic/native/gmb_runtime_c/src/gmb_hpack_huffman.h  (codici Huffman, 257 simboli + EOS)
  - GMBasic/native/gmb_runtime_c/src/gmb_hpack_static.h   (tabella statica HPACK, 61 voci)
"""

import os
import re
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "GMBasic", "native", "gmb_runtime_c", "src")
RFC_CACHE = "/tmp/rfc7541.txt"
RFC_URL = "https://www.rfc-editor.org/rfc/rfc7541.txt"


def rfc_text() -> str:
    if os.path.exists(RFC_CACHE):
        txt = open(RFC_CACHE, encoding="utf-8", errors="replace").read()
        if "(  0)  |11111111|11000" in txt:
            return txt
    data = urllib.request.urlopen(RFC_URL, timeout=60).read().decode("utf-8", "replace")
    open(RFC_CACHE, "w", encoding="utf-8").write(data)
    return data


def parse_huffman(txt: str):
    """(simbolo, codice, lunghezza) per i 257 simboli dell'Appendice B."""
    out = {}
    for m in re.finditer(r"\(\s*(\d+)\)\s+\|([01|]+)\s+([0-9a-f]+)\s+\[\s*(\d+)\]", txt):
        sym = int(m.group(1))
        bits = m.group(2).replace("|", "")
        code = int(m.group(3), 16)
        length = int(m.group(4))
        assert len(bits) == length, (sym, bits, length)
        assert int(bits, 2) == code, (sym, bits, code)
        out[sym] = (code, length)
    return out


def parse_static(txt: str):
    """(indice, nome, valore) della tabella statica (Appendice A)."""
    start = txt.find("Appendix A.  Static Table Definition")
    end = txt.find("Appendix B.  Huffman Code", start)
    seg = txt[start:end]
    out = {}
    for line in seg.splitlines():
        m = re.match(r"\s*\|\s*(\d+)\s*\|\s*([a-z0-9\-:]+)\s*\|\s*([^|]*)\|\s*$", line)
        if m:
            out[int(m.group(1))] = (m.group(2).strip(), m.group(3).strip())
    return out


def gen_huffman(table) -> str:
    lines = [
        "/* GENERATO da tools/py/gen_hpack_huffman.py — NON modificare a mano.",
        " * Tabella dei codici Huffman di HPACK (RFC 7541, Appendice B): 256 simboli + EOS (256).",
        " * `code` è allineato ai bit ALTI (come nell'RFC); `bits` è la lunghezza in bit. */",
        "#ifndef GMB_HPACK_HUFFMAN_H",
        "#define GMB_HPACK_HUFFMAN_H",
        "",
        "typedef struct { unsigned int code; unsigned char bits; } gmb_hpack_huff_t;",
        "",
        "static const gmb_hpack_huff_t gmb_hpack_huffman[257] = {",
    ]
    for sym in range(257):
        code, bits = table[sym]
        lines.append(f"    {{ 0x{code:08x}u, {bits:2d} }},   /* {sym} */")
    lines += ["};", "", "#endif /* GMB_HPACK_HUFFMAN_H */", ""]
    return "\n".join(lines)


def gen_static(table) -> str:
    lines = [
        "/* GENERATO da tools/py/gen_hpack_huffman.py — NON modificare a mano.",
        " * Tabella STATICA di HPACK (RFC 7541, Appendice A): indice 1..61 (0 = vuoto). */",
        "#ifndef GMB_HPACK_STATIC_H",
        "#define GMB_HPACK_STATIC_H",
        "",
        "typedef struct { const char* name; const char* value; } gmb_hpack_static_t;",
        "",
        "static const gmb_hpack_static_t gmb_hpack_static[62] = {",
        '    { NULL, NULL },   /* 0: non usato */',
    ]
    for idx in sorted(table):
        name, value = table[idx]
        lines.append(f'    {{ "{name}", "{value}" }},   /* {idx} */')
    lines += ["};", "", "#endif /* GMB_HPACK_STATIC_H */", ""]
    return "\n".join(lines)


def main() -> int:
    txt = rfc_text()
    huff = parse_huffman(txt)
    static = parse_static(txt)
    if len(huff) != 257:
        print(f"ERRORE: tabella Huffman incompleta ({len(huff)}/257)", file=sys.stderr)
        return 1
    if len(static) != 61:
        print(f"ERRORE: tabella statica incompleta ({len(static)}/61)", file=sys.stderr)
        return 1

    files = {
        os.path.join(SRC, "gmb_hpack_huffman.h"): gen_huffman(huff),
        os.path.join(SRC, "gmb_hpack_static.h"): gen_static(static),
    }
    check = "--check" in sys.argv
    rc = 0
    for path, content in files.items():
        if check:
            cur = open(path, encoding="utf-8").read() if os.path.exists(path) else ""
            if cur.strip() != content.strip():
                print(f"DIVERSO: {os.path.relpath(path, ROOT)}", file=sys.stderr)
                rc = 1
            else:
                print(f"OK      {os.path.relpath(path, ROOT)}")
        else:
            open(path, "w", encoding="utf-8").write(content)
            print(f"scritto {os.path.relpath(path, ROOT)}")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
