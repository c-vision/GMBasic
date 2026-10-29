# tools/audit — `gmb_audit`

Audit della copertura builtin per backend: estrae la superficie canonica (enum
`BuiltInFunction` + StdLib) e i `case`/dispatch di ciascun backend, e produce un manifest JSON
riconciliabile. Strumento di sola LETTURA: non modifica alcun backend.

## Provenienza

Sorgente ripreso **verbatim** da `migration/tools-unclassified/audit/` (eliminata il 2026-10-29
insieme al resto di `tools-unclassified`), dove era stato patchato per il monorepo: chiave di
deduplica `(namespace, nome)` invece del solo nome, riconoscimento del dispatch per nome e per
`case SysCall` (senza, 12 funzioni CONTROLS/WINDOWS risultavano falsi gap), SPEED/MOUSE.
`Program.cs` è identico al file eliminato (verificato con `diff`).

## Stato dei riferimenti di progetto — DA RIPUNTARE PRIMA DELL'USO

`audit.csproj` e il resolver `ResolveRoot()` di `Program.cs` sono ancora quelli del layout
pre-monorepo:

- le `ProjectReference` puntano a `../../common/...` (e una a `../...csproj`, che non esiste):
  nel monorepo i progetti C# stanno in `GMBasicSharp/common/...`;
- `ResolveRoot()` cerca `GMBASIC.sln`, che nel monorepo non c'è (c'è
  `GMBasicSharp/GMBasicSharp.slnx`), e ricade sulla risalita da `bin/`;
- i path interni (`common/Compiler/BuiltInFunction.cs`, `native/gmb_c99native/src/...`,
  `native/gmb_vm_c/gmb_vm_core.c`) sono relativi alla radice del progetto C# / del progetto C,
  non alla radice del monorepo;
- l'output (`--out-dir` di default `Root + docs/comparison/manifests`, e il report
  `Root + docs/comparison/audit-backends.md`) deve finire nella **`docs/` della radice del
  monorepo** (unica cartella di documentazione, indice `docs/README.md`): finché `ResolveRoot()`
  restituisce la radice del progetto C#, usare `--out-dir <monorepo>/docs/comparison/manifests`.

Stessa situazione di `tools/registry-export/registry-export.csproj` (riferimenti legacy). Il
progetto **non è stato compilato né eseguito** dopo lo spostamento: nessuna verifica dichiarata.

## Uso documentato (forma originale)

```
dotnet run --project tools/audit -- --all
dotnet run --project tools/audit -- --pcodevm [--json out.json]
dotnet run --project tools/audit -- --list-backends
```

`test_retired_flags.sh` verifica che i flag ritirati (`--c99`, `--registry-c99`) escano con 2 e
non producano file. Anche questo script assume il layout pre-monorepo.

## Perché è tenuto

AGENTS.md §12 lo indica come infrastruttura di audit canonica: ogni nuovo comando/funzione deve
essere implementato in modo che l'audit lo conti. La logica di riconoscimento del dispatch
(per nome, per `HashSet<BuiltInFunction>{...}`, per `case SysCall.FORM_*`, per `vm_form_*`) è
conoscenza da preservare: `docs/Sync-GMBasic-Monorepo.md` nota che va replicata in qualunque
riscrittura C dell'audit, altrimenti si riproducono i 12 falsi gap.
