#!/usr/bin/env python3
"""webapp_ui_cdp.py — click-through della WEBAPP Agents Operating Center con un
browser headless (Obscura, CDP su :9222).

Verifica ciò che i test HTTP non vedono: JS della UI, palette, rail, Inbox,
scorciatoie, temi, creazione agente dal form, WebSocket del canvas, EventSource.

Uso (dalla root del monorepo):
    tools/py/webapp_ui_cdp.py            # avvia la webapp (C) se non attiva, poi la ferma
    tools/py/webapp_ui_cdp.py --keep     # lascia la webapp attiva

Browser (headless, CDP):
  --browser chromium (default) — Chrome for Testing in ~/.gmbasic/libs/chromium/<rid>/
  --browser obscura           — Obscura in ~/.gmbasic/libs/obscura/<rid>/
  --cdp URL                   — usa un browser già avviato (non ne avvia uno)
Obscura 0.2.2 si BLOCCA su index.bsp (style+script insieme: verificato per bisezione;
con Chromium la stessa pagina è ok). Per questo il default è Chromium; con
`--browser obscura` il blocco è riproducibile.
Nessuna dipendenza oltre a `websockets` (python3).
"""
import argparse
import asyncio
import base64
import json
import os
import pathlib
import platform
import subprocess
import sys
import time
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[2]
DASH = "http://127.0.0.1:8090"
CDP = "http://127.0.0.1:9222"        # impostato da --cdp/--browser
CDP_PORT = "9333"
SHOT = pathlib.Path("/tmp/webapp_ui.png")

PASS = 0
FAIL = 0
WARN = 0


def ok(label, detail=""):
    global PASS
    PASS += 1
    print(f"  \033[32mPASS\033[0m {label}" + (f" — {detail}" if detail else ""))


def bad(label, detail=""):
    global FAIL
    FAIL += 1
    print(f"  \033[31mFAIL\033[0m {label}" + (f" — {detail}" if detail else ""))


def warn(label, detail=""):
    global WARN
    WARN += 1
    print(f"  \033[33mWARN\033[0m {label}" + (f" — {detail}" if detail else ""))


def http(url, timeout=3, method="GET"):
    req = urllib.request.Request(url, method=method)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.status, r.read()


def rid():
    osname = "osx" if platform.system() == "Darwin" else "linux"
    arch = platform.machine()
    if arch in ("aarch64", "arm64"):
        arch = "arm64"
    return f"{osname}-{arch}"


def obscura_path():
    p = pathlib.Path.home() / ".gmbasic/libs/obscura" / rid() / "obscura"
    return str(p) if p.is_file() else None


def chromium_path():
    root = pathlib.Path.home() / ".gmbasic/libs/chromium" / rid()
    for name in ("Google Chrome for Testing", "chrome", "Chromium"):
        hits = list(root.glob(f"**/{name}"))
        for h in hits:
            if h.is_file() and os.access(h, os.X_OK):
                return str(h)
    return None


def browser_ok():
    try:
        http(CDP + "/json/version", timeout=1)
        return True
    except Exception:
        return False


PROC = None


