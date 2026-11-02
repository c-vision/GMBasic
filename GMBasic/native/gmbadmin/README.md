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
gmb pkg cache [list|gc|dedup|clean [--force]]
gmb pkg verify <file.gmbpkg>
```

### Pubblicazione
```bash
gmb pack <dir> [--out f.gmbpkg] [--key-file priv|--key segreto]
gmb publish <f.gmbpkg> [--registry u] [--token t]
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

### Librerie native
```bash
gmb libs status · gmb libs ensure [--runtime-only|--dev|--components|--all]
gmb libs check [--index file] · gmb libs publish <support|engine|core> <rid> [--yes]
```

### Setup componenti on-demand
```bash
gmb setup status
gmb setup <LIBS|OCR|TORCH|BROWSER|OBSCURA|ALL> [--force] [--yes] [--torch-local dir]
```
Manifest in `shared/settings/` (`chromium-manifest.json`, `obscura-manifest.json`,
`ml-manifest.json`), download via `gmb_fetch` (ripresa + sha256 + rename atomico), estrazione
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
**`gmb_repl <file.bas>`** (REPL/runner C): non c'è più alcuna dipendenza da `dotnet REPL.dll`.
La REPL risolve `gmb` e `gmb_vm_host` anche da una CWD arbitraria (PATH + dir dell'eseguibile),
così il job launchd funziona con qualunque `WorkingDirectory`.

### Modelli HF/MLX
```bash
gmb hf  <repo> [--file F|pattern] [--dest D] [--token T] [--quiet]
gmb mlx <repo> [--dest D] [--token T] [--quiet]      # intero repo
```
Elenco file da `https://huggingface.co/api/models/<repo>`, download con `gmb_fetch`
(ripresa/stall-retry), token da `--token` o `~/.cache/huggingface/token`.

### Altro
```bash
gmb fixtures [list|ensure|pull <nome>]
gmb brain [list|info <nome>|use <nome[,nome]>|detach <nome>]
gmb grep <pattern> [path] [-i] [-n] [--include G] [--depth N]
gmb convert <file> [-o out.(md|html|txt)] · gmb convfmt
gmb keygen · gmb pkg verify
```

## Residuo (F9c)

`graph export-obsidian`, `graph watch|unwatch`, `web2md`, `batch`: rispondono con un errore
**esplicito** («non ancora in C»). Nessuna nuova logica C# è stata aggiunta: il porting è
tracciato in `TODO.md` §6.

## Sorgente unica

La REPL C usa lo stesso backend per `ADD/REMOVE/BUMP/TRUST/CACHE/RESTORE/UPDATE/ROLLBACK`
(`rpl_admin_cmd("pkg", …)`): un'unica implementazione, nessuna copia.
