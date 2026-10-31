# ServiceConsole — TODO

## v0.2 — daemon lifecycle (dopo il daemon manager)
- [ ] Entry point riconosciuto come daemon (SUB Main + loop cooperativo) e validazione "no interattivi" in modalità servizio.
- [ ] **Segnali**: SIGTERM/SIGINT → stop ordinato (`HTTP SERVER STOP`, flush, exit 0) con timeout; oggi il loop è a tempo.
- [ ] **State/health su disco**: `bin/state.json` (pid, status, uptime, last_error) con heartbeat; `/healthz` legge lo stato reale.
- [ ] **Log JSON** su stdout (livello/timestamp) + rotazione lato supervisore.
- [ ] **Config**: secrets fuori dal file (keychain), override da env; validazione schema.
- [ ] Integrazione `gmb daemon create|install|register|start|stop|status|logs|delete` (launchd LaunchAgent macOS / systemd --user Linux).

## v0.2b — Tray / menu bar (richiesta 2026-10-02)
- [x] `TRAY.*` builtin + libreria C (`native/gmb_runtime_c/gmb_tray*`): NSStatusItem macOS, Shell_NotifyIcon Windows.
- [x] Coda main-thread + pump (pattern SDL): VMC e IL verificati; icona creata/rimossa nel ServiceConsole daemon.
- [x] Menu: Stato, Apri console, Esci (creati).
- [x] Azioni menu via `TRAYLAST()` (polling): Stato→tooltip, Apri console→`SYSTEM.SHELLOUT("open ...")`, Esci→stop.
- [ ] `ON TRAY` statement (ergonomico, handler come task GPM) — evoluzione futura.
- [ ] Icona template, LSUIElement (agent senza Dock), coesistenza SDL (un solo NSApplication, no doppio run loop).
- [ ] Verifica: `tray_selftest()` + AX (`ManagedMacAx`). Vedi `Knowledge/Tray-MenuBar-GMBasic-Valutazione.md`.

## v0.3 — GUI polimorfica
- [x] GUI di controllo TUI in-process: tabella servizi (nome/porta/stato, colonne allineate con `PADR`),
      selezione riga, azioni start/stop/restart (sel/tutti), pannello log (6 righe, scroll), refresh
      in-place via `LOCATE` (window-relative), tray pump/menu. Verificata su VM/C (`mode=gui`, maxsec).
- [x] Uso dei builtin di formattazione (`PADL/PADR/PADC`, forma 2-arg con fill=spazio) — fix emitter C99Native
      (`st_pad` esigeva 3 argomenti) e rimozione dell'helper `Pad` duplicato nelle app.
- [ ] La finestra di stato usa i **widget TUI** (`Control`) resi su GUI nativa via `IControlSurface`/`GfxSurface`.
- [ ] Semafori di stato (badge), bottoni grafici start/stop/restart (widget), log scrollabile con scrollbar.
- [ ] Chart di uptime/latenza (widget Candlestick/LineChart).
- [ ] Screenshot BMP come gate (`test_graphics_*`) per la verifica automatica della GUI.

## v0.4 — piattaforma
- [x] Poll dei servizi (`HTTP.GET /healthz`), aggregato, dashboard HTML su `/`.
- [x] Start/stop dal tray (`daemon start` + `GET /control/stop`).
- [ ] **Sicurezza**: `/control/stop` senza auth (bind localhost o token) — da irrobustire.
- [ ] Lettura del **registry dei daemon** (`gmb daemon list --json`) e controllo (start/stop/restart/log).
- [ ] Aggregazione health di N servizi (marketdata, strategy-runner, backtest, execution, risk, api) con dipendenze/readiness.
- [ ] Alert e kill-switch (propagazione stop a catena).

## Note
- Regole: GPM inderogabile (handler = task GPM); SDL sul main thread (la GUI TUI non usa SDL; la GUI pixel sì).
- La logica di stato deve essere **una sola** e girare su TUI ora e GUI dopo (nessun fork di codice).
