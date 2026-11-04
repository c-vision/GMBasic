# gmbadmin — CLI amministrativo di GMBASIC (C)

Backend **C** dei sottocomandi amministrativi di `gmb`. Il front per l'utente resta **`gmb`**:
quando il primo argomento è un comando admin, `gmb` esegue `gmbadmin` con lo **stesso argv**
(`execv`, nessuna traduzione).

Regola del progetto: tutto ciò che esiste nel mondo C# deve esistere in C; il C# è un wrapper
P/Invoke. Questo tool è l'unico posto dove vive la logica amministrativa.

## Build

```bash
cmake -S GMBasic/native/gmbadmin -B GMBasic/native/gmbadmin/build
cmake --build GMBasic/native/gmbadmin/build --target gmbadmin
```

Nasce in `GMBasic/native/gmbadmin/build/gmbadmin`; `gmb` lo trova lì (checkout) o nel `PATH`.
Linka il runtime C condiviso (`gmb_runtime`, `gmb_pkg`, `gmb_doc`, `gmb_boot`) e — per
costruzione — usa le librerie **appena compilate** (`-Wl,-rpath` sul build dir prima dell'rpath
della cache `~/.gmbasic/libs/<rid>/`, che può essere più vecchia).

## Comandi

### Pacchetti
```bash
gmb pkg init <dir> --name X --version V [--type T]
gmb pkg add <nome[@ver]> [--dev] [--dir d]
gmb pkg remove <nome> [--dir d]
gmb pkg bump <major|minor|patch|X.Y.Z> [--dir d]
gmb pkg list [--cache d] · gmb pkg info <nome> [--cache d]
gmb pkg install <nome[@ver]> [--cache d] [--registry url]
gmb pkg uninstall <nome> [--cache d]
gmb pkg restore [dir] [--locked] [--cache d] [--registry url]
gmb pkg update [dir] [--check] [--cache d]
gmb pkg rollback [dir] [--list] [--to prefisso]
gmb pkg trust [list|add|remove|fingerprint] …
gmb pkg cache [list|verify|gc|dedup|clean [--force]]   # verify = confronto col lock
gmb pkg verify <file.gmbpkg>
gmb pack --dry-run <dir>        # elenca i file che entrerebbero (nessuna scrittura)
```

### Pubblicazione
```bash
gmb pack <dir> [--out f.gmbpkg] [--key-file priv|--key segreto]
gmb publish <f.gmbpkg> [--registry u] [--token t]
gmb publish --repo <git-url|path> <nome@tag> [--registry u] [--token t]   # verifica repo + register
gmb login <token> [--registry u] · gmb logout
gmb register <nome> [--email e] [--registry u]
gmb admin reserve|assign|revoke|password|user <create|setkey|status|list> …
gmb keygen [dir] [--pem]
```

- `pack` usa l'elenco file del manifest (exports/binaries/ffi/samples, filtro `files`/`ignore`)
  e con `--key-file`/`--key` produce la **firma Ed25519 detached** (`.gmbpkg.sig` + `.gmbpkg.pub`
  dentro lo zip), identica a `PackageBuilder.Pack` del C#.
- `pkg verify` verifica la firma: `firma OK (ed25519)`, `non firmato`, oppure errore esplicito.
- `login`/`register` scrivono token e registry in `~/.gmbasic/config.json` (chmod 600).

### Librerie native (GMBasicModules)
```bash
gmb install [--runtime-only|--dev|--components|--all]   # alias di `gmb libs ensure`
gmb install --verify                                     # alias di `gmb libs check`
gmb libs status · gmb libs ensure|install [--runtime-only|--dev|--components|--all]
gmb libs check [--index file] · gmb libs publish <support|engine|core> <rid> [--yes]
gmb libs prune [--dry-run]      # elimina dalla cache i file non presenti nel manifest
gmb libs index [--rid R]        # rigenera l'indice delle librerie (script di release)
```

