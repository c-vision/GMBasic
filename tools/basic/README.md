# tools/basic — strumenti del monorepo in BASIC

Gli strumenti di analisi e manutenzione del monorepo si scrivono **in BASIC** e si eseguono
sulla gamba nativa (C99Native). Il Python **non si usa**: un sorvegliante
(`tools/sh/no_python_guard.sh`) mette in quarantena qualunque `.py` compaia nell'albero
(esclusi `build/`, `_deps/`, `.build/`, `.content/`, `node_modules/`, `.git/`, `vendor/`).

## Come si esegue uno strumento

Dalla root del monorepo:

```sh
bash tools/sh/gmb_c99native_run.sh ../tools/basic/<nome>.bas <suffisso>
# con argomenti:
bash tools/sh/gmb_c99native_run.sh ../tools/basic/<nome>.bas <suffisso> -- <argomenti...>
```

Il runner compila il `.bas` in `tools/basic/bin/` (gitignored) e lo esegue con CWD =
`tools/basic`; gli strumenti risalgono da soli alla root del monorepo.

## Strumenti

| strumento | cosa fa | output |
|---|---|---|
| `gmbsuite_import_audit.bas` | audit simbolo-per-simbolo dell'import delle librerie `gmbsuite` (copie migrate, ABI dei dylib via `nm`, pacchetti, inventario per progetto). Exit 1 se ci sono lacune esatte | `tools/reports/gmbsuite-import-audit.md` + `.tsv` |
| `string_dispatch_scan.bas` | gate Phase 4: residui di dispatch per lexeme su membri `Coll*` (`CollMembers.Is` obbligatorio) | stdout, exit 1 se residui |
| `sample_audit.bas` | censimento dei Sample (`shared/samples/**`): SURFACE / MODULO / FIXTURE / PACKAGE / DIDATTICO / CORTO / ENV | `tools/reports/sample_audit.txt` |
| `sample_minitest_gaps.bas` | casi d'uso dei Sample SURFACE non previsti dai MiniTest canonici (ARITA / ASSENTE) | `tools/reports/sample_minitest_gaps.txt` |
| `minitests_work_absolute.bas` | rende assoluti i path `work/` dei MiniTest (`GMBWORK$`) o aggiunge `CHDIR PROJECTFOLDER()` | stdout (dry-run di default, `--apply` scrive con backup) |
| `cellfont_from_bdf.bas` | rigenera la tabella ASCII 8x16 del font a celle del TUI da un BDF (Terminus OFL) | header C indicato |
| `gen_hpack_huffman.bas` | rigenera le tabelle HPACK (RFC 7541) dai dati ufficiali; `--check` verifica l'allineamento | due header C |
| `skills_attribution.bas` | rigenera l'attribuzione/licenza delle skill di terzi | `shared/skills/ATTRIBUTION.md` + `attribution.json` |
| `bsp_engine_parity.bas` | parità di resa fra motore BSP managed e C sulla webapp (normalizza timestamp/uptime/pid) | stdout, exit 1 se differenze |
| `webapp_ui_cdp.bas` | click-through della dashboard con browser headless via CDP (palette, rail, Inbox, scorciatoie, tema, creazione agente, filtri, SSE, WebSocket canvas, errori JS) | stdout, exit 1 se FAIL |

### Nota su `webapp_ui_cdp.bas`

Il request/response CDP passa dal **client WebSocket in BASIC** (`WEBSOCKET CONNECT` +
`ws.OnMessage`), reso possibile dal receive-loop lato client in `gmb_ws_client.c` e
dall'emissione di `ws.Send`/`ws.Close` (prima compilavano in un lookup JSON no-op).

Esito di riferimento sul campo: **20 PASS / 0 WARN / 0 FAIL**, con screenshot PNG valido in
`/tmp/webapp_ui.png`. Due punti che erano WARN e sono stati chiusi:

- **screenshot**: `FROMBASE64` ora costruisce la stringa binaria con la lunghezza esatta
  (`gmb_string_from_bytes`) invece di `gmb_string_alloc` (che troncava al primo NUL) —
  vedi `Bugs/(solved) - C99-FROMBASE64-Payload-Grandi.md`;
- **WebSocket del canvas**: Chromium headless a volte non consegna il primo `onopen` alla
  pagina. Il controllo verifica l'endpoint **dal contesto della pagina** con una socket
  fresh (se apre, il server WS della webapp è raggiungibile) e riporta il dettaglio.

## Perché non Python

- una sola catena di build e di test (la stessa dei 4 backend), senza dipendenze di sistema;
- gli strumenti usano la stessa libreria standard del linguaggio (IO, SYSTEM, JSON, GREP, HTTP)
  e quindi verificano anche il runtime;
- niente script orfani: ogni strumento vive qui, è versionato e ha il suo comando.

## Sorvegliante

```sh
tools/sh/no_python_guard.sh status        # stato
tools/sh/no_python_guard.sh scan          # quarantena dei .py presenti (una passata)
tools/sh/no_python_guard.sh run           # in primo piano
tools/sh/no_python_guard.sh install       # avvio automatico al login (launchd)
tools/sh/no_python_guard.sh uninstall     # rimozione
tools/sh/no_python_guard.sh install-hook  # hook pre-commit che rifiuta i .py
```

I file quarantena finiscono in `~/.gmbasic/quarantine/py/<stamp>/<nome>` e l'evento è
registrato in `/tmp/gmb_pyguard.log`. Sorgente del sorvegliante: `tools/c/pyguard.c`.
