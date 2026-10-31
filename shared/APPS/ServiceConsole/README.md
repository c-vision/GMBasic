# ServiceConsole (GMBasic) — primo servizio con GUI

Prima applicazione della serie **daemon + GUI** in GMBasic. Serve come banco di prova per:
- il **daemon manager** (`gmb daemon create/install/register/start/stop/delete`, launchd/systemd) — vedi
  `Knowledge/GMBasic-Daemon-Manager-Valutazione.md`;
- la **GUI polimorfica** (widget TUI resi su GUI nativa via `IControlSurface`) — vedi
  `Knowledge/TUI-GUI-Input-SDL2-Analisi-Polimorfismo.md`.

## Cosa fa
- **Servizio (daemon)**: HTTP server in-language con `GET /healthz`, `GET /status`, `GET /config`; loop cooperativo
  (`SLEEP`, nessun busy-wait); stato in memoria (uptime in tick). Il lifecycle reale (SIGTERM/restart) sarà del
  daemon manager.
- **GUI** (`mode=gui`): console di controllo in-process (TUI) — intestazione con porta/attivi/stato, **tabella
  servizi** allineata (nome/porta/stato, refresh in-place via `LOCATE`), **selezione** riga (`>`), **azioni**
  (`a` avvia tutti · `f` ferma tutti · `s` stop sel · `t` start sel · `r` restart sel), **pannello log**
  (6 righe, scroll), tray attivo (pump + menu). Unica logica di stato: `PollServices` (in-process, niente
  round-trip HTTP). In futuro la stessa logica girerà su finestra **pixel** senza modifiche (widget polimorfici).
- **Console**: poll dei servizi configurati (`HTTP.GET /healthz` ogni 5s) → aggregato `N/M attivi`, dettaglio
  per servizio, stato in **testo semplice** su `/` (nessun HTML) — l'app è 100% console.
- **Start/stop dal tray**: *Avvia servizi* (via `gmb daemon start <nome>`, launchd) e *Ferma servizi*
  (via `GET /control/stop` su ciascun servizio, che si ferma da solo). Endpoint `/control/stop` esposto dal servizio.
- **Tray** (in daemon e GUI): icona nella barra del SO (default: marchio GMBasic; `icon=` per la tua), menu
  **Stato** (aggiorna il tooltip), **Apri console** (apre la **console TUI** in Terminal via `console_tui.sh`),
  **Esci** (stop ordinato). Funziona su macOS (VMC e IL) e Windows (compilato).
- **Status (default)**: banner + configurazione + stato, senza server né finestra (per CI/diagnostica).

## Layout
```
APPS/ServiceConsole/
  README.md              questo file
  TODO.md                backlog
  serviceconsole.cfg     configurazione (key=value)
  src/ServiceConsole.bas entry point (SUB Main)
```

## Configurazione (`serviceconsole.cfg`)
```
name     = service-console
port     = 8770
services = marketdata:8771,strategy:8772
mode     = gui           # status | daemon | gui (default: console)
maxsec   = 86400
tray     = 1             # 0 = niente icona
# icon   = /percorso/icona.png   (vuoto = marchio GMBasic)
```
Le righe che iniziano con `#` sono commenti. Il file è cercato in `IO.CWD()` e poi, risalendo i
parent, in `APPS/ServiceConsole/` e `shared/APPS/ServiceConsole/`.

## Build & run

Dalla **ROOT del monorepo** (`GMBasicMonoRepo/`):

```sh
# status (default) — non interattivo
tools/sh/harness.sh il shared/APPS/ServiceConsole/src/ServiceConsole.bas

# daemon (HTTP) e GUI (console TUI): modi compile-time → `#DEF` in testa al file
#   #DEF SERVICECONSOLE_MODE_DAEMON   oppure   #DEF SERVICECONSOLE_MODE_GUI
tools/sh/harness.sh il shared/APPS/ServiceConsole/src/ServiceConsole.bas

# console TUI (script dell'app, imposta da sé la CWD = shared/ e risolve i tool dalla root)
./shared/APPS/ServiceConsole/console_tui.sh
./shared/APPS/ServiceConsole/console_gui.sh
```

I runner di `tools/runners/` non passano `-D` (nessun `-D` in `test_ilrun`/`test_pcode`/`test_vmrun`);
l'equivalente in-sorgente è `#DEF <SIMBOLO>` (gap tracciato nel vault:
`Todo/(open) - CLI-Args-e-Env-per-Applicazioni-BASIC`). Endpoint (daemon): `curl http://localhost:8770/healthz` · `/status` · `/config`.

Tasti GUI: `a` avvia tutti · `f` ferma tutti · `s` stop selezionato · `t` start selezionato ·
`r` restart selezionato · `1-8` seleziona riga · `q`/`ESC` esci. Uscita automatica a `maxsec`.
La console TUI standalone (client, via HTTP) è `src/ServiceConsoleTui.bas` (`console_tui.sh`).

## Stato

- **v0.1** — status + daemon HTTP + GUI TUI.
- **v0.2** — GUI di controllo completa (tabella/selezione/azioni/log/tray), tray menu-bar, daemon
  manager (`gmb daemon …`), parità TRAY/SHELL/HTTP su 4 backend. Verificata su VM/C e C99Native.
- Prossimi passi in `TODO.md`.

> [!warning] **Verifica 2026-10-31 (monorepo).**
> 1. **Compila**: usava quattro builtin inesistenti (`FORM_RESET`, `FORM_FRAME`, `FORM_CLOSED`,
>    `FORM_CLOSE`); ora passa dalle FFI reali di `libgmb_gfx` (`gmb_tui_controls_reset`,
>    `gmb_ui_frame`, `gmb_form_closed`, `gmb_form_request_close`), come fa `GMBasicTuiIde/src/GMBasicTuiIde.bas`.
> 2. **Modo default (status) va in errore a runtime**: `OverflowException: Value was either too
>    large or too small for an Int16` — aperto come bug
>    `Bugs/(open) - ServiceConsole-Status-Overflow-Int16.md` nel vault del monorepo.
> 3. I modi `daemon`/`gui` non sono stati ri-eseguiti dopo il fix (richiedono `#DEF …` in testa al
>    file: i runner non passano `-D`).
