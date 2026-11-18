# GMBasic 1.0.0 — release notes

**Data:** 2026-11-18 · **RID:** `osx-arm64` (Apple Silicon) · **Licenza:** vedi `LICENSING.md`

Prima release **compilata** di GMBasic: un solo archivio, nessun sorgente da compilare
all'installazione, nessun requisito .NET.

| pezzo | valore |
|---|---|
| archivio | `gmbasic-1.0.0-osx-arm64.zip` |
| dimensione | 60 MB (1344 file, scompattato ~155 MB) |
| integrità | `gmbasic-1.0.0-osx-arm64.zip.sha256` + `release-manifest.json` (sha256 di **ogni** file) |
| verifica | `gmb doctor --verify` → 924 OK, 0 manomessi, 0 mancanti |

## Installazione

```sh
unzip gmbasic-1.0.0-osx-arm64.zip
cd gmbasic-1.0.0-osx-arm64
export PATH="$PWD/bin:$PATH"

gmb doctor              # ambiente: 18/18 ok
gmb doctor --verify     # confronta OGNI file col manifest
gmb eval "PRINT 40+2"   # 42
```

Per un programma:

```sh
gmb hello.bas                      # build + run (scorciatoia)
gmb build --project <dir> --entry <file.bas>
```

## Cosa c'è dentro

- **`bin/`** — 14 eseguibili: `gmb` (compilatore/CLI), `gmbnative` (C99Native), `gmbchunk`
  (bytecode A1/GMBC), `gmb_vm_host` (VM/C), `gmbadmin` (pkg/libs/brain/doctor), `gmbrepl`
  (REPL C), `gmbnotebook`, `gmblsp`, `gmblibs`, `gmbfetch`, `gmbbrains`, `gmbfixtures`,
  `gmbtheme`, `gmbimgdiff`.
- **`GMBasic/native/`** — header e librerie statiche/dinamiche del runtime condiviso,
  del compilatore, del DB (ISAM/DBF/SPEED/LEDGER), di chart/doc/torch/agent.
- **`shared/`** — `include/` (le `.inc` FFI), `Controls/`, `settings/` (manifest di download).
- **`LLMEnv/`** — motore LLM locale (**Apache-2.0**): header dei codec/engine/desc/token,
  descrittori `.gmbarch` in `ABI/`, `LICENSE` e `NOTICE`.

## Requisiti

- **macOS su Apple Silicon** (questo RID).
- **`cc`** = Xcode Command Line Tools: serve a `gmb build` per compilare il C emesso.
  `gmb doctor` lo verifica.
- **Rete** solo per i download on demand (`gmb libs ensure`, `gmb fixtures ensure`,
  `gmb brain install`) — verifica **sha256** prima di ogni estrazione.
- **Nessun .NET**: i compilatori e i runtime sono C.

## Cosa NON è in questa release

- **`gmbcs` / `--engine csharp`** (compilatore C#): non è incluso nell'artefatto. Il motore
  della release è il **ramo C** (`gmb`, C99Native, VM/C). `gmb doctor` lo riporta come
  **facoltativo**, quindi 18/18 resta verde.
- **RID `osx-x64`, `linux-x64`, `linux-arm64`, `win-*`**: l'artefatto è per `osx-arm64`.
  La toolchain di cross-compilazione (`zig`) e i manifest esistono, ma questa release pubblica
  **un solo RID**.
- **Sample e MiniTest**: il corpus (`shared/minitests`, `shared/samples`) sta nel repository,
  non nell'archivio; i feed di mercato si scaricano con `gmb fixtures ensure`.

## Verifiche eseguite sull'artefatto

Eseguite **sullo zip scompattato in una cartella temporanea**, con `LLMDESC_ABI_DIR`/`LLM_ROOT`
rimossi dall'ambiente (nessun percorso del checkout di sviluppo usato):

| verifica | esito |
|---|---|
| `sha256sum -c *.sha256` | OK |
| `gmb doctor` | **18/18 ok** |
| `gmb doctor --verify` | **924 OK**, 0 manomessi, 0 mancanti |
| `gmb eval "PRINT 40+2"` | `42` |
| `gmb Hello.bas` (programma testuale) | compila, linka, esegue (`hello 1.0`, `42`) |
| `IMPORT AI` + `gmb build` | 2 sorgenti, **0 errori** (header LLMEnv completi) |
| `otool -l bin/*` | **0** path assoluti (rpath `@loader_path`) |

## Stato del corpus di test su questa storia

Sweep completo della settimana sulla gamba **nativa** (C99Native, il riferimento):

```
C99Native MiniTests: PASS=2128 FAIL=0 SKIP=264
```

Gli SKIP sono tutti dichiarati: `gen_AI_*` che dipendono dallo stato della macchina (222),
`TAG: heavy` con modello reale (30), `TAG: interactive` (12).

Cross-leg sulle **tre gambe gestite** (IL, PCodeVM, VM-C), un processo per dominio:
**122 domini su 125 verdi**. I 3 restanti richiedono dipendenze d'ambiente
(`llm/` ×4 serve un modello, `train/` il registro di training, `tray/` il pump sul main thread).

## Licenze

- Runtime, compilatori, tool: **PUL-1.1** (vedi `LICENSING.md`).
- `LLMEnv/` (motore LLM): **Apache-2.0**, con `LICENSE` e `NOTICE` inclusi.
- Componenti di terze parti: vedi `LICENSING.md` e i singoli `NOTICE`.
