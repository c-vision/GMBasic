# `shared/settings/` — configurazioni condivise del COMPILATORE

Punto unico delle configurazioni che i due mondi (C# e C) leggono allo stesso modo. Sono
**versionate**: nessun percorso assoluto, nessun segreto, identiche su ogni macchina.

| file | chi lo legge | a cosa serve |
|---|---|---|
| `system.settings.json` | compilatore + runtime (`pr_load`, `GmbiPaths`) | default globali: `paths`, `consoleConfiguration`, `engine`, `compiler`, `importFolders`, `defaultNamespaces` |
| `fixtures-manifest.json` | `gmb_fixtures_ensure*`, compilatore, REPL, CLI | feed grandi scaricati on demand (release `fixtures-v1`) |
| `brains-manifest.json` | `gmb_brains`, `gmb_brain_fetch`, bundle | cervelli distribuiti: versione, forme `rag`/`graph`, repo/tag/asset, sha256 |
| `libs-manifest.json` + `libs-index-<rid>.tsv` | `LibsResolver` (C#), `gmb_libs_ensure` (C) | bundle delle librerie native per RID |
| `ml-manifest.json` | `ModelDownload` / componenti ML | modelli e componenti (torch, mlx) |
| `chromium-manifest.json` | plugin `browser` | build di Chromium per RID |
| `obscura-manifest.json` | agenti CUA | browser headless |
| **`providers.json`** | `AI.PROVIDERS()` in **entrambe** le gambe | **seed** dei provider: tre default del codice (`fixture-embedding`, `fixture-completion`, `keyword-reranker`), `roleOrder`, `activeIndex`, `schemes` (hf/huggingface/ollama/openai/anthropic) |

## Regola

- **Default** → qui (o nel codice). **Stato dell'utente** → `<configDir>` (locale, `.gitignore`).
- Il runtime legge prima il file **locale** e, se manca, questo **seed**. Il bundle copia i
  seed necessari accanto all'eseguibile (`<exe>/config/providers.json`), così un programma
  distribuito funziona senza il checkout.
- `providers.json` è l'unico seed di *configurazione runtime* in questa cartella: è qui perché
  `shared/minitests/ai/gen_AI_Providers.bas` ha il suo contenuto come oracolo e le due gambe
  devono rispondere identiche anche su un checkout pulito (prima il C# faceva
  `File.ReadAllText` diretto e lanciava `FileNotFoundException`).

Dettaglio del lato `<configDir>`: [GMBasic/config/README.md](../../GMBasic/config/README.md).
