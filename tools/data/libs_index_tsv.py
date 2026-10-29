#!/usr/bin/env python3
"""libs_index_tsv.py — emette l'indice TSV usato dal downloader C (`<exe>.libs.txt`).

Il runtime C (`gmb_libs_fetch.c`) legge un file **TSV** (niente parser JSON):

    <nome-file>\\t<sha256>\\t<size>\\t<url>

Uso:
    libs_index_tsv.py all    <manifest.json> <rid> <out.tsv>
        tutti i file del RID che hanno un `url` (modello per-file)

    libs_index_tsv.py needed <manifest.json> <rid> <out.tsv> [nome...]
        solo i nomi indicati (es. le dipendenze reali di un eseguibile, lette con
        `otool -L` / `readelf -d`); i nomi non presenti nel manifest sono ignorati
        (sono lib di sistema o di build, non scaricabili)

Exit: 0 sempre (se il manifest manca, scrive un file vuoto) — un indice vuoto significa
"niente da scaricare", mai un errore bloccante per il build.
"""
import json
import sys


def main(argv: list[str]) -> int:
    if len(argv) < 4:
        print(__doc__, file=sys.stderr)
        return 2
    mode, manifest, rid, out = argv[0], argv[1], argv[2], argv[3]
    names = argv[4:]

    try:
        with open(manifest, encoding="utf-8") as f:
            d = json.load(f)
    except Exception as e:  # manifest assente/illeggibile → indice vuoto
        print(f"[libs] manifest non leggibile ({e}): indice vuoto", file=sys.stderr)
        open(out, "w").write("")
        return 0

    r = (d.get("rids") or {}).get(rid) or {}
    files = r.get("files") or {}

    if mode == "needed":
        wanted = [n.strip() for n in names if n.strip()]
    elif mode == "all":
        wanted = list(files.keys())
    else:
        print(f"modo sconosciuto: {mode}", file=sys.stderr)
        return 2

    lines = []
    for n in dict.fromkeys(wanted):          # dedup preservando l'ordine
        f = files.get(n)
        if not f or not f.get("url"):
            continue                          # lib di sistema o non pubblicata per-file
        lines.append("{}\t{}\t{}\t{}".format(n, f["sha256"], f["size"], f["url"]))

    # Intestazione con le VERSIONI dei gruppi (tag immutabili `runtime-v1` …). Non serve al
    # downloader (salta le righe `#`): serve a chi legge il file per sapere contro quale versione
    # è stato compilato l'eseguibile. Le URL sono già versionate, quindi il pinning è implicito.
    vers = [f"{g}-{v['version']}" for g, v in sorted((r.get("groups") or {}).items())
            if isinstance(v, dict) and v.get("version")]

    with open(out, "w", encoding="utf-8") as fh:
        if vers:
            fh.write("# requires " + " ".join(vers) + "\n")
        if lines:
            fh.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
