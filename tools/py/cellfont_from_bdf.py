#!/usr/bin/env python3
"""cellfont_from_bdf.py — genera il bitmap ASCII 8x16 usato dal font a CELLE del TUI.

CHE COSA FA. `gmb_gfx_cellfont.c` contiene una tabella `[95][16]` (ASCII 32..126, 8x16,
1 bit/px, MSB-left) usata dal render TUI->pixel (SDLConsole). Prima era GENERATA da
`Resources/C64_Pro_Mono-STYLE.otf`: un font che la licenza style64 vieta di ridistribuire,
quindi anche il bitmap che ne deriva non era ridistribuibile. Qui la tabella si rigenera da
**Terminus 8x16** (SIL OFL 1.1), che ha esattamente la matrice 8x16 richiesta.

USO (dalla radice del repo):
    python3 tools/py/cellfont_from_bdf.py ter-u16n.bdf \\
        GMBasic/native/gmb_runtime_c/src/gmb_gfx_cellfont_bitmap.h

Il file generato è un HEADER con la sola tabella e l'avviso di licenza OFL: il resto di
`gmb_gfx_cellfont.c` resta sotto la licenza del repository.
"""
from __future__ import annotations

import pathlib
import sys

FIRST, LAST = 32, 126
W, H = 8, 16


def parse_bdf(path: pathlib.Path) -> dict[int, list[int]]:
    glyphs: dict[int, list[int]] = {}
    enc = None
    bbx = None
    in_bitmap = False
    rows: list[int] = []
    for raw in path.read_text(errors="ignore").splitlines():
        line = raw.strip()
        if line.startswith("ENCODING"):
            enc = int(line.split()[1])
        elif line.startswith("BBX"):
            p = line.split()
            bbx = (int(p[1]), int(p[2]), int(p[3]), int(p[4]))   # w h xoff yoff
        elif line == "BITMAP":
            in_bitmap = True
            rows = []
        elif in_bitmap:
            if line == "ENDCHAR":
                in_bitmap = False
                if enc is not None and bbx is not None:
                    glyphs[enc] = place(rows, bbx)
                enc = None
                bbx = None
            elif line:
                rows.append(int(line, 16))
    return glyphs


def place(rows: list[int], bbx: tuple[int, int, int, int]) -> list[int]:
    """Porta un glifo BDF nella matrice 8x16 della cella (bit allineati a sinistra)."""
    w, h, xoff, yoff = bbx
    out = [0] * H
    # yoff = distanza della PRIMA riga dal baseline; la cella e' alta 16 con origine in alto.
    top = H - (h + (-yoff) if yoff < 0 else h) if yoff < 0 else H - h
    top = max(0, top)
    for i, bits in enumerate(rows):
        y = top + i
        if not (0 <= y < H):
            continue
        if w == W:
            out[y] = bits & 0xFF
        elif w < W:
            # allinea a sinistra dentro la cella
            out[y] = ((bits << (W - w - xoff)) if (W - w - xoff) >= 0 else (bits >> -(W - w - xoff))) & 0xFF
        else:
            out[y] = (bits >> (w - W)) & 0xFF
    return out


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, dst = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    glyphs = parse_bdf(src)
    missing = [c for c in range(FIRST, LAST + 1) if c not in glyphs]
    if missing:
        print(f"ERRORE: mancano {len(missing)} codepoint ASCII nel BDF: {missing[:8]}", file=sys.stderr)
        return 1

    lines = [
        "/* ============================================================================",
        " * gmb_gfx_cellfont_bitmap.h — tabella ASCII 8x16 del font a CELLE del TUI.",
        " *",
        " * GENERATO da tools/py/cellfont_from_bdf.py — NON modificare a mano.",
        " * Sorgente dei glifi: **Terminus** 8x16 (ter-u16n.bdf), Copyright (C) 2020",
        " * Dimitar Toshkov Zhekov, **SIL Open Font License 1.1** (testo in",
        " * Resources/licenses/OFL-1.1-Terminus.txt). Il bitmap qui sotto è opera derivata",
        " * del font e resta sotto la SIL OFL 1.1; il resto del repository è sotto PUL.",
        " *",
        " * Formato: [95][16], ASCII 32..126, 1 bit/px, MSB-left, riga 0 = alto.",
        " * ============================================================================ */",
        "#ifndef GMB_GFX_CELLFONT_BITMAP_H",
        "#define GMB_GFX_CELLFONT_BITMAP_H",
        "",
        "static const unsigned char gmb_cellfont_ascii[95][16] = {",
    ]
    for cp in range(FIRST, LAST + 1):
        g = glyphs[cp]
        body = ",".join(f"0x{b:02X}" for b in g)
        ch = chr(cp)
        label = " " if cp == 32 else ch
        lines.append(f"  {{{body}}}, /* {cp} '{label}' */")
    lines += ["};", "", "#endif /* GMB_GFX_CELLFONT_BITMAP_H */", ""]

    dst.write_text("\n".join(lines))
    print(f"scritto {dst} ({LAST - FIRST + 1} glifi da {src.name})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
