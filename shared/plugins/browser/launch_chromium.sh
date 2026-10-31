#!/usr/bin/env bash
# launch_chromium.sh — avvia Chromium headless con CDP (porta 9222 default).
# B1 del piano plugin browser (sezione 10). Usa la cache gestita da `gmb setup BROWSER`
# (~/.gmbasic/libs/chromium/<rid>/; legacy ~/.gmbasic/chromium/<rid>/) e
# ripiega sul Chromium di sistema. Uso:
#   plugins/browser/launch_chromium.sh [port] [profile]
set -euo pipefail
PORT="${1:-9222}"
NAMED_PROFILE="${2:-${GMBASIC_CHROMIUM_PROFILE:-}}"

# RID come in ChromiumCache.ResolveRid (osx-arm64, osx-x64, linux-x64, linux-arm64, win-x64).
os="$(uname -s)"
arch="$(uname -m)"
case "$os" in
  Darwin)  os_rid="osx";;
  Linux)   os_rid="linux";;
  *)       os_rid="$os";;
esac
case "$arch" in
  arm64|aarch64) arch_rid="arm64";;
  x86_64|amd64)  arch_rid="x64";;
  *)             arch_rid="$arch";;
esac
[ "$os_rid" = "win" ] && arch_rid="x64"  # il manifest win ha solo x64
RID="$os_rid-$arch_rid"

# Root del repo (risale finché trova GMBASIC.slnx) → cache canonica ~/.gmbasic/libs/.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT=""
dir="$SCRIPT_DIR"
while [ -n "$dir" ] && [ "$dir" != "/" ]; do
  if [ -f "$dir/GMBasic.sln" ] || [ -f "$dir/GMBASIC.sln" ]; then REPO_ROOT="$dir"; break; fi
  dir="$(dirname "$dir")"
done

# 1) Cache gestita (gmb setup BROWSER) — binario estratto (ricorsivo), percorsi multipli.
# ORDINE: prima la cache CANONICA del downloader (`gmb setup BROWSER` → FetchChromiumLib in
# ~/.gmb/libs/chromium/<rid>), poi la copia nel checkout, poi il percorso legacy. Prima l'ordine
# era invertito e la cache canonica NON era nemmeno elencata: il browser scaricato dal comando
# di setup non veniva mai trovato dallo script (che cercava solo checkout e ~/.gmbasic).
# ORDINE: cache canonica (~/.gmbasic/libs/chromium/<rid>) → vecchia radice → checkout (legacy).
CACHE_DIRS=("$HOME/.gmbasic/libs/chromium/$RID")
CACHE_DIRS+=("$HOME/.gmbasic/chromium/$RID")
CHROME=""
for CACHE in "${CACHE_DIRS[@]}"; do
  if [ -d "$CACHE" ]; then
    CHROME="$(find "$CACHE" -type f \( -name "Google Chrome for Testing" -o -name chrome -o -name chrome.exe \) -perm -u+x 2>/dev/null | head -1)"
    [ -n "$CHROME" ] && break
  fi
done
CACHE="${CACHE_DIRS[0]:-$HOME/.gmbasic/chromium/$RID}"

# 2) Chromium di sistema.
if [ -z "${CHROME:-}" ]; then
  CHROME="$(command -v google-chrome || command -v chromium || command -v chromium-browser || command -v chrome || true)"
fi

if [ -z "${CHROME:-}" ]; then
  echo "Chromium non trovato. Scaricalo con: gmb browser setup" >&2
  echo "  (cache: $CACHE; in alternativa: npx playwright install chromium)" >&2
  exit 1
fi

# E11: WARM START — se il CDP sulla porta è GIÀ attivo, riusa il processo in esecuzione
# (nessun nuovo avvio): le run successive condividono lo stesso Chromium + profilo.
# Chrome moderne ascolta su [::1]; Obscura e versioni precedenti su 127.0.0.1.
if curl -s --max-time 1 "http://127.0.0.1:$PORT/json/version" >/dev/null 2>&1 || \
   curl -s --max-time 1 "http://[::1]:$PORT/json/version" >/dev/null 2>&1; then
  echo "reusing chromium on :$PORT (warm start)" >&2
  exit 0
fi

# E2: profilo STABILE (~/.gmbasic/libs/chromium/profile) → cookie/
# localStorage persistono tra le run (login una volta, sessione riusata). Con
# GMBASIC_CHROMIUM_EPHEMERAL=1 torna un profilo temporaneo (test isolati).
# Un argomento [profile] nominativo (isolamento sessione/per-utente, feature #3 Camoufox
# via CDP) usa ~/.gmbasic/libs/chromium/profiles/<nome> → ogni utente ha un profilo
# separato (cookie/storage/fingerprint), non condiviso.
if [ -n "$NAMED_PROFILE" ]; then
  # profilo nominativo: sempre nella cache utente (scrivibile, una sola posizione)
  USER_DATA_DIR="$HOME/.gmbasic/libs/chromium/profiles/$NAMED_PROFILE"
  mkdir -p "$USER_DATA_DIR"
elif [ "${GMBASIC_CHROMIUM_EPHEMERAL:-0}" = "1" ]; then
  USER_DATA_DIR="$(mktemp -d /tmp/gmb-chromium.XXXXXX)"
else
  # profilo STABILE: nella cache utente, accanto a Chromium
  USER_DATA_DIR="$HOME/.gmbasic/libs/chromium/profile"
  mkdir -p "$USER_DATA_DIR"
fi

# E15: SANDBOX — profilo ristretto di default. --no-sandbox SOLO se richiesto esplicitamente
# (CI/container senza namespace, es. Linux root): su macOS/desktop il sandbox Chromium resta
# attivo. --disable-extensions riduce la superficie di attacco (niente estensioni).
SANDBOX_FLAG=""
if [ "${GMBASIC_CHROMIUM_NO_SANDBOX:-0}" = "1" ]; then
  SANDBOX_FLAG="--no-sandbox"
fi

exec "$CHROME" --headless=new --disable-gpu $SANDBOX_FLAG \
  --disable-extensions --disable-component-update --disable-notifications \
  --disable-blink-features=AutomationControlled \
  --remote-debugging-port="$PORT" \
  --user-data-dir="$USER_DATA_DIR" \
  ${GMBASIC_CHROMIUM_PROXY:+--proxy-server="$GMBASIC_CHROMIUM_PROXY"} \
  ${GMBASIC_CHROMIUM_USER_AGENT:+--user-agent="$GMBASIC_CHROMIUM_USER_AGENT"} \
  about:blank
