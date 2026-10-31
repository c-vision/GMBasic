# GMBasic (C)

The **C-only** project of the monorepo: native compiler, VM/C, shared runtime, native codecs/LLM.
No `.cs` file and no .NET dependency in the build.

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

Local dependencies (not versioned): `Resources/libs`, `libs-runtime`, `gmbllm`/`LLMEnv` sources
next to the repo. Build tools: `tools/` (shell only; no Python).

## Verification

- `conformance/run_conformance` (workspace) over the `shared/minitests` corpus.
- `runtime-abi/check_abi` for the P/Invoke ABI.
- `tools/sh/check_file_size.sh --quiet` (size gate; currently red for historical monolithic files).

## License

C runtime: Apache-2.0. `gmbllm/` (weight production/quantization): **PUL** — see
`gmbllm/LICENSE`. The `gmbasic-c` repo stays private until `gmbllm` is split out.
