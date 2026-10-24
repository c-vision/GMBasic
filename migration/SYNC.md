# SYNC GMBasic → GMBasicMonoRepo

## Base
- BASE_COMMIT: `c9ad151ebfaad12f6f312850102c807074de9776` (2026-10-23)
- **Il solo commit NON basta**: il working tree di GMBasic conteneva **23 modifiche non
  committate** in `native/` (es. `gmb_sdl_console.c` con `extern gmb_tui_menu_any_open`); con il
  solo HEAD la build C fallisce. Lo snapshot iniziale usa quindi **working tree + commit base**.
- Inventario modifiche al momento dello snapshot: `git -C GMBasic status --short native | wc -l`
  = 23 (elencate sotto, hash del file di inventario da rigenerare ad ogni sync).

## Snapshot iniziale (2026-10-23)
- `native/`, `gmbllm/` copiati dal **working tree** (esclusi build/_deps/oggetti).
- `Resources/libs` (837 MB, include dylib non tracciate come `libMoltenVK.dylib`) e
  `libs-runtime` (963 MB) copiati dal working tree: dipendenze locali di build, NON da git.
- `tools/` e `GMBasic/tools` sono **symlink temporanei** a `migration/tools-unclassified`
  (i tool vanno riscritti per progetto: F2/F3). Da rimuovere prima della pubblicazione.
- `shared/`, `GMBasicSharp/`, `spec/registry`, `GMBasic/tests-mixed`, `migration/tools-unclassified`
  derivano dallo snapshot HEAD.

## Build C (verificata)
- `gmb_runtime`: OK (`cmake -DCMAKE_BUILD_TYPE=Release`).
- `gmb_vm_host`: OK con `-DGMB_HAS_COMMERCIAL_DB=ON -DGMB_VM_LINK_COMPILER=ON`
  (i default freschi li disattivano → `gmb_db.h` mancante).

## Prossimi sync
1. `git -C GMBasic fetch` + `git diff --name-status <base>..HEAD`.
2. Aggiornare `<base>` e riapplicare ai path nuovi (`GMBasic/`, `GMBasicSharp/`, `shared/`).
3. Registrare qui commit, file, esito; se il working tree cambia durante il port, ri-sync.

## POLICY TOOLS (2026-10-23)
- Nessun Python. C → tool in C o GMBasic `.bas`; Sharp → tool in C#; shell solo bootstrap/CI.
- I symlink `tools`/`GMBasic/tools` sono temporanei e vanno sostituiti da tool riscritti.

## REPL C# ripristinata (2026-10-23)
- Copiata `REPL/` da `GMBasic-Deprecated/REPL` in `GMBasicSharp/REPL` (26 file).
- Ripristinati gli originali C# necessari: **`common/ClipperLib` completo** (da Deprecated),
  `SdlConsole*.cs`, `AlertBox.cs`.
- `GMBasicSharp` usa ora gli **originali C#** per lo stato/controlli finestra: rimossa la copia
  proxy `common/CommonLib/Consoles/Tui/State` (solo lato Sharp); `IlBackendRuntime` rifà
  riferimento a `ClipperLib.csproj`; `GmBasicInputSource` → `ClipperLib.App`.
- `ScreenConfig.Palettes.GetConsoleNamed()` ripristinato (per ora C-backed).
- **Build REPL verde** (0 errori).
- Il progetto C resta C-only: nessun riferimento a ClipperLib.

## C toolchain + Sharp solution (2026-10-23)
- C: `gmb_runtime`, `gmb_vm_host`, `gmb_compiler_c` (build `gmb`) costruiti; C99Native è
  consumato dai sorgenti dal gate (nessun eseguibile standalone). Configurazione:
  `-DGMB_HAS_COMMERCIAL_DB=ON -DGMB_VM_LINK_COMPILER=ON`.
- **12 funzioni "solo IL" verificate GIÀ implementate in C**: C99Native le emette
  (`gmb_c99_emit.c` 9530-9560) e VM/C le dispatcha (`gmb_vm_tui.c` 400-419, 402,404,405,
  411-419). Il gap 12 era **stale** (manifest audit non rigenerati: richiede `--all`).
- Sharp: creata `GMBasicSharp/GMBasicSharp.slnx` (28 progetti main); build **0 errori**.
  Corretto `IlBackendCodegen/genrunner` (path `../../../Executor`).