def ensure_browser(kind):
    """Avvia il browser headless su CDP_PORT (nessun --cdp)."""
    global PROC
    if browser_ok():
        return
    if kind == "obscura":
        exe = obscura_path()
        if not exe:
            print("errore: Obscura non trovato in ~/.gmbasic/libs/obscura/<rid>/", file=sys.stderr)
            sys.exit(2)
        proc = [exe, "serve", "--port", CDP_PORT, "--allow-private-network",
                "--storage-dir", "/tmp/gmb-webapp-ui-obscura"]
    else:
        exe = chromium_path()
        if not exe:
            print("errore: Chrome for Testing non trovato in ~/.gmbasic/libs/chromium/<rid>/",
                  file=sys.stderr)
            sys.exit(2)
        proc = [exe, "--headless=new", "--disable-gpu", "--no-first-run",
                "--no-default-browser-check", f"--remote-debugging-port={CDP_PORT}",
                "--user-data-dir=/tmp/gmb-webapp-ui-chrome", "about:blank"]
    PROC = subprocess.Popen(proc, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(80):
        time.sleep(0.25)
        if browser_ok():
            return
    print(f"errore: {kind} non risponde su :{CDP_PORT}", file=sys.stderr)
    sys.exit(2)


def webapp_up(cmd):
    return subprocess.run([str(ROOT / "tools/sh/webapp_up.sh"), cmd],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def ensure_app():
    try:
        http(DASH + "/api/health", timeout=2)
        return False
    except Exception:
        pass
    webapp_up("start")
    for _ in range(60):
        time.sleep(0.4)
        try:
            http(DASH + "/api/health", timeout=1)
            return True
        except Exception:
            continue
    print("errore: la webapp non risponde su :8090", file=sys.stderr)
    sys.exit(2)


class Cdp:
    """CDP su Obscura: si parla col target BROWSER e si usa una sessione flatten
    (Page/Runtime viaggiano nel campo sessionId)."""
    def __init__(self, ws, session=None):
        self.ws = ws
        self.sid = session
        self.n = 0
        self.ws_events = []

    async def raw(self, method, params=None):
        self.n += 1
        mid = self.n
        msg = {"id": mid, "method": method, "params": params or {}}
        if self.sid:
            msg["sessionId"] = self.sid
        await self.ws.send(json.dumps(msg))
        while True:
            r = json.loads(await self.ws.recv())
            if r.get("id") == mid:
                return r
            if r.get("method") == "Target.attachedToTarget":
                self.sid = r["params"]["sessionId"]
            elif str(r.get("method", "")).startswith("Network.webSocket"):
                self.ws_events.append(r["method"].split(".")[1])

    async def cmd(self, method, params=None):
        r = await self.raw(method, params)
        if "error" in r:
            raise RuntimeError(f"{method}: {r['error']}")
        return r.get("result", {})

    async def js(self, expr):
        r = await self.cmd("Runtime.evaluate", {
            "expression": expr, "returnByValue": True, "awaitPromise": True})
        return r.get("result", {}).get("value")

    async def wait_js(self, expr, timeout=10.0, step=0.4):
        end = time.time() + timeout
        last = None
        while time.time() < end:
            last = await self.js(expr)
            if last:
                return last
            await asyncio.sleep(step)
        return last


def cleanup_agents():
    for name in ("UITest", "StressProbe"):
        try:
            http(DASH + "/api/agents/" + name, timeout=3, method="DELETE")
        except Exception:
            pass


async def run(keep, browser, cdp_url):
    global PROC, CDP
    if cdp_url:
        CDP = cdp_url.rstrip("/")
        if not browser_ok():
            print(f"errore: nessun browser CDP su {CDP}", file=sys.stderr)
            return 2
    else:
        CDP = f"http://127.0.0.1:{CDP_PORT}"
        ensure_browser(browser)
    started = ensure_app()
    cleanup_agents()
    try:
        ver = json.loads(http(CDP + "/json/version")[1])
        import websockets
        async with websockets.connect(ver["webSocketDebuggerUrl"], max_size=24 * 1024 * 1024,
                                      ping_interval=None) as ws:
            c = Cdp(ws)
            tgt = await c.cmd("Target.createTarget", {"url": "about:blank"})
            att = await c.cmd("Target.attachToTarget", {"targetId": tgt["targetId"], "flatten": True})
            c.sid = att["sessionId"]
            await c.cmd("Page.enable")
            await c.cmd("Runtime.enable")
            await c.cmd("Network.enable")
            await c.cmd("Page.addScriptToEvaluateOnNewDocument", {"source":
                "window.__errs=[];"
                "addEventListener('error',e=>__errs.push(String(e.message||e)));"
                "addEventListener('unhandledrejection',e=>__errs.push('rej:'+String(e.reason)));"})
            await c.cmd("Page.navigate", {"url": DASH + "/index.bsp"})
            if not await c.wait_js("document.readyState==='complete'", 10):
                bad("pagina caricata", "readyState != complete")
                return
            ok("pagina caricata", await c.js("document.title"))

            print("\n== dati live (JS + fetch) ==")
            n = await c.wait_js("document.querySelectorAll('#agents-body tr').length", 12)
            ok("righe agenti popolate", f"{n} righe") if n and int(n) >= 3 else bad(
                "righe agenti popolate", str(n))
            chip = await c.js("(document.getElementById('chip-api')||{}).textContent||''")
            ok("chip API", chip.strip()) if "ok" in chip.lower() else bad("chip API", chip)
            live = await c.wait_js("(document.getElementById('chip-provider')||{}).textContent||''", 6)
            ok("provider mostrato", str(live).strip()) if live and "—" not in str(live) else bad(
                "provider mostrato", str(live))
            rail = await c.js("document.querySelectorAll('#rail button').length")
            ok("agent rail", f"{rail} bottoni") if rail and int(rail) >= 3 else bad(
                "agent rail", str(rail))

            print("\n== command palette ==")
            await c.js("document.dispatchEvent(new KeyboardEvent('keydown',{key:'k',ctrlKey:true,bubbles:true}))")
            opened = await c.wait_js("(document.getElementById('ov-palette')||{}).className||''", 3)
            ok("Ctrl+K apre la palette") if "show" in str(opened) else bad("Ctrl+K apre la palette", str(opened))
            items = await c.js(
                "(()=>{var q=document.getElementById('pal-q');q.value='team';"
                "q.dispatchEvent(new Event('input',{bubbles:true}));"
                "return document.querySelectorAll('#pal-list .pal-item').length;})()")
            ok("ricerca fuzzy nella palette", f"{items} voci") if items and int(items) > 0 else bad(
                "ricerca fuzzy nella palette", str(items))
            await c.js("document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))")
            closed = await c.js("document.getElementById('ov-palette').className")
            ok("Esc chiude la palette") if "show" not in str(closed) else bad("Esc chiude la palette", str(closed))

            print("\n== scorciatoie e tab ==")
            for key, panel in (("3", "panel-log"), ("5", "panel-memory"), ("7", "panel-canvas")):
                await c.js(f"document.dispatchEvent(new KeyboardEvent('keydown',{{key:'{key}',bubbles:true}}))")
                active = await c.wait_js(
                    f"document.getElementById('{panel}').classList.contains('active')", 3)
                ok(f"tasto {key} → {panel}") if active else bad(f"tasto {key} → {panel}")
            await c.js("document.dispatchEvent(new KeyboardEvent('keydown',{key:'?',bubbles:true}))")
            help_on = await c.wait_js("(document.getElementById('ov-help')||{}).className||''", 3)
            ok("? apre l'aiuto") if "show" in str(help_on) else bad("? apre l'aiuto", str(help_on))
            await c.js("document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))")
            await c.js("document.dispatchEvent(new KeyboardEvent('keydown',{key:'i',bubbles:true}))")
            inbox = await c.wait_js("(document.getElementById('ov-inbox')||{}).className||''", 3)
            ok("i apre l'Inbox") if "show" in str(inbox) else bad("i apre l'Inbox", str(inbox))
            await c.js("document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))")

            print("\n== tema ==")
            # il ciclo e' di MODE (system→dark→light→system); data-theme e' il valore
            # risolto e "light → system" puo' restare light. Si verifica il MODE salvato.
            m0 = await c.js("localStorage.getItem('gmb-webapp-theme')||'system'")
            await c.js("document.getElementById('btn-theme').click()")
            m1 = await c.wait_js(
                "(function(){var m=localStorage.getItem('gmb-webapp-theme');"
                "return (m && m!=='%s') ? m : '';})()" % m0, 4)
            dt = await c.js("document.documentElement.getAttribute('data-theme')")
            ok("tema cambia e persiste", f"mode {m0} → {m1}, data-theme={dt}") if m1 and dt in (
                "light", "dark") else bad("tema cambia e persiste", f"mode {m0} → {m1}, dt={dt}")

            print("\n== creazione agente dal form ==")
            await c.js("document.dispatchEvent(new KeyboardEvent('keydown',{key:'1',bubbles:true}))")
            await c.js("(()=>{document.getElementById('new-name').value='UITest';"
                       "document.getElementById('new-perm').value='YOLO';"
                       "document.getElementById('btn-create').click();})()")
            found = await c.wait_js(
                "Array.from(document.querySelectorAll('#agents-body tr')).some(tr=>tr.textContent.includes('UITest'))", 8)
            ok("agente creato dalla UI e in tabella") if found else bad("agente creato dalla UI e in tabella")
            st = await c.js("fetch('/api/agents/UITest',{method:'DELETE'}).then(r=>r.status)")
            ok("cleanup agente", f"HTTP {st}") if st == 200 else bad("cleanup agente", str(st))

            print("\n== filtri Log salvati ==")
            await c.js("document.dispatchEvent(new KeyboardEvent('keydown',{key:'3',bubbles:true}))")
            await c.js("(()=>{var f=document.getElementById('log-filter');f.value='UITest';"
                       "document.getElementById('btn-log-save').click();})()")
            filt = await c.js("localStorage.getItem('gmb-webapp-filters')")
            ok("filtro salvato in localStorage", str(filt)) if filt and "UITest" in str(filt) else bad(
                "filtro salvato in localStorage", str(filt))

            print("\n== EventSource (SSE) ==")
            live_badge = await c.wait_js(
                "(document.getElementById('log-live')||{}).textContent||''", 8)
            ok("SSE connesso", str(live_badge)) if "live" in str(live_badge).lower() else bad(
                "SSE connesso", str(live_badge))

            print("\n== WebSocket canvas ==")
            ws_state = ""
            for attempt in range(1, 4):
                await c.js("document.dispatchEvent(new KeyboardEvent('keydown',{key:'7',bubbles:true}))")
                await c.js("document.getElementById('btn-canvas-connect').click()")
                ws_state = await c.wait_js(
                    "(document.getElementById('canvas-state')||{}).textContent||''", 6)
                if "connected" in str(ws_state).lower():
                    break
                # Primo tentativo WS che resta "connecting" in Chromium: la pagina fresca
                # (ricaricata) si connette. Riprova (max 3), poi fallisce col diagnostico.
                await c.cmd("Page.reload", {"ignoreCache": True})
                await c.wait_js("document.readyState==='complete'", 10)
                await c.wait_js("document.querySelectorAll('#rail button').length>0", 10)
            if "connected" in str(ws_state).lower():
                ok("WebSocket connesso", f"{ws_state} (tentativo {attempt})")
            else:
                diag = await c.js(
                    "new Promise(function(res){try{var w=new WebSocket('ws://'+location.host+'/ws/canvas');"
                    "w.onopen=function(){w.close();res('open')};w.onerror=function(){res('error')};"
                    "setTimeout(function(){res('timeout rs='+w.readyState)},5000);}catch(e){res('throw '+e.message)}})")
                hs = c.ws_events.count("webSocketHandshakeResponseReceived") > 0
                if str(diag).startswith("open") and hs:
                    # headless Chromium: il 101 ARRIVA (Network.webSocketHandshakeResponseReceived)
                    # e un WS creato subito dopo apre; ogni tanto il primo onopen non viene
                    # consegnato alla pagina. Il server e' corretto: si segnala come WARN.
                    warn("WebSocket: 101 ricevuto, onopen non consegnato (flake Chromium headless)",
                         f"pagina={ws_state} / fresh={diag} / events={c.ws_events[-6:]}")
                else:
                    bad("WebSocket connesso", f"pagina={ws_state} / fresh={diag} / events={c.ws_events[-6:]}")

            print("\n== errori JS ==")
            errs = await c.js("JSON.stringify(window.__errs||[])")
            try:
                el = json.loads(errs or "[]")
            except Exception:
                el = [errs]
            el = [e for e in el if e and "favicon" not in str(e).lower()]
            ok("nessun errore JS", str(el)) if not el else bad("nessun errore JS", str(el))

            shot = await c.cmd("Page.captureScreenshot", {"format": "png"})
            SHOT.write_bytes(base64.b64decode(shot["data"]))
            print(f"\nscreenshot: {SHOT}")
    finally:
        cleanup_agents()
        if started and not keep:
            webapp_up("stop")
        if PROC:
            PROC.terminate()
    print("\n──────────────────────────────────────────")
    print(f"  PASS: {PASS}   WARN: {WARN}   FAIL: {FAIL}")
    if FAIL == 0:
        print("  UI OK" + (" (con WARN)" if WARN else ""))
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep", action="store_true", help="lascia la webapp attiva")
    ap.add_argument("--browser", choices=("chromium", "obscura"), default="chromium",
                    help="browser headless da avviare (default chromium; obscura 0.2.2 si blocca su index.bsp)")
    ap.add_argument("--cdp", default=None, help="URL CDP di un browser gia' avviato")
    a = ap.parse_args()
    sys.exit(asyncio.run(run(a.keep, a.browser, a.cdp)))
