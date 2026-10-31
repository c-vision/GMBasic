# Licensing

## One license for the whole repository

**GMBasic is distributed under the PERSONAL USE LICENSE (PUL) v1.1** — see [`LICENSE`](LICENSE)
and [`NOTICE`](NOTICE). There is **no edition split** (no "commercial" vs "community" build): the
repository contains **everything**, and everything is under the same license.

What that means in practice:

| | |
|---|---|
| **No activation key**, no online check, no trial, no development-mode switch | the software runs as-is |
| **Personal / educational use** | granted by the license |
| **Commercial use** | **not granted**. Only separate terms, agreed with the author, can allow it |
| **Public or private forks** | allowed (PUL §Expressions), but every copy / fork / derivative **must keep the copyright notices, the license and the repository URL** |
| **AI training on this code** | prohibited (see `LICENSE`) |

> **PUL is not an OSI open-source license.** The source is public and readable, the license allows
> personal use and forks, but it is **not** "open source" in the OSI sense and it forbids commercial
> use. GitHub reports it as *License: other*.

## Why the old split is gone

Until 2026-10-31 the tree carried an `Edition` axis: a `Commercial`/`Community` MSBuild property, a
`CommercialEditionRequiredException` for the database drivers, a `GMB_HAS_COMMERCIAL_DB` option, and
two sample roots (`shared/samples/Commercial`, `shared/samples/Community`). That axis existed to
sell a separate edition. It has been **removed**: one build, one set of samples, one license.

The folder [`GMBasic/gmbllm/`](GMBasic/gmbllm/README.md) (model compiler: quantization encode,
training, converter) stays **separate for technical reasons** — it is the part that produces or
alters model weights — but it is **not** a different license: it is PUL like the rest.

## What is not ours (third-party)

Third-party material keeps **its own license**, and is listed in [`NOTICE`](NOTICE) and
[`GMBasicIDE/THIRD_PARTY_NOTICES.md`](GMBasicIDE/THIRD_PARTY_NOTICES.md):

- **SQLite** (`GMBasic/native/gmb_sql_ext/sqlite346/`) — public domain;
- **stb_truetype** (`gmb_runtime_c/include/stb_truetype.h`) — MIT / public domain;
- **pdf_oxide** C header (`GMBasic/native/deps/`) — generated header of an external project;
- the **fonts** in `Resources/` — origin and license of each file in
  [`Resources/FONTS.md`](Resources/FONTS.md): only **Cascadia Code** (`font.ttf`, SIL OFL 1.1) is
  redistributable; **SF Mono** (Apple) and **C64 Pro Mono** (style64) are **not** and must be
  removed before publishing;
- the **skills** in `shared/skills/` — [`shared/skills/ATTRIBUTION.md`](shared/skills/ATTRIBUTION.md)
  lists, per skill, the license its own file declares: only the ones with a declared permissive
  license are redistributable, the others are **not**;
- the **FFI packages** under `packages/` build third-party libraries from source at install time
  (their sources are **not** in this repository).

## External repositories consumed by this one

| repo | visibility | license | role |
|---|---|---|---|
| [`c-vision/LLMEnv`](https://github.com/c-vision/LLMEnv) | public | **Apache-2.0** | inference engine (MLX, llama.cpp/GGUF, codecs, tokenizers, chat templates) |
| [`c-vision/GMBasicModules`](https://github.com/c-vision/GMBasicModules) | public | release assets | prebuilt native libraries, market data feeds, ML/Chromium/OCR components |
| [`c-vision/GMBasicBrains`](https://github.com/c-vision/GMBasicBrains) | **private** | release assets | brains (RAG + knowledge graph); a token is required |

The dependency direction is one-way: **this repo → LLMEnv**. The Apache code of the engine stays
Apache even when linked from PUL code; the reverse (Apache code depending on PUL code) is forbidden,
and `LLMEnv/tools/gmb_license_gate.py` must exit 0 after any change on the LLM boundary.
