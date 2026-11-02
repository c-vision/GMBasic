# gmbadmin — CLI amministrativo di GMBASIC (C)

Backend **C** dei sottocomandi amministrativi di `gmb` (`pkg`, `fixtures`, `setup`, `libs`,
`brain`, `grep`, `convert/convfmt`, `keygen`). Il front per l'utente resta **`gmb`**: quando il
primo argomento è un comando admin, `gmb` esegue `gmbadmin` con lo **stesso argv**.

Regola: tutto ciò che esiste nel mondo C# deve esistere in C; il C# è un wrapper P/Invoke.
Restano da portare (`register`, `admin`, `daemon`, `rag`, `graph`, `hf/mlx`,
`setup TORCH|BROWSER|OCR`, `libs publish`, firma Ed25519 del pack): rispondono con un errore
**esplicito** (porting F9c in `TODO.md` §6).

## Build

```bash
cmake -S GMBasic/native/gmbadmin -B GMBasic/native/gmbadmin/build
cmake --build GMBasic/native/gmbadmin/build --target gmbadmin
```

L'eseguibile nasce in `GMBasic/native/gmbadmin/build/gmbadmin`; `gmb` lo trova lì (checkout)
o nel `PATH`. Linka il runtime C condiviso (`gmb_runtime`, `gmb_pkg`, `gmb_doc`).

## Comandi

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

gmb fixtures [list|ensure|pull <nome>]
gmb setup status · gmb libs status
gmb brain [list|info <nome>|use <nome>|detach <nome>]
gmb grep <pattern> [path] [-i] [-n] [--include G] [--depth N]
gmb convert <file> [-o out.(md|html|txt)] · gmb convfmt
gmb pack <dir> [--out f.gmbpkg] · gmb publish <f.gmbpkg> [--registry u] [--token t]
gmb login <token> [--registry u] · gmb logout
gmb libs ensure · gmb libs check [--index file]
gmb keygen
```

La REPL C (`gmb_repl`) usa lo stesso backend per `ADD/REMOVE/BUMP/TRUST/CACHE/RESTORE/UPDATE/
ROLLBACK` (`rpl_admin_cmd("pkg", …)`): una sola implementazione.
