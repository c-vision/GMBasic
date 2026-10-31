# GMBasic (C)

Progetto **solo C** del monorepo: compilatore nativo, VM/C, runtime condiviso, codec/LLM nativi.
Nessun file `.cs` e nessuna dipendenza .NET per la build.

## Build

```
cd GMBasic/native/gmb_runtime_c
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target gmb_runtime

cd ../gmb_vm_c
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGMB_HAS_COMMERCIAL_DB=ON -DGMB_VM_LINK_COMPILER=ON
cmake --build build --target gmb_vm_host

cd ../gmb_compiler_c
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target gmb_cli
```

Dipendenze locali (non versionate): `Resources/libs`, `libs-runtime`, `gmbllm`/`LLMEnv`
sorgenti accanto al repo. Tool di build: `tools/` (solo shell; niente Python).

## Verifica

- `conformance/run_conformance` (workspace) sul corpus `shared/minitests`.
- `runtime-abi/check_abi` per l'ABI P/Invoke.
- `tools/sh/check_file_size.sh --quiet` (gate dimensioni; oggi rosso per file monolitici storici).

## Licenza

Runtime C: Apache-2.0. `gmbllm/` (produzione/quantizzazione pesi): **PUL** — vedere
`gmbllm/LICENSE`. Il repo `gmbasic-c` resta privato finché `gmbllm` non è separato.
