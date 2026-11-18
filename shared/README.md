# shared

Material **shared by the two worlds** (`GMBasic/` in C, `GMBasicSharp/` in C#), versioned.

| folder | contents |
|---|---|
| `samples/` | sample BASIC programs (`Community/`, `Native/`) + `Fixtures/` |
| `samples/Fixtures/` | **fixtures**: the 4 market feeds (`markets`, `option_chains`, `rates`, `darkpool`) + the small fixtures. The feeds are downloaded with `gmb fixtures ensure` **always here** — no symlinks, no external disk (decision 2026-10-31) |
| `minitests/` | corpus with `' EXPECT:` oracle (the native leg is the reference) |
| `Apps/` | complete applications that use the compiler |
| `include/`, `Controls/` | `.inc` exports of the libraries and control examples |
| `libs/` | sources of **our** FFI libraries (`src/`, `include/`, `build.sh`) |
| **`settings/`** | **shared compiler seeds**: `system.settings.json` and the manifests (fixtures, brains, libs, ml, chromium, obscura) + `providers.json`. See `settings/README.md` |
| `skills/` | skill archive (markdown) — see `skills/README.md` |
| `plugins/` | plugin descriptors for the agent (`PLUGIN.md` + `tool.json`) — see `plugins/README.md` |
| `spec/`, `conformance/`, `runtime-abi/` | contract: canonical registry/TokenKind, `.bas` runner with `EXPECT`, list of the C→C# ABI symbols (`check_all.sh`, ABI 1.0.0) |
| `webapp/` | shared frontend of the apps |

## Rule: seed vs state

- **Seed** (versioned, identical on every machine) → `settings/`. E.g. `providers.json`, which the
  runtime reads as a fallback when the local file is missing.
- **State** of the installation (active providers, MCP servers, brains, indexes) → `<configDir>`
  (`GMBasic/config`, or `<exe>/config` in a distributed program). **It does not go into git**: it
  contains machine paths.
- The monorepo documentation lives in the **vault** (``);
  `docs/` at the root is reserved for the printed manual (currently empty), the reports of the tools
  are in `tools/reports/`.
