# `shared/settings/` — shared configurations of the COMPILER

Single point of the configurations that the two worlds (C# and C) read in the same way. They are
**versioned**: no absolute path, no secret, identical on every machine.

| file | who reads it | what it is for |
|---|---|---|
| `system.settings.json` | compiler + runtime (`pr_load`, `GmbiPaths`) | global defaults: `paths`, `consoleConfiguration`, `engine`, `compiler`, `importFolders`, `defaultNamespaces` |
| `fixtures-manifest.json` | `gmb_fixtures_ensure*`, compiler, REPL, CLI | large feeds downloaded on demand (release `fixtures-v1`) |
| `brains-manifest.json` | `gmb_brains`, `gmbbrains`, bundle | distributed brains: version, `rag`/`graph` shapes, repo/tag/asset, sha256 |
| `libs-manifest.json` + `libs-index-<rid>.tsv` | `LibsResolver` (C#), `gmb_libs_ensure` (C) | bundle of the native libraries per RID |
| `ml-manifest.json` | `ModelDownload` / ML components | models and components (torch, mlx) |
| `chromium-manifest.json` | `browser` plugin | Chromium builds per RID |
| `obscura-manifest.json` | CUA agents | headless browser |
| **`providers.json`** | `AI.PROVIDERS()` in **both** legs | provider **seed**: three code defaults (`fixture-embedding`, `fixture-completion`, `keyword-reranker`), `roleOrder`, `activeIndex`, `schemes` (hf/huggingface/ollama/openai/anthropic) |

## Rule

- **Default** → here (or in the code). **User state** → `<configDir>` (local, `.gitignore`).
- The runtime reads the **local** file first and, if it is missing, this **seed**. The bundle copies the
  necessary seeds next to the executable (`<exe>/config/providers.json`), so that a distributed
  program works without the checkout:
  - `providers.json` **always** (1.4 KB, needed by `AI.PROVIDERS()` and by the provider operations);
  - the other files only if the program declares them: `"config": ["mcp.json"]` in its
    `app.settings.json` (source: `<programma>/config/<nome>` if it exists, otherwise this seed).
- `providers.json` is the only *runtime configuration* seed in this folder: it is here because
  `shared/minitests/ai/gen_AI_Providers.bas` has its content as oracle and the two legs
  must answer identically even on a clean checkout (before, C# did a
  direct `File.ReadAllText` and threw `FileNotFoundException`).

Detail of the `<configDir>` side: [GMBasic/config/README.md](../../GMBasic/config/README.md).
