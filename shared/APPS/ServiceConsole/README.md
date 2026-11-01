# ServiceConsole (GMBasic) — first service with a GUI

First application of the **daemon + GUI** series in GMBasic. It serves as a test bench for:
- the **daemon manager** (`gmb daemon create/install/register/start/stop/delete`, launchd/systemd) — see
  `Knowledge/GMBasic-Daemon-Manager-Valutazione.md`;
- the **polymorphic GUI** (TUI widgets rendered on the native GUI via `IControlSurface`) — see
  `Knowledge/TUI-GUI-Input-SDL2-Analisi-Polimorfismo.md`.

## What it does
- **Service (daemon)**: in-language HTTP server with `GET /healthz`, `GET /status`, `GET /config`; cooperative loop
  (`SLEEP`, no busy-wait); state in memory (uptime in ticks). The real lifecycle (SIGTERM/restart) will be the
  daemon manager's job.
- **GUI** (`mode=gui`): in-process control console (TUI) — header with port/active/state, aligned **service
  table** (name/port/state, in-place refresh via `LOCATE`), row **selection** (`>`), **actions**
  (`a` start all · `f` stop all · `s` stop sel · `t` start sel · `r` restart sel), **log panel**
  (6 lines, scroll), active tray (pump + menu). Single state logic: `PollServices` (in-process, no
  HTTP round-trip). In the future the same logic will run on a **pixel** window without changes (polymorphic widgets).
- **Console**: poll of the configured services (`HTTP.GET /healthz` every 5s) → aggregate `N/M active`, detail
  per service, state in **plain text** on `/` (no HTML) — the app is 100% console.
- **Start/stop from the tray**: *Start services* (via `gmb daemon start <nome>`, launchd) and *Stop services*
  (via `GET /control/stop` on each service, which stops itself). Endpoint `/control/stop` exposed by the service.
- **Tray** (in daemon and GUI): icon in the OS bar (default: GMBasic brand; `icon=` for yours), menu
  **State** (updates the tooltip), **Open console** (opens the **TUI console** in Terminal via `console_tui.sh`),
  **Exit** (orderly stop). Works on macOS (VMC and IL) and Windows (compiled).
- **Status (default)**: banner + configuration + state, with no server and no window (for CI/diagnostics).

## Layout
```
APPS/ServiceConsole/
  README.md              this file
  TODO.md                backlog
  serviceconsole.cfg     configuration (key=value)
  src/ServiceConsole.bas entry point (SUB Main)
```

## Configuration (`serviceconsole.cfg`)
```
name     = service-console
port     = 8770
services = marketdata:8771,strategy:8772
mode     = gui           # status | daemon | gui (default: console)
maxsec   = 86400
tray     = 1             # 0 = no icon
# icon   = /percorso/icona.png   (empty = GMBasic brand)
```
Lines starting with `#` are comments. The file is searched in `IO.CWD()` and then, walking up the
parents, in `APPS/ServiceConsole/` and `shared/APPS/ServiceConsole/`.

## Build & run

From the **monorepo ROOT** (`GMBasicMonoRepo/`):

```sh
# status (default) — non-interactive
tools/sh/harness.sh il shared/APPS/ServiceConsole/src/ServiceConsole.bas

# daemon (HTTP) and GUI (console TUI): compile-time modes → `#DEF` at the top of the file
#   #DEF SERVICECONSOLE_MODE_DAEMON   or   #DEF SERVICECONSOLE_MODE_GUI
tools/sh/harness.sh il shared/APPS/ServiceConsole/src/ServiceConsole.bas

# console TUI (app script, sets the CWD = shared/ itself and resolves the tools from the root)
./shared/APPS/ServiceConsole/console_tui.sh
./shared/APPS/ServiceConsole/console_gui.sh
```

The `tools/runners/` runners do not pass `-D` (no `-D` in `test_ilrun`/`test_pcode`/`test_vmrun`);
the in-source equivalent is `#DEF <SIMBOLO>` (gap tracked in the vault:
`Todo/(open) - CLI-Args-e-Env-per-Applicazioni-BASIC`). Endpoints (daemon): `curl http://localhost:8770/healthz` · `/status` · `/config`.

GUI keys: `a` start all · `f` stop all · `s` stop selected · `t` start selected ·
`r` restart selected · `1-8` select row · `q`/`ESC` exit. Automatic exit at `maxsec`.
The standalone TUI console (client, via HTTP) is `src/ServiceConsoleTui.bas` (`console_tui.sh`).

## Status

- **v0.1** — status + HTTP daemon + TUI GUI.
- **v0.2** — complete control GUI (table/selection/actions/log/tray), menu-bar tray, daemon
  manager (`gmb daemon …`), TRAY/SHELL/HTTP parity on 4 backends. Verified on VM/C and C99Native.
- Next steps in `TODO.md`.

> [!warning] **Verification 2026-10-31 (monorepo).**
> 1. **It compiles**: it used four non-existent builtins (`FORM_RESET`, `FORM_FRAME`, `FORM_CLOSED`,
>    `FORM_CLOSE`); now it goes through the real FFIs of `libgmb_gfx` (`gmb_tui_controls_reset`,
>    `gmb_ui_frame`, `gmb_form_closed`, `gmb_form_request_close`), come fa il progetto IDE TUI
>    (`GMBasicTuiIde`, in sviluppo fuori dal repo pubblico).
> 2. **Default mode (status) fails at runtime**: `OverflowException: Value was either too
>    large or too small for an Int16` — opened as bug
>    `Bugs/(open) - ServiceConsole-Status-Overflow-Int16.md` in the monorepo vault.
> 3. The `daemon`/`gui` modes were not re-run after the fix (they require `#DEF …` at the top of the
>    file: the runners do not pass `-D`).