### Setup componenti on-demand
```bash
gmb setup status
gmb setup <LIBS|OCR|TORCH|BROWSER|OBSCURA|ALL> [--force] [--yes] [--torch-local dir]
gmb setup rm <BROWSER|OBSCURA|TORCH|LIBS|OCR|ALL> [--force]
```
`setup TORCH` cerca **prima** un torch locale, in quest'ordine: `--torch-local <dir>` →
option `paths.torchLocal` (system/app settings) → **auto-detect** dei venv (`~/ai/<env>`,
`.venv`, `venv`, conda envs, Python di sistema/Homebrew). Una cartella è valida se contiene
`lib/libtorch*`. I file dichiarati `source=local` vengono copiati da lì; solo il resto va in
rete (url/archivio, sha256 verificato).
Manifest in `shared/settings/` (`chromium-manifest.json`, `obscura-manifest.json`,
`ml-manifest.json`), download via `gmbfetch` (ripresa + sha256 + rename atomico), estrazione
zip/`tar`. `OCR` fa parte del bundle LIBS (tesseract).

### RAG e grafo
```bash
gmb rag build|update <wikiDir> [--rag d] [--name n] [--collection c]
                              [--chunk N] [--overlap N] [--dry-run] [--prune|--no-prune]
gmb rag status [--rag d] [--name n]
gmb rag query "<q>" [--rag d] [--name n] [--topk N] [--collection c]
gmb graph build|update <wikiDir> [--graphs d] [--name n] [--dry-run]
gmb graph status [--graphs d]
```
- `rag build/update` indicizza i documenti del wiki con `gmb_rag_index_source` (chunking
  condiviso col C#: default 2048/256), `--prune` rimuove dal DB i file spariti
  (`gmb_rag_list_sources` + `gmb_rag_delete_source`). Senza provider di embedding risponde con
  l'errore esplicito del runtime (mai un fallback silenzioso).
- `graph build` usa `gmb_graph_build` (secondbrain: LINK/TAG_SHARE/CONTENT_MENTION…), scrive
  `<graphs>/<nome>.json`.

### Daemon (macOS/launchd)
```bash
gmb daemon install <name> <file.bas> [--port N] [--no-start]
gmb daemon start|stop|restart|status <name> · logs <name> [--follow]
gmb daemon list · uninstall <name> [--purge]
```
Registry `~/.gmbasic/daemons/<name>/` (`manifest.json` + `logs/`), plist
`~/Library/LaunchAgents/it.gmb.<name>.plist`, label `it.gmb.<name>`. Il job lancia
**`gmbrepl <file.bas>`** (REPL/runner C): non c'è più alcuna dipendenza da `dotnet REPL.dll`.
La REPL risolve `gmb` e `gmb_vm_host` anche da una CWD arbitraria (PATH + dir dell'eseguibile),
così il job launchd funziona con qualunque `WorkingDirectory`.

### Modelli HF/MLX
```bash
gmb hf  <repo> [--file F|pattern] [--dest D] [--token T] [--quiet]
gmb mlx <repo> [--dest D] [--token T] [--quiet]      # intero repo
```
Elenco file da `https://huggingface.co/api/models/<repo>`, download con `gmbfetch`
(ripresa/stall-retry), token da `--token` o `~/.cache/huggingface/token`.

### Second Brain (toolchain completo, C)
```bash
gmb brain list|info <nome>|use <nome>|detach <nome>|forget <nome>|resolve <nome>
gmb brain install <nome> [<sorgente>] · uninstall <nome>
gmb brain stats <nome> [--json]
gmb brain verify <nome> [--json] · verify <nome> --golden <file.jsonl> [--topk N] [--min-p1 X]
gmb brain create <nome> --from <dir> [--mode rag|graph|both] [--register] [--activate] [--dry-run]
gmb brain register <nome> [--mode …] [--source …] [--activate]
gmb brain buildvault [--out <dir>]
gmb brain optimize <nome> [--dry-run] [--reindex-fts]
gmb brain rename <nome> <nuovo> [--dry-run]
gmb brain backup|export <nome> [--out file.zip]
gmb brain sign <nome> <cartella> [--out file.zip]
gmb brain dedupe <nome> [--apply] [--min-chars N] [--dry-run]
gmb brain extract <nome> --out <nuovo> [--source S] [--node N] [--hops N] [--dry-run]
gmb brain merge <a> <b> [<c>…] --out <nuovo> [--on-conflict skip|replace] [--dry-run]
gmb brain convert <nome> --to rag|graph [--out nuovo] [--in-place] [--dry-run] [--min-edges N]
gmb brain diff <a> <b> [--json]
gmb brain doctor <nome>
gmb graph export-obsidian <nome> [--graphs d] [--out dir]
gmb graph watch <wikiDir> [--graphs d] [--name n] [--interval S] [--once]
```
Implementazione: `gmb_runtime_c/src/gmb_brain_registry.c` (registry `config/brains.json` +
manifest `brain.manifest.json`), `gmb_brain_tools.c` (stats/verify/create/register/buildvault/
optimize/rename/export/sign/backup), `gmb_brain_tools2.c` (dedupe/extract/merge/convert/
verify-golden/diff/doctor + export grafo). Il C# non ha più nessuno di questi comandi.

### Altro
```bash
gmb fixtures [list|ensure|pull <nome>]
gmb brain [list|info <nome>|use <nome[,nome]>|detach <nome>]
gmb brain install <nome> [<sorgente>] · uninstall <nome> · resolve <nome>
gmb brain login --github [<token>|--token T] · logout     # token per GMBasicBrains (0600)
gmb grep <pattern> [path] [-i] [-n] [--include G] [--depth N]
gmb convert <file> [-o out.(md|html|txt)] · gmb convfmt
gmb web2md <url> [-o out.md] · gmb batch <dir> [-o outdir] [-r]
gmb keygen · gmb pkg verify
```

## Residuo (F9c)

**Nessun comando esiste solo in C#**: il vecchio CLI `GMBasicSharp/Cli` è stato rimosso.

- `brain buildvault` usa il catalogo firme `shared/spec/registry/stdlib-signatures.json`
  (generato da `tools/registry-export --signatures`, incluso in `--all`/`--check`): ogni riga
  ha **firma completa** (parametri con tipo, `?` opzionale, `@` byref, `[]` array) e ritorno.
- `stats.json` del grafo è **completo** (degreeTop/betweennessTop/godNodes/surprisingConnections/
  suggestedQuestions/edgeProvenance/folders), stesse chiavi di `GraphExporter.ToStatsJson`.
- `publish --repo` fa l'intero flusso: manifest → git → compilazione+self-test → register →
  pubblicazione dell'artefatto `.gmbpkg` → **materialize** degli asset binari.

## Alias storici (contratto del vecchio CLI)

`gmb init|add|remove|bump|restore|update|trust|rollback|cache …` sono accettati come alias di
`gmb pkg …`; `gmb install …` è alias di `gmb libs ensure`; `gmb <file.bas>` esegue il programma
con il compilatore C (equivalente a `gmb run --project <cartella> --entry <file>`).

## Download (dove va cosa)

| cosa | comando | sorgente | destinazione |
|---|---|---|---|
| librerie native | `gmb libs ensure` / `gmb install` | indice piatto `…/GMBasicModules/releases/download/libs-index/index-<rid>.txt` (sha256 per file) | `~/.gmbasic/libs/<rid>/` |
| brains | `gmb brain install <nome>` | `shared/settings/brains-manifest.json` → asset del repo **privato** `c-vision/GMBasicBrains` (GitHub API + token) | `~/.gmbasic/brains/<nome>/<ver>/` |
| fixtures | `gmb fixtures ensure` | release `fixtures-v1` | `shared/samples/Fixtures/<nome>/` |
| componenti | `gmb setup TORCH\|BROWSER\|OBSCURA` | manifest in `shared/settings/` | `~/.gmbasic/libs/<comp>/<rid>/` |
| modelli HF/MLX | `gmb hf` / `gmb mlx` | HF API | `--dest` o `samples/ai/models/<hf\|mlx>/` |

## Verifica

```bash
tools/sh/test_c_selfchecks.sh    # selfcheck UNITARI in C (brain registry/toolchain, pack firmato)
tools/sh/brain_c_smoke.sh        # 30 check end-to-end del CLI brain (isolato)
```

I selfcheck C (`GMBasic/native/gmb_runtime_c/tests/`) girano su una cartella temporanea via
`gmb_paths_test_override` (nessuna scrittura nella config dell'utente, nessuna env):

| test | copre |
|---|---|
| `gmb_brain_registry_selfcheck` | registry `brains.json`, risoluzione rag/graph, use/detach, manifest+corruzione |
| `gmb_brain_tools_selfcheck` | stats/verify/dedupe/extract/merge/convert(+stats completo)/diff/doctor/export/sign |
| `gmb_pkg_pack_signed_selfcheck` | firma Ed25519 (.sig/.pub), verify con la chiave del pacchetto, manomissione, HMAC |

## Runtime AI (split 2026-11-02)

Il runtime C (`libgmb_runtime.a`) **non linka LLMEnv**. Le parti AI — ASR, tokenizer LLM,
model I/O, TRAIN DISTILL e i sorgenti MoE di LLMEnv — vivono nella dylib
`libgmb_runtime_ai`, caricata su richiesta dal gateway `gmb_ai_gateway.c`.
Senza dylib i comandi AI falliscono con un errore **esplicito** («runtime AI non disponibile»),
non con un no-op. `gmbllm` è a sua volta diviso in `gmbllm_core` (consumato dal runtime) e
`gmbllm_train` (training/MLX, solo tool TRAIN).

**Bundle di distribuzione (`--libs`, `--plugins`, `--skills`, `--brains`).** Crea
sottocartelle nella destinazione: `<out>/libs/<rid>/` (chiusura delle dipendenze; oppure
`=all`, un gruppo `runtime|dev|ffi|llmmotor|torch`, un componente `ml|chromium|obscura|ocr`,
un elenco di nomi), `<out>/plugins/<nome>/`, `<out>/skills/<nome>/`,
`<out>/brains/<nome>/<ver>/`. `libgmb_runtime_ai` è sempre in `libs/<rid>`. L'rpath di
load-time è `@loader_path/libs/<rid>` + `@loader_path` (`$ORIGIN` su ELF): le copie vincono
sulla cache. `gmbcs` accetta gli stessi flag (dipendenze managed come copie reali).
Il runtime cerca **prima** accanto al programma (gateway AI, FFI, plugin, skill, componenti)
e solo dopo nella cache. Alias storici di `--libs`: `-alllib`, `--alllibs`, `--full`.
Senza il flag resta il percorso di iterazione (rpath verso la cache / symlink).

**Dove sta la dylib** (ordine di ricerca del gateway):
1. `GMBasic/build/libs/<rid>/` nel checkout (output del build);
2. **accanto all'eseguibile** — `gmb build`/`gmb run` ce la **copiano** a ogni link (come il
   compilatore C# copia la closure accanto al programma: un eseguibile distribuito non deve
   dipendere dal checkout);
3. `~/.gmbasic/libs/<rid>/` (solo se ve la mette un deploy esplicito: `gmb libs prune` toglie
   dalla cache i file non presenti nel manifest, quindi NON è il posto canonico).

Su Windows lo split non esiste (niente `dynamic_lookup`): le sorgenti AI restano nel core e la
dylib non si costruisce. Su Linux i consumer esportano i simboli del core
(`-Wl,--export-dynamic`) perché la dylib li risolve a load-time.

## Sorgente unica

La REPL C usa lo stesso backend per `ADD/REMOVE/BUMP/TRUST/CACHE/RESTORE/UPDATE/ROLLBACK`
(`rpl_admin_cmd("pkg", …)`): un'unica implementazione, nessuna copia.