- **C99Backend ritirato** da Sharp: spostato in `GMBasic-Deprecated/common-C99Backend-2026-10-23`
  (+ test in `tests-GMBASIC.Backend.C99.Tests-2026-10-23`).
- Policy: rimosso Python dai path vivi. `cmake/embed_spirv.py` → `cmake/embed_spirv.cmake`
  (generazione header SPIR-V in CMake puro, byte/length identici); 4 script in `shared/` e
  `gmb_db_c/tools/fill_1to1.py` spostati in Deprecated.
- Confini: `find GMBasic -name '*.cs'` = **0**; Sharp ha 2 helper C di test
  (`tests-mixed/.../run_*.c`) da spostare nel progetto C.
- `migration/_import` (snapshot completo) contiene ~270 .py legati ai tool legacy: **temporaneo**,
  da escludere dalla pubblicazione; `tools/` e `GMBasic/tools` sono symlink temporanei.

## Verifica C dopo il cambio SPIR-V (2026-10-23)
- `gmb_runtime` ricostruita OK; `gmb_gfx_spirv.h` generato da CMake (sprite.vert=1508,
  sprite.frag=636 byte, identici al vecchio script).
- `tests/gmb_gaps_selfcheck.c` era **stale** rispetto all'API (`snap` → `snap_zone`, edges
  stringa): aggiornato; il test WindowManager ora passa.
- Restano 2 FAIL **pre-esistenti** nel selfcheck: `Graphics Texture Rotation` (serve GPU/contesto)
  e `Keyboard read_line` (serve TTY). Target `gmb_gfx_present_input_selfcheck` non compila per
  header SDL2 mancanti (config test, non la lib).

## Audit rigenerato + confini (2026-10-23)
- Audit `--all` eseguito su staging (`/var/folders/.../gmb-audit-stage`): superficie canonica
  **1383**; **PCodeVM, IL, VMC, C99Native = 1383/1383 (100%)**.
