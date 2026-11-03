# ServiceConsole — TODO

## v0.2 — daemon lifecycle (after the daemon manager)
- [ ] Entry point recognized as a daemon (`SUB Main` + cooperative loop) and "no interactive" validation in service mode.
- [ ] **Signals**: SIGTERM/SIGINT → orderly stop (`HTTP SERVER STOP`, flush, exit 0) with timeout; today the loop is time-based.
- [ ] **State/health on disk**: `bin/state.json` (pid, status, uptime, last_error) with heartbeat; `/healthz` reads the real state.
- [ ] **JSON logs** on stdout (level/timestamp) + rotation on the supervisor side.
- [ ] **Config**: secrets outside the file (keychain), env overrides; schema validation.
- [ ] `gmb daemon create|install|register|start|stop|status|logs|delete` integration (macOS launchd LaunchAgent / Linux systemd --user).

## v0.2b — Tray / menu bar (request 2026-10-02)
- [x] `TRAY.*` builtin + C library (`native/gmb_runtime_c/gmb_tray*`): macOS NSStatusItem, Windows Shell_NotifyIcon.
- [x] Main-thread queue + pump (SDL pattern): VMC and IL verified; icon created/removed in the ServiceConsole daemon.
- [x] Menu: Status, Open console, Quit (created).
- [x] Menu actions via `TRAYLAST()` (polling): Status→tooltip, Open console→`SYSTEM.SHELLOUT("open ...")`, Quit→stop.
- [ ] `ON TRAY` statement (ergonomic, handler as a GPM task) — future evolution.
- [ ] Template icon, LSUIElement (agent without Dock), SDL coexistence (a single NSApplication, no double run loop).
- [ ] Verification: `tray_selftest()` + AX (`ManagedMacAx`). See `Knowledge/Tray-MenuBar-GMBasic-Valutazione.md`.

## v0.3 — polymorphic GUI
- [x] In-process TUI control GUI: service table (name/port/status, columns aligned with `PADR`),
      row selection, start/stop/restart actions (sel/all), log panel (6 lines, scroll), in-place refresh
      via `LOCATE` (window-relative), tray pump/menu. Verified on VM/C (`mode=gui`, maxsec).
- [x] Use of the formatting builtins (`PADL/PADR/PADC`, 2-arg form with fill=space) — C99Native emitter fix
      (`st_pad` required 3 arguments) and removal of the duplicated `Pad` helper in the apps.
- [ ] The status window uses the **TUI widgets** (`Control`) rendered on the native GUI via `IControlSurface`/`GfxSurface`.
- [ ] Status badges, graphical start/stop/restart buttons (widgets), scrollable log with scrollbar.
- [ ] Uptime/latency chart (Candlestick/LineChart widget).
- [ ] BMP screenshot as a gate (`test_graphics_*`) for the automatic verification of the GUI.

## v0.4 — platform
- [x] Service polling (`HTTP.GET /healthz`), aggregate, HTML dashboard on `/`.
- [x] Start/stop from the tray (`daemon start` + `GET /control/stop`).
- [ ] **Security**: `/control/stop` without auth (bind localhost or token) — to be hardened.
- [ ] Reading the **daemon registry** (`gmb daemon list --json`) and control (start/stop/restart/log).
- [ ] Health aggregation of N services (marketdata, strategy-runner, backtest, execution, risk, api) with dependencies/readiness.
- [ ] Alerts and kill-switch (stop propagation in a chain).

## Notes
- Rules: GPM non-derogable (handler = GPM task); SDL on the main thread (the TUI GUI does not use SDL; the pixel GUI does).
