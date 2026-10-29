# Cartella ABI di LLMEnv (descrittori dei modelli)

I descrittori `.gmbarch` (ruoli dei tensori, quantizzazione, strutture, codec) hanno **un solo proprietario: LLMEnv**
(cartella `ABI/` del repo LLMEnv). GMBasic non ne tiene copie e non ne genera uno per modello.

## Dove sta
Non ha un percorso fisso: dipende dal canale con cui LLMEnv e' installato.

| Installazione | Cartella |
|---|---|
| pkg | `/usr/local/lib/llmenv/ABI` |
| tar.gz | `lib/llmenv/ABI`, accanto alle librerie |
| Homebrew | `<prefix>/opt/llmenv/lib/llmenv/ABI` |
| albero sorgente | `LLMEnv/ABI` (fratello di GMBasic) |

Dalla release **0.2.0** di LLMEnv il pacchetto contiene la cartella (con `ABI_VERSION`).

## Impostazione `paths.abiDir`
- Programmi BASIC: `app.settings.json` (cartella del programma). Compilatore: `system.settings.json`. Template: `GMBasicSharp/Resources/system.settings.json`.
- Valore `null` = non ancora localizzata. Se manca, e' `null` o non e' piu' una cartella valida (altra macchina, LLMEnv spostato),
  alla **prima necessita'** (primo uso dei descrittori) il runtime C la cerca (`llmdesc_abi_locate` di LLMEnv), la scrive nel file
  (commenti JSONC intatti) e dalla volta dopo la legge soltanto. L'albero sorgente e `LLMDESC_ABI_DIR` si ritrovano da soli e non si salvano.
- C#: `PathSettings.AbiDir` / `GmbiPaths.AbiDir` espongono solo il valore configurato; la ricerca e la scrittura sono nei dylib C
  (`gmb_ai_modelio` per il compilatore, runtime per le applicazioni), come impone la separazione C / C# (§0bis).

## Se LLMEnv manca
Il caricamento/conversione di un modello **fallisce** con un errore che indica come installare LLMEnv (nessun ripiego silenzioso
sul descrittore generico). Da terminale interattivo il runtime propone `brew tap c-vision/llmenv https://github.com/c-vision/LLMEnv
&& brew install llmenv`; con `GMB_ABI_AUTOINSTALL=1` lo esegue senza chiedere.

## Codice
- `GMBasic/native/gmb_runtime_c/src/gmb_abi.c`: lettura/scrittura testuale della chiave, risoluzione, installazione; registrato con
  `llmdesc_set_abi_policy(.., required=1)` da un constructor. Il compilatore lo compila con `-DGMB_ABI_SCOPE_SYSTEM`
  (`GMBasic/native/gmb_compiler_c/CMakeLists.txt`, target `gmb_ai_modelio`).
- `GMBasic/native/gmb_runtime_c/src/gmb_paths.c` richiama `gmb_abi_link_anchor()` per tenere il modulo nel link da libreria statica.
- `GMBasic/gmbllm/src/llmmodelio_model.c`: il percorso GGUF risolve con `llmdesc_abi_dir`.
- Test: `GMBasic/native/gmb_runtime_c/tests/test_abi_settings.c` (JSONC, percorso obsoleto, prima esecuzione con un `brew` finto, LLMEnv assente).
- `GMBasic/tools/gen_gmbarch_abi.bas` (non versionato) scrive in `../LLMEnv/ABI` solo i descrittori mancanti.
