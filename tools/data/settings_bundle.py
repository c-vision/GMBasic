#!/usr/bin/env python3
"""settings_bundle.py — settings MINIMI accanto a un programma compilato.

PERCHÉ. Un `.bas` compilato si distribuisce **senza compilatore**: chi lo riceve non ha
`shared/settings/`. Il compilatore deve quindi lasciare accanto all'eseguibile i settings che
servono al RUNTIME — e **solo** quelli.

`system.settings.json` è il default GLOBALE del compilatore e dell'app: contiene `compiler`
(cc/cflags/ldflags), `importFolders`, `defaultNamespaces`, `libraryFolders`,
`defaultProjectsFolder` — roba che serve a **compilare**, non a eseguire. Distribuirlo
significherebbe confondere i due mondi (e un binario vecchio cambierebbe comportamento quando
cambiano i default del compilatore).

Qui si tengono le sole sezioni che il runtime legge davvero:

    paths                  percorsi dati (GmbiPaths / pr_load)
    consoleConfiguration   font e geometria della console (gmb_console.c, gmb_settings.c)
    engine                 opzioni del motore (gmb_opts.c / GmbEnv)

`app.settings.json` del programma (accanto al `.bas`) fa **override chiave per chiave** sulle
chiavi comuni; liste e valori scalari dell'app sostituiscono quelli globali.

Uso:
    settings_bundle.py --system <system.settings.json> --app <app.settings.json> --out <file>

Exit: 0 se ha scritto, 2 su errore d'uso, 3 se non c'è nulla da scrivere.
Stessa regola dell'implementazione C# (`CommonLib/ProgramBundle.cs`, `RuntimeSections`): se
cambia l'elenco delle sezioni, va cambiato in ENTRAMBI i punti.
"""
import argparse
import json
import os
import sys

RUNTIME_SECTIONS = ("paths", "consoleConfiguration", "engine")


def _strip_comments(text: str) -> str:
    """Rimuove // e /* */ fuori dalle stringhe (i settings sono JSONC)."""
    out = []
    i, n = 0, len(text)
    in_str = False
    while i < n:
        c = text[i]
        if in_str:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if c == '"':
                in_str = False
            i += 1
            continue
        if c == '"':
            in_str = True
            out.append(c)
            i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def load(path):
    if not path or not os.path.isfile(path):
        return None
    try:
        return json.loads(_strip_comments(open(path, encoding="utf-8").read()))
    except Exception:
        return None


def merge(system: dict | None, app: dict | None) -> dict | None:
    if not system and not app:
        return None
    result = {}
    for section in RUNTIME_SECTIONS:
        s = (system or {}).get(section)
        a = (app or {}).get(section)
        if isinstance(s, dict) and isinstance(a, dict):
            merged = dict(s)
            merged.update(a)          # l'app vince chiave per chiave
            result[section] = merged
        elif a is not None:
            result[section] = a       # lista/valore: sostituisce
        elif s is not None:
            result[section] = s
    return result


def main(argv):
    ap = argparse.ArgumentParser(description="Settings minimi per un programma compilato")
    ap.add_argument("--system", help="system.settings.json condiviso (config del compilatore)")
    ap.add_argument("--app", help="app.settings.json del programma (override)")
    ap.add_argument("--out", required=True, help="file da scrivere (accanto all'eseguibile)")
    args = ap.parse_args(argv)

    merged = merge(load(args.system), load(args.app))
    if merged is None:
        return 3
    text = json.dumps(merged, indent=2, ensure_ascii=False) + "\n"
    try:
        if os.path.isfile(args.out) and open(args.out, encoding="utf-8").read() == text:
            return 0                                   # già aggiornato
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(text)
    except Exception as e:
        print(f"settings_bundle: {e}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
