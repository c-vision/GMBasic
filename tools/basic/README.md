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
| `ffi_bind.bas` | genera le `DECLARE NATIVE` da un header C (AST di `clang`), oppure `--check` verifica che un `.inc` scritto a mano combaci con l'header (drift = exit 1) | stdout / `--out <file.inc>` |
| `bsp_engine_parity.bas` | parità di resa fra motore BSP managed e C sulla webapp (normalizza timestamp/uptime/pid) | stdout, exit 1 se differenze |
| `scene_census.bas` | censimento della superficie `SCENE`: per ogni verbo (tavolo `gmb_p_scene_verb_tk`) e ogni lettura (registro `stdlib_tokens.csv`) verifica TokenKind, emitter C99, VM/C, ponte C#, facciata runtime, MiniTest, sample e guida; segnala anche il drift inverso (TokenKind/syscall che nessuna superficie raggiunge). Exit 1 se manca una gamba o un MiniTest | stdout, exit 0/1 (`--quiet` = solo lacune) |
| `webapp_ui_cdp.bas` | click-through della dashboard con browser headless via CDP (palette, rail, Inbox, scorciatoie, tema, creazione agente, filtri, SSE, WebSocket canvas, errori JS) | stdout, exit 1 se FAIL |

### `ffi_bind.bas` — bindings FFI dall'header, non a mano

```bash
# genera le DECLARE del sottoinsieme ofx_fin_ (stdout)
gmb run tools/basic/ffi_bind.bas -- --header shared/libs/finance/include/gmbfinance.h \
    --lib gmbfinance --only ofx_fin_ --strip ofx_fin_ --prefix FINN_

# gate: le dichiarazioni scritte a mano combaciano con l'header? (exit 1 = drift)
gmb run tools/basic/ffi_bind.bas -- --header shared/libs/finance/include/gmbfinance.h \
    --lib gmbfinance --only ofx_fin_ --strip ofx_fin_ --prefix FINN_ \
    --check shared/include/finance.inc
```

Mappatura C→BASIC: `void`→`SUB`, `double|float`→`DOUBLE`, interi/`bool`→`LONG`,
`char*`→`STRING`, `void*`→`QUAD`, `T*` con un parametro di lunghezza→`p() AS BYTE|DOUBLE`,
`T**`→`BYREF`. Struct/union per valore, varargs e function pointer si **saltano con messaggio**
(mai silenzioso). `--check` confronta per **ALIAS** e per **forma tipologica**: i nomi BASIC e i
nomi dei parametri possono differire, arità/tipi/BYREF/array no.
Casi verificati sul campo: `finance.inc` 20/20, `bits.inc` 21/21.

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
