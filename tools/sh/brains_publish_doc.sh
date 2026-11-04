#!/usr/bin/env bash
# brains_publish_doc.sh — pubblica in `c-vision/GMBasicBrains` il DOCUMENTO dei cervelli.
#
# PERCHÉ ESISTE. I cervelli sono **asset di release** (non file nel tree: `trading` è ~600 MB
# zippato, oltre il limite GitHub di 100 MB per file). Chi apre il repo, però, deve poter capire
# COSA c'è senza scaricare 1,2 GB: questo script genera `BRAINS.md` (tabella) e `README.md`
# dalla fonte unica dei metadati, `shared/settings/brains-manifest.json`, e li committa nel repo.
#
# La tabella è GENERATA: non si modifica a mano. `sha256` e `size` sono quelli dell'asset
# pubblicato (gli stessi che il motore `gmbfetch` verifica prima di estrarre).
#
# USO
#   tools/sh/brains_publish_doc.sh            genera e PUBBLICA (commit + push su main)
#   tools/sh/brains_publish_doc.sh --check    genera in una cartella temporanea e mostra il diff,
#                                             senza toccare il repo
#
# Richiede: git, gh autenticato con accesso a c-vision/GMBasicBrains (repo privato).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MONOREPO="$(cd "$SCRIPT_DIR/../.." && pwd)"
MANIFEST="$MONOREPO/shared/settings/brains-manifest.json"
REPO="c-vision/GMBasicBrains"

CHECK=0
for a in "$@"; do
  case "$a" in
    --check) CHECK=1 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "opzione sconosciuta: $a" >&2; exit 2 ;;
  esac
done

[ -f "$MANIFEST" ] || { echo "manifest non trovato: $MANIFEST" >&2; exit 1; }
command -v jq >/dev/null || { echo "jq richiesto" >&2; exit 1; }

TMP="$(mktemp -d "${TMPDIR:-/tmp}/gmb-brains-doc.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

# ── BRAINS.md (tabella generata dal manifest) ────────────────────────────────────────────────
{
  echo "# Cervelli disponibili"
  echo
  echo "> **Generato** da \`shared/settings/brains-manifest.json\` del monorepo GMBasic — **non"
  echo "> modificare a mano**: rigenera con \`tools/sh/brains_publish_doc.sh\`."
  echo
  echo "I cervelli sono **asset di release** di questo repo (non file nel tree: un cervello arriva a"
  echo "1,4 GB). Si installano con il package manager del compilatore:"
  echo
  echo '```sh'
  echo "gmb brain login --github          # una volta: token con accesso a questo repo privato"
  echo "gmb brain install <nome>          # download on demand (resume + sha256), cache ~/.gmbasic/brains"
  echo "gmb brain list                    # cosa è installato, con le forme [rag] / [graph]"
  echo '```'
  echo
  echo "| cervello | versione | forme | release | asset | dimensione | sha256 |"
  echo "|---|---|---|---|---|---|---|"
  jq -r '
    .brains[] as $b
    | $b.versions[]
    | "| `\($b.name)` | \(.version)\(if $b.latest == .version then " **(latest)**" else "" end)"
      + " | \(.modes | join("+")) | `\(.tag)` | `\(.asset)`"
      + " | \(.size) B | `\(.sha256)` |"
  ' "$MANIFEST"
  echo
  echo "**Forme**: \`rag\` = database vettoriale (\`rag/<nome>.db\`, ricerca semantica);"
  echo "\`graph\` = export JSON del knowledge graph (\`graphs/<nome>.json\`, query/path/community)."
  echo "Un cervello può avere una o entrambe: il pacchetto contiene tutte le forme dichiarate."
  echo
  echo "**Dove finisce dopo l'installazione** (layout canonico):"
  echo
  echo '```'
  echo "~/.gmbasic/brains/<nome>/<versione>/"
  echo "├── brain.manifest.json      # files + sha256 di ogni forma"
  echo "├── rag/<nome>.db            # se modes contiene \"rag\""
  echo "└── graphs/<nome>.json       # se modes contiene \"graph\""
  echo '```'
  echo
  echo "I \`.gmbrain.zip\` di ogni release sono immutabili; ogni asset ha lo \`sha256\` della tabella,"
  echo "verificato dal motore di download **prima** dell'estrazione."
} > "$TMP/BRAINS.md"

# ── README.md (puntatore + regole d'accesso) ─────────────────────────────────────────────────
{
  echo "# GMBasicBrains"
  echo
  echo "Cervelli (**Second Brain**) di GMBasic, distribuiti come **asset di release** e scaricati"
  echo "**on demand** dal package manager del compilatore (stesso modello di \`torch\`: i dati non"
  echo "vivono in git)."
  echo
  echo "- **Elenco completo** (nome, versione, forme \`rag\`/\`graph\`, sha256, dimensione): [BRAINS.md](BRAINS.md)"
  echo "- **Installazione**: \`gmb brain login --github\` → \`gmb brain install <nome>\`"
  echo "  (oppure, da BASIC, \`AI BRAIN INSTALL \"<nome>\"\`)."
  echo "- **Accesso**: questo repo è **privato**; senza un token con accesso, l'installazione è"
  echo "  **rifiutata** (fail-closed) — nessun URL anonimo."
  echo "- **Cache locale**: \`~/.gmbasic/brains/<nome>/<versione>/{rag,graphs}/\`."
  echo
  echo "Nel **tree** di questo repo ci sono solo i metadati (questo file e \`BRAINS.md\`): i"
  echo "pacchetti \`.gmbrain.zip\` stanno nelle release (\`brains-v1\`, \`brains-v2\`, …), dove ogni"
  echo "asset è immutabile e verificabile."
} > "$TMP/README.md"

if [ "$CHECK" = "1" ]; then
  mkdir -p "$TMP/repo"
  git clone --quiet --depth 1 "git@github.com:$REPO.git" "$TMP/repo" 2>/dev/null \
    || git clone --quiet --depth 1 "https://github.com/$REPO.git" "$TMP/repo"
  echo "=== diff (repo → generato) ==="
  diff -u "$TMP/repo/BRAINS.md" "$TMP/BRAINS.md" 2>/dev/null || true
  diff -u "$TMP/repo/README.md" "$TMP/README.md" 2>/dev/null || true
  echo "=== (nessun push: --check) ==="
  exit 0
fi

# ── pubblica: clone, scrivi, commit, push (repo piccolo: solo metadati) ──────────────────────
git clone --quiet --depth 1 "https://github.com/$REPO.git" "$TMP/repo"
cp "$TMP/BRAINS.md" "$TMP/README.md" "$TMP/repo/"
(
  cd "$TMP/repo"
  git add BRAINS.md README.md
  if git diff --cached --quiet; then
    echo "brains: metadati già aggiornati (nessun commit)"
    exit 0
  fi
  git -c user.name="gmbasic-brains-bot" -c user.email="gmbasic@maxxcom.it" \
      commit -q -m "docs: indice dei cervelli (generato da shared/settings/brains-manifest.json)"
  git push --quiet origin HEAD
  echo "brains: BRAINS.md e README.md pubblicati in $REPO"
)