- I 12 gap storici (`CONTROLS.*` + `WINDOWS.WINDOWCLOSED`) erano **falsi negativi** dell'estrattore:
  C99Native li emette per nome, PCodeVM via `case SysCall.FORM_*`, VMC via `vm_form_*`.
  Patch (solo tool legacy in `migration/tools-unclassified/audit/Program.cs`): riconosce
  `new HashSet<BuiltInFunction>{...}`, i rami per nome del percorso FORM e i case SysCall; C99
  (C# transpiler ritirato) escluso da `--all`.
- Manifest in `spec/registry/manifests/` (C99Native, IL, PCodeVM, VMC); report in
  `shared/docs/comparison/audit-backends.md`. **Da replicare nel futuro audit-tool C**: il
  riconoscimento del dispatch per nome/SysCall è necessario (altrimenti 12 falsi gap).
- Confini: **0 file C in `GMBasicSharp`**; `tests-mixed/native_tests/run_tests.c` →
  `GMBasic/native/tests/native_tests/`; `run_db_checks.c` → `GMBasic/native/gmb_db_c/tests/`;
  `NativeCChecks.cs` → Deprecated. Ripristinato `GMBasicSharp/Directory.Build.props` (matrice).
  `GMBASIC.Database.Native.Tests` compila (0 errori); `GMBasicSharp.slnx` 0 errori.

## Verifica "146 non implementate" (2026-10-24)
- **Falso allarme**: l'audit deduplicava la superficie per solo NOME (1383), mentre il registry
  canonico è per (namespace,nome) (1523). Corretto il tool (chiave ns+nome).
- Confronto insiemi: **canonico 1523 = StdLib 1523**, differenza **0**. Nessuna funzione manca
  da StdLib: né in Sharp né nel repo originale GMBasic → nessuna modifica a GMBasic.
- Con la superficie corretta: IL/PCodeVM/VMC **1523/1523 (100%)**; C99Native **1517/1523**.
- Unici gap reali (C99Native, emissione mancante): `SPEED.SETDURABILITY`, `SPEED.FLUSHDURABILITY`,
  `SPEED.BEGINGROUPCOMMIT`, `SPEED.ENDGROUPCOMMIT`, `SPEED.INFO`, `MOUSE.CLICK`.
  Runtime: set/flush/group-commit esistono in gmb_db; `SPEED.INFO` e `MOUSE.CLICK` (forma area)
  non hanno ancora una funzione C dedicata.

## Sync nuovi commit + chiusura gap SPEED/MOUSE (2026-10-24)
- **Nuovi commit** in GMBasic originale: `6fe9765af` (@GET TYPE MiniTest/sample/guida) e
  `7a474842d` (parità C99Backend campi tipizzati). Copiati in `shared/`:
  `minitests/controls/get_types_parity.bas`, `samples/Community/Clipper/ClipperTypedGets.bas`.
  Le modifiche C99Backend non riguardano il monorepo (backend ritirato).
- **Gap chiusi sui 4 backend** (non solo C99Native): SPEED.SETDURABILITY/FLUSHDURABILITY/
  BEGINGROUPCOMMIT/ENDGROUPCOMMIT/INFO e MOUSE.CLICK (0 arg + area).
  - C condiviso: `gmb_db_speed_begin/end_group_commit`, `gmb_db_speed_{set_durability,info}_str`,
    getter `Database_GetDurabilityFull/GroupCommitDepth`, `gmb_mouse_click[_area]` (Clipper).
  - C99Native: entry `emit_bf_speed` + `emit_bf_mouse` (nessuna logica inline).
  - VM/C: builtin 454-458 (`gmb_builtin.c`) + MOUSE.CLICK in `gmb_bf_393`.
  - IL/PCode: `IlRuntime.Speed{Begin,End}GroupCommit`, flush via `SpeedDriverForArea`
    (prima dipendeva da `_speedHandles`, non popolato da PCode/VM), emissione IL dei token.
  - Fix harness `test_ilrun/Program.cs` (ClipperLib → CommonLib proxy).
- MiniTest `speed/gaps.bas` (EXPECT setdur/flush/begin/end/info/click0/area0).
  Eseguito sul file singolo: **IL OK, PCode OK, VM/C OK, C99Native PASS**.
- Audit rigenerato: **IL/PCodeVM/VMC/C99Native = 1523/1523 (100%)**.

## Import commit + commit in GMBasic (2026-10-25)
- Importati nel monorepo i commit **`7a474842d..8b77bfe00`** (6 commit, tutti `native/`):
  `gmb_compiler.c`, `gmb_c99gpm.c`, `gmb_read.c`, `gmb_vm_core.c`, `gmb_vm_host.c`,
  `gmb_vm_tui.c` — presi da **HEAD** (`git show`), non dal working tree (che ha WIP altrui).
- Build monorepo riverificate: `gmb_ai_llm` + `gmb_runtime` + `gmb_db` + `gmb_vm_host` OK.
- **Commit in GMBasic**: `0bc552167` — 16 file, +231/−8 (runtime/DB/C99Native/VM/C/IL/PCode,
  MiniTest `speed/gaps.bas`). `gmb_builtin.c` committato **solo per i propri hunk** (46 righe):
  le 419 righe di WIP di un'altra AI restano non committate. `test_ilrun/Program.cs` è
  gitignored: fix locale non committato.
- Stato verificato: IL/PCode/VM/C/C99Native PASS su `speed/gaps.bas`; audit 1523/1523.

## C99Backend ritirato nel repo ORIGINALE (2026-10-25)
- `common/C99Backend` → `GMBasic-Deprecated/common-C99Backend-2026-10-25` (incluso il WIP
  non committato di un'altra AI, preservato) e `tests/GMBASIC.Backend.C99.Tests` →
  `.../tests-GMBASIC.Backend.C99.Tests-2026-10-25`.
- Rimossi: voci `GMBASIC.sln`, `ProjectReference` in `tools/audit`, blocco `GmbBackendC99`
  in `Directory.Build.props`/`.targets`, `InternalsVisibleTo` C99 nei 3 csproj, gate/step
  C99 in `tools/{gmb_http_probe,llm_llvm_gate,test_mcp_e2e}.sh`, voci in
  `tools/check_file_size.sh`; script `gmb_c99_rebuild.sh`/`worktree_helper.sh` in Deprecated.
- `tools/audit`: propagata la versione corretta (chiave ns+nome, speed/mouse, PCode SysCall);
  `--c99`/`--registry-c99` ora rispondono "ritirato" (exit 2).
- Test `NativeParityTests` che leggeva C99Backend: rimosso (obsoleto); `NativeDb.cs` ripristinato.
- Verifica: `dotnet build GMBASIC.sln` **0 errori**; `speed/gaps.bas` **IL/PCode/VM/C/C99Native PASS**.
- Nota: `tools/check_file_size.sh --quiet` resta rosso per file monolitici preesistenti
  (`gmb_builtin.c` 18k righe non in allowlist) — non legato a questo lavoro.

## Import READ nativo + ritiro C99Backend (2026-10-25)
- Importati da HEAD i commit **`5dff233c4` / `edd4da60b` / `66ab81ddc`** (READ nativo via P/Invoke,
  PCode/IL allineati): file C copiati da HEAD in `GMBasic/native/`; file Sharp senza divergenze
  copiati; `IlRuntime.cs` e `PCodeProcessor.Syscalls.cs` presi da HEAD e riapplicata la
  trasformazione namespace Sharp (CommonLib→ClipperLib) → `GMBasicSharp.slnx` 0 errori.
- Ripristinato in `GMBasic/native/gmb_runtime_c/CMakeLists.txt` l'embed SPIR-V in CMake puro
  (`embed_spirv.cmake`): il HEAD originale usa ancora `embed_spirv.py` (Python).
- Build: `gmb_runtime`, `gmb_ai_llm`, `gmb_db`, `gmb_vm_host` OK.
- C99Backend: assente dal monorepo (già rimosso); `parity5` ora 4 gambe nel repo originale.

## Rifinitura Sharp post-ritiro C99 (2026-10-25)
- Rimossa da `GMBasicSharp/REPL` la vecchia gamba C99: blocco `#if GMB_C99`/`RunC99Safe`,
  parsing `--c99`/`UseC99`, help POC e `C99DebugDriver` (stub) spostato in
  `GMBasic-Deprecated/GMBasicSharp-REPL-C99-2026-10-25`; `DebugBackend` ora {PCode, VmC}.
- `GMBasicSharp.slnx` e `REPL` compilano 0 errori; C: `gmb_runtime`, `gmb_ai_llm`, `gmb_db`,
  `gmb_vm_host`, `gmb_compiler_c` OK.
- Compliance: 0 C# nel progetto C, 0 C nel progetto Sharp, 0 .py fuori da migration/tools/build.

## Pulizia pre-repo (2026-10-25)
- `migration/_import` (snapshot duplicato, 270 .py) spostato in
  `GMBasic-Deprecated/monorepo-import-2026-10-25`.
- Rimossi i symlink `tools/` e `GMBasic/tools/` → `migration/tools-unclassified`.
- `GMBasic/tools/` è ora reale e contiene solo gli script shell del progetto C presi da HEAD
  (`gmb_ensure_ai_libs.sh`, `install_mlx_dylib.sh`, `gmb_c99native_run.sh`, `parity5` 4 gambe,
  `test_minitests_c99native`, `check_file_size.sh`, `rebuild_ai_native.sh`, `gmb_http_probe.sh`,
  `llm_llvm_gate.sh`, `test_mcp_e2e.sh`).
- Build `gmb_vm_host` OK con i tool reali; nessun `.py` in `GMBasic/tools`.
- `migration/tools-unclassified` resta come archivio legacy (fuori dai repo da pubblicare).

## F3 — runtime-abi (2026-10-25)
- Creato `runtime-abi/`: `check_abi.c` (dlopen/dlsym), `CMakeLists.txt`, README e 4 elenchi
  di entry point estratti dai binding Sharp: `gmb_db` (51), `gmb_gfx` (385), `gmb_ai_llm` (60),
  `gmb_ai_modelio` (4).
- Verifica: **51/51, 385/385, 60/60, 4/4** simboli presenti nelle dylib buildate.
- `conformance/` ancora da popolare (runner C per corpus `.bas` + EXPECT).

## F3 — conformance: blocco toolchain (2026-10-25)
- `runtime-abi` completato e verde (51/51, 385/385, 60/60, 4/4).
- `conformance/` **non ancora popolato**: il runner deve usare il toolchain C, ma in monorepo:
  - `gmb` risolveva la root solo via `GMBASIC.sln` → aggiunto marker
    `native/gmb_runtime_c/include/gmb_runtime.h` in `gmb_inc_resolve.c` (fix fatto) e fallback
    LLMEnv `install/../../LLMEnv` in `gmb_cli.c`;
  - il link minimo di `gmb` (`-lm -lpthread`) non copre SDL/OpenSSL del runtime → i sample
    console falliscono su `_SDL_*` (aggiunto `-lcrypto -lssl`, resta SDL);
  - `tools/gmb_c99native_run.sh` usa ancora i path del vecchio layout (LLMEnv accanto a
    GMBasic, settings) → fallisce con `llmengine_plugin.h` non trovato.
- Prossimo: unificare root/LLMEnv/link recipe in un'unica sorgente C (o settings del monorepo)
  e poi implementare `conformance/run_conformance.c` (EXPECT vs output).

## F3 — conformance (primo runner) (2026-10-25)
- Creato `conformance/run_conformance.c` (+ CMake/README): estrae `' EXPECT:` dal `.bas`,
  esegue `GMBasic/tools/gmb_c99native_run.sh`, normalizza (righe unite con `|`, spazi finali
  rimossi) e confronta. Exit 0 PASS/SKIP, 1 FAIL, 2 setup.
- PASS verificati: `bits/bitclear.bas`, `bits/bitcount.bas`, `keyword_named_var.bas`.
- Fix toolchain monorepo: root C senza `.sln` riconosciuta dal marker
  `native/gmb_runtime_c/include/gmb_runtime.h` (`gmb_inc_resolve.c`); LLMEnv cercato anche in
  `install/../../LLMEnv` (`gmb_cli.c`); `gmb_c99native_run.sh` rileva LLMEnv sibling/workspace
  (niente env) e linka `MoltenVK`.
- Nota: `speed/gaps.bas` passa col runner C99Native ufficiale ma fallisce via
  `gmb_c99native_run.sh` ("SPEED.SETDURABILITY non definita") — differenza driver/flags da
  indagare (non è un gap di StdLib).

## Fix gmb_c99native_run.sh (2026-10-25)
- Causa del "SPEED.SETDURABILITY non definita": driver del build **stantio**. Ora lo script
  ricostruisce il target `gmb_c99native_driver` (incrementale) prima di scegliere il binario.
- Rimosso l'uso di `python3` per i settings: bootstrap deterministico `-O1 -w` + `cc`
  (override `CF_OVERRIDE`). Nessun Python nel tool.
- Verifica con `conformance/run_conformance`: PASS su `bits/bitclear`, `bits/bitcount`,
  `keyword_named_var`, `speed/gaps`.
- Patch applicata anche al repo originale `GMBasic/tools/gmb_c99native_run.sh`.

## F3 — spec (2026-10-25)
- Aggiunti `spec/README.md` (contratto: registry append-only, TokenKind chiave, consumo C/Sharp)
  e `spec/VERSION`. `spec/registry` già versionato.
- `conformance/run_conformance` operativo (single-file); verifica PASS su bits/keyword/speed.

## Conformance multi-file + fix script (2026-10-25)
- `run_conformance` accetta N file (root assoluta, path risolti prima del chdir) e stampa il
  totale con exit aggregato.
- Verifica: `bits/bitclear`, `bits/bitcount`, `keyword_named_var`, `speed/gaps` → **4/4 PASS**.
- `gmb_c99native_run.sh`: driver ricostruito prima dell'uso (fix stale) e niente Python.

## Pulizia Python residuo (2026-10-25)
- `GMBasic/tools/test_mcp_e2e.sh` (client Python) archiviato in Deprecated.
- `GMBasicSharp/tests-mixed/convert_parity.sh`: copie fixture con `cp`; normalizzazione con
  `awk`+`tr` (niente Python).
- `GMBasicSharp/tests-mixed/AI.Tests/McpClientTests`: fixture MCP ora `/bin/sh` (script inline),
  niente `python3`; test `StdioCall_ReusesProcessAcrossCalls` PASS.
- Verifica: nessun `python3` nei path vivi del monorepo.

## Python residuo nei test (2026-10-25)
- `gmb_compiler_c/tests/gmb_cli_project_test.sh`: JSON con `jq` (niente Python).
- `gmb_compiler_c/tests/gmb_cli_minitests_test.sh`: `json_get`/validità/ordine-conteggi con `jq`.
- `shared/settings/config/` (stato runtime, incluso `mcp.json` con `python3`) spostato in
  `GMBasic-Deprecated/monorepo-shared-config-2026-10-25`.
- Resta solo un **path** a site-packages torch in `native/gmb_torch/build_host.sh` (link a
  libtorch, non esecuzione Python): dipendenza esterna ML da valutare in F5.

## F4 — verifica separazione + igiene repo (2026-10-25)
- `GMBasicSharp.slnx` builda **senza** `SkipNativeAiLibs` / toolchain C (0 errori); `GMBasic`
  builda con CMake (runtime/vm/db/compiler/ai) senza .NET.
- Aggiunti `.gitignore` (root, GMBasic, GMBasicSharp, shared, conformance, runtime-abi):
  bin/obj/build/work/out, `libs-runtime`, `Resources/libs`, stato (graphs/rag/graphify).
- README per progetto: `GMBasic/README.md`, `GMBasicSharp/README.md`, `shared/README.md`
  (build, dipendenze, ABI, licenza PUL/Apache).
- Confini: 0 C# in GMBasic, 0 C in GMBasicSharp; nessun `.py` nei path vivi (resta solo un
  path a site-packages torch nel build_host C, dipendenza esterna ML da decidere in F5).

## F3 — COMPLETATA (2026-10-25)
- `spec/` (registry + README + VERSION), `conformance/` (runner multi-file; PASS su bits/keyword/speed),
  `runtime-abi/` (4 elenchi + `check_abi` + `check_all.sh`, `ABI_VERSION` 1.0.0).
- Gate ABI: **51/51, 385/385, 60/60, 4/4 → ABI 1.0.0 OK**.
- Ownership/licenze: `LICENSING.md` (Apache runtime/Sharp/shared; PUL `gmbllm`; monodirezionalità).
- Criterio F3 soddisfatto; F4 verificata; restano F5/F6.

## F5 — DECISIONE (2026-10-25)
- **Nessuna reimplementazione C#**: GMBasicSharp usa le dylib C via P/Invoke. Il C resta l'unica
  sorgente di runtime/UI/DB/AI; niente duplicazione, parità garantita dal contratto ABI.
- F5 si riduce a: mantenere `runtime-abi` completo per ogni P/Invoke (4 lib verificate, altre
  elencate da coprire), licenze (Sharp Apache + dipendenza PUL per `gmbllm`).
- `gmbasic-sharp` pubblico = sorgente; l'esecuzione richiede il runtime C (repo privato/PUL).

## F5 — ABI estesa (2026-10-25)
- `runtime-abi/check_all.sh` copre **13 dylib** (db, gfx, ai_llm, ai_modelio, ai_mlx,
  finetune, llamacpp, llmtoken, torch_loader, compress, grep, pkg, moe_stream): **tutte OK**.
- Rimossi da `MlxProviders` P/Invoke morti (`gmb_mlx_opts_register`, `gmb_opt_*`,
  `llm_ai_mlx_model_lora_save`, `gmb_mlx_set_moe_swap_dir`): non esportati/rinominati.
- Aperto: `GmbEnv.Get` usa `gmb_opt_env` su `gmb_ai_mlx` (non esportato); fallback settings/env
  maschera l'errore, da rebind/export.

## Fix ABI `GmbEnv` (2026-10-25)
- `gmb_opt_env` ora è esportato da `libgmb_ai_llm` (link option `-Wl,-u,_gmb_opt_env` su Apple):
  `GmbEnv.Get` non ricade più nel fallback per EntryPoint mancante.
- `gmb_ai_llm.symbols` aggiornato (61/61); `check_all.sh` verde.
- Patch CMake da applicare anche al repo originale (stessa riga in `gmb_runtime_c/CMakeLists.txt`).

## Fix GmbEnv (2026-10-25)
- `GmbEnv` ora usa `gmb_ai_llm` (dove `gmb_opt_env` è esportato con `-Wl,-u,_gmb_opt_env`),
  non `gmb_ai_mlx`. Il fallback non maschera più un EntryPointNotFound.
- Sharp builda 0 errori; `check_all` ABI OK (ai_llm 61/61).

## Repo locali creati (2026-10-25)
- `GMBasic` → commit `d6ed1fd` (C baseline; include plugin/lib dylib prebuilt da ripulire prima della pubblicazione).
- `GMBasicSharp` → commit `bb05cb3` (C# baseline).
- `shared` → commit `875f649` (samples/minitests/spec/conformance/runtime-abi; 212 MB).
- Workspace `GMBasicMonoRepo` → commit `7f29463` con submodule gitlink e `.gitmodules` (URL relativi
  placeholder: da sostituire con i remote definitivi). Submodule **non inizializzati** (solo gitlink).
- `BASE_COMMIT` riportato a `66ab81ddc` (ultimo commit importato): **pending sync `66ab..50a6552df`**
  (~20 commit: DYNAMIC VM/C, model-quantize, READ write-through, LIBRARY runner).
