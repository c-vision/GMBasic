# V.35 — LLM locale

## Cosa ottieni da questo capitolo

Un modello che gira sulla tua macchina: caricarlo, decidere se sta in RAM, quantizzarlo, servirlo.
Il capitolo spiega anche dove passa il confine di licenza, perché è la parte che si sbaglia quando
si aggiunge una funzione.

## 1. Caricare un modello

```basic
AI MODEL LOAD "qwen", "/percorso/al/modello", RESIDENT
```

| Forma | Comportamento |
|---|---|
| `AI MODEL LOAD n, path` | **swap** predefinito: gli esperti vengono letti da disco |
| `AI MODEL LOAD n, path, RESIDENT` | tutto in RAM: più veloce, più memoria |
| `AI MODEL PROMOTE` / `DEMOTE` | sposta fra residente e paginato |
| `AI.STREAMSTATS()` | quanto è stato letto da disco |

Lo swap è la risposta al caso "il modello non entra": i pesi restano sul disco e il sistema li
pagina quando servono. Costa latenza, non memoria.

Con i pesi in formato MLX/safetensors la stessa scelta si fa alla registrazione:

```basic
AI REGISTER "locale", %CHAT, "<dir>", "{""engine"":""mlx"",""swap"":""true""}"
```

Le chiavi accettate sono `swap`, `diskSwap`, `lazyWeights`: tutte dicono "non materializzare tutto
all'avvio".

## 2. Prima di caricare: ci sta?

Il capitolo V.29 mostra come si chiede al sistema quanta memoria c'è. Un modello da 99 GB con 90 GB
disponibili **non entra**, e scoprirlo dopo il caricamento significa un processo ucciso.

La regola pratica: peso del modello + KV cache + margine del 20%. La KV cache cresce con la
lunghezza del contesto, e su un contesto lungo può superare il modello stesso.

## 3. KV cache e contesto

La KV cache è la memoria che conserva lo stato dell'attenzione per i token già visti. Senza, ogni
token costerebbe il ricalcolo di tutta la sequenza.

| Opzione | Effetto |
|---|---|
| dimensione massima del contesto | più contesto = più KV cache |
| quantizzazione della KV | meno memoria, più velocità, un po' di qualità |
| swap della KV (`kvSwapEnabled`) | sessioni lunghe su disco |
| numero di sessioni calde | quante conversazioni restano in memoria |

`KV-Quant-Hetero-KV-Guida-Utente` documenta le varianti; `ModelResidencyRegistry` è il meccanismo
che demota/espelle le sessioni inattive.

## 4. Quantizzare: il confine di licenza

Qui sta la regola che vale più di tutto il capitolo:

```
Inferenza  → LLMEnv (Apache-2.0)     : caricare, dequantizzare, eseguire
Produzione → gmbllm  (PUL)           : quantizzare, addestrare, LoRA, surgery
```

- il namespace `LLM` **ispeziona** i modelli (architettura, tensor, token) e vive nel perimetro
  Apache;
- il namespace `TRAIN` **produce o trasforma pesi** (finetune, LoRA, distillazione, quantizzazione
  encode) e vive nel perimetro PUL;
- la dipendenza è a senso unico: `gmbllm` linka le librerie Apache, mai il contrario.

Se stai aggiungendo una funzione e non sai dove metterla, la domanda è una: **legge pesi o li
scrive?** Legge → `LLM`. Scrive → `TRAIN`.

## 5. Servire il modello

`LLM-Server-Guida` e `LLM-Server-OpenAI-Compatibile` descrivono il server locale con API
compatibile OpenAI: così un client che parla con un provider remoto parla anche con la tua
macchina, cambiando solo l'URL.

Il vantaggio concreto: sviluppo con un modello locale (nessun costo, nessun dato fuori),
produzione con uno remoto se serve più qualità — senza riscrivere il client.

## 6. Formati e tool

| Formato/strumento | Uso | Guida |
|---|---|---|
| safetensors/MLX | pesi su Apple Silicon | `Modelli-IA-Locali` |
| GGUF | pesi quantizzati, MoE con streaming | `Embedded-LLM-Infrastruttura-Guida` |
| `.gmbllm` | artifact del compilatore di modelli | `LLM-Arch-ModelCompiler-Guida-Utente` |
| tokenizer | `LLM-Tokens-Guida-Utente` | conteggio e vocabolario |
| distillazione | `AI-Distill-Guida-Utente` | modello piccolo da uno grande |

## 7. Prestazioni: dove si perde tempo

L'esperienza del progetto è netta: la maggior parte dei "modello lento" non è un kernel lento, è
**overhead** — allocazioni per token, copie di pesi, viste ricreate a ogni chiamata. Prima di
cercare un'ottimizzazione del calcolo, conta le allocazioni per token.

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| processo ucciso dal sistema | modello + KV oltre la RAM | usa swap o un modello più piccolo |
| lentezza dopo il primo token | pesi non residenti | `RESIDENT` se la memoria basta |
| qualità crollata | quantizzazione troppo aggressiva | usa una quantizzazione più alta |
| `AI.STREAMSTATS()` sempre a zero | modello residente | è atteso: nessuna lettura da disco |
| funzione nuova nel posto sbagliato | confine di licenza ignorato | leggere → `LLM`, scrivere → `TRAIN` |

## 9. Dove andare adesso

- **V.32** — la superficie `AI` che usa il modello.
- **V.29** — memoria disponibile prima del caricamento.
- **V.31** — i tensori su cui gira l'inferenza.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### LLM-Arch-ModelCompiler-Guida-Utente

*Fonte: `LLM-Arch-ModelCompiler-Guida-Utente`*


## 1. Descrizione d'architettura `.gmbarch` — comandi `LLM ARCH`
| Comando | Parametri | Cosa fa |
|---|---|---|
| `LLM ARCH LIST` | — | Elenca le descrizioni `.gmbarch` disponibili (la cartella ABI di LLMEnv; il vecchio `./arch` resta solo come elenco legacy). |
| `LLM ARCH RESOLVE <hf_dir>` | `<hf_dir>` = cartella del modello (con `config.json`) | Risolve e carica la descrizione; senza dir → errore esplicito. |
| `LLM ARCH SHOW` | — | Stampa ruoli / quant / structures della descrizione **risolta**. Se nessuna è risolta: messaggio esplicito. |
| `LLM ARCH CHECK <hf_dir>` | `<hf_dir>` | Conta i tensori dell'index e quanti **non hanno ruolo** (`OTHER`). |
| `LLM ARCH PLAN <hf_dir>, <out>` | `<dir>`, `<out>` = file di uscita | Scrive il piano **per tensore**: `name → role → formato proposto → (scale)`, calcolato DALLA descrizione. Legge solo indice+config (leggero). |

## 2. Policy dei formati — comandi `LLM POLICY`
| Comando | Parametri | Cosa fa |
|---|---|---|
| `LLM POLICY SHOW` | — | Mostra le regole attive + i default. |
| `LLM POLICY SET <ruolo>, <formato>` | `ruolo` (es. `ROUTED_EXPERT`), `formato` (es. `iq2_xxs`) | Imposta/aggiorna la regola. |
| `LLM POLICY RESET` | — | Azzera le regole esplicite (si torna ai default). |

## 2bis. Policy nominata (DSL Livello A) + `.gmbpolicy`
| Comando | Cosa fa |
|---|---|
| `LLM POLICY DEFINE "nome", "<regole>"` | Definisce una policy nominata. Regole separate da `;`, condizione/azione con `:`. Condizioni `AND` con `&`: `ROLE=<r>`, `LIKE=<glob con * ?>`, `ARCH=<a>`, `LAYER<op><n>`, `IMP<op><v>` (imatrix); op in `= != >= <= > <`. Azione = un formato (`q2k…q6k`, `q4_0`, `q8_0`, `iq1s`/`iq1m`/`iq2xs`/`iq2s`/`iq2xxs`/`iq3xxs`/`iq3s`, `mxfp4`, `nvfp4`, `f16`, `f32`, `bf16`) o `NOQUANT`; **QuantIR I11**: azione a **4 assi** `w=q4_k,a=fp8,acc=f16,o=f16` (pesi/activation/accum/output — la precisione del CALCOLO); `DEFAULT:<fmt>` è il fallback. |
| `LLM POLICY APPLY "nome"[, "DRYRUN"]` | Attiva la policy (l'ultima vince); con `DRYRUN` mostra le regole **senza** attivarla. |
| `LLM POLICY DIFF <modello>` | Piano per-tensore **policy vs default** (report-only, nessuna scrittura). |
| `LLM POLICY ITEMS` / `LLM POLICY ITEM <i>` | Elenca / legge una voce del piano memorizzato (JSON / indice `i`). Il piano è prodotto da `DIFF`. |
| `LLM POLICY SETITEM "<name-glob>", "<fmt>"` | **Livello B**: decisione **per-tensore** con priorità massima (sopra DSL/ricetta/ruolo). `<name-glob>` = nome tensore o glob (`*`/`?`). |
| `LLM POLICY CLEARITEM` | Azzera gli override `SETITEM` (si torna a DSL/ricetta/ruolo). |
| `LLM POLICY CALLBACK <Proc>` | **Livello B (algoritmico)**: la GPM itera il piano e invoca `FUNCTION <Proc>(name AS STRING, role AS STRING, dflt AS STRING) AS STRING` per-tensore; il valore ritornato è il formato scelto. `<Proc>` è un **identificatore di procedura** (proc-ref), non una stringa. |
| `LLM POLICY GATE <base>[, <ref>]` | Quantizza `base`→`<base>.out.gguf` con la policy attiva, misura il gate (`ppl` di `ref` vs `out`) e, se **rifiutato**, fa **rollback** (rimuove l'output). `<ref>` opzionale: senza, verdict = `ACCEPT` ("quantized only"). |
| `LLM POLICY RECOVER <model>, <dataset>, <ref>` | **§9.B — il "training reale" del quality gate**: composizione `gate → retrain → re-eval → ACCEPT/ROLLBACK`. Se `gate(ref, model)` rifiuta, esegue un **fine-tune LoRA** di `model` su `dataset` (deriva `<dataset>.train.bin`→`.hold.bin`), lo **fonde** in `<model>.recovered` (base intatta) e **ri-misura**: `ACCEPT` tiene il recuperato, `REJECT` fa **rollback**. Stampa un report JSON (`verdict`, `trained`, `recovered`/`rolled_back`, `gate`). Vedi §2bis-bis. |
| `LLM POLICY SAVE <file.gmbpolicy>` / `LOAD <file>` | Persistenza: scrive/rilegge sia le regole `SET` sia le **policy nominate** (`rules` + `policies`). |
| `LLM POLICY RESET` | Azzera le regole esplicite `SET` per-ruolo (si torna ai default). |

LLM POLICY DEFINE "mix", "ROLE=ROUTED_EXPERT&LAYER>=12:q6k;LIKE=*_down_exps:q6k;DEFAULT:q4k"
LLM POLICY APPLY "mix", "DRYRUN"     ' prova senza attivare
LLM POLICY SAVE "work/mix.gmbpolicy" ' persisti
LLM POLICY LOAD "work/mix.gmbpolicy" ' ricarica (round-trip verificato, parity4 4/4)


### 2bis-bis. Recupero qualità dopo un rifiuto — `LLM POLICY RECOVER` (§9.B)

' dataset di calibrazione (righe jsonl {"prompt":..,"response":..}):
TRAIN DATASET BUILD "work/recover.jsonl", "work/recover", {tokenizer: "path/tokenizer.json"}
' recupero (modelli = dir MLX):
LLM POLICY RECOVER "path/model_mlx", "work/recover.train.bin", "path/ref_mlx"

| Parametro | Obbligatorio | Descrizione |
|---|---|---|
| `<model>` | sì | Dir **MLX** del modello da recuperare (trainable); può essere già degradato. |
| `<dataset>` | sì | File `<x>.train.bin` prodotto da `TRAIN DATASET BUILD`. Il holdout `<x>.hold.bin` è **derivato** automaticamente (sostituzione `.train.bin`→`.hold.bin`). |
| `<ref>` | sì | Dir **MLX** di riferimento (il modello "buono"): il gate misura `ppl(ref)` vs `ppl(cand)`. |

## 2ter. `LLM MODEL QUANTIZE` — ri-quantizzazione (riduzione) di modelli già quantizzati
| Forma | Parametri | Cosa fa |
|---|---|---|
| `LLM MODEL QUANTIZE <in>, <out>, <bits>, <group>` | dir MLX/HF già quantizzata | **Dequant** dei pesi U32+scales/biases (qparams **per-modulo** dal `config.json` sorgente) e **ri-encode** ai bits/group richiesti. Scrive `<out>` con `model.safetensors`, `model.safetensors.index.json`, config aggiornato. |

## 3. Verbi del model compiler (namespace `LLM`)
| Verbo | Parametri | Cosa fa |
|---|---|---|
| `LLM PLAN <in>, <out>` | 2 file GGUF | Scrive il piano (ruolo + formato proposto) per tensore. |
| `LLM TENSOR IR <path>` | modello GGUF/dir MLX | **MODEL IR**: tabella per-tensore (nome/ruolo/tipo/dims/byte). §4.2. |
| `LLM COMPILE <model>, <out.gmbllm>` | GGUF + out | **ARTEFATTO** eseguito specializzato `.gmbllm` (top-level §4.9). |
| `LLM QUALITY <model>, <prompt>, <out>` | dir MLX **o file `.gguf`** + prompt + out | **GATE NLL/perplexity** (§4.5 I6), via engine MLX/Metal. Ora alias di **`TRAIN EVAL`** (hard move). Accetta `.gguf` (tokenizer embedded): misura la PPL di un modello quantizzato da `LLM QUANT`. |
| `LLM QUANT <in>, <out>` | 2 file GGUF | Quantizza e riscrive il GGUF secondo la policy/ricetta (formato per-ruolo). |
| `LLM QUANT <in>, <out>, <bpw>` | 2 file GGUF + `bpw` | **AUTO-ALLOCAZIONE** bit per-tensore sotto il budget `bpw` (es. `4.5`): Q2_K..Q8_0 scelti per sensibilità. v. §3ter-quater. |
| `LLM MERGE <a>, <b>, <out>` | 3 file | Model soup. Metodo via `LLM POLICY SET "MERGE_METHOD", "mean"(def)|"dare"|"ties"` — dare/ties operano sul **task-vector `b-a`** (base `a`); `MERGE_P`=P(drop) DARE (def .1), `MERGE_K`=frazione tenuta TIES (def .2). MiniTest `llm_merge_soup`. |
| `LLM PRUNE <in>, <out>, <keep>` | file + intero | Riduce gli esperti a `keep` (router coerente). |
| `LLM EXTRACT <in>, <out>, <layer>, <expert>` | file + 2 interi | Estrae un esperto come FFN denso. |
| `LLM PROFILE <model>, <out>` | file modello + out | Report del routing reale (uso per esperto). |
| `LLM MODEL INSPECT <path>` | dir HF/MLX o file `.gguf` | Dump read-only del descriptor (v. §3ter-bis). |
| `LLM MODEL SAVE <src>, <dst>` | 2 file GGUF | **EMITTER**: riscrive l'artefatto (metadata + byte tensori). v. §3ter-bis. |
| `LLM MODEL SAVE <gguf>, <dir>.mlx` | GGUF + directory | **EMITTER MLX**: emette un modello **MLX** (`config.json` + `model.safetensors`), tensori con i nomi **HF** (`model.layers.N.self_attn.q_proj`) e quantizzazione affine (`scales`/`biases`). L'arch è risolta da `general.architecture` (fallback: ABI senza underscore). |
| `LLM MODEL ABLATE <in>, <out>, <pattern>` | 2 file GGUF + pattern | **MODIFICARE**: azzera i tensori il cui nome contiene `pattern`. v. §3ter-ter. |
| `LLM MODEL REMOVE <in>, <out>, <layer>` | RIMUOVERE layer: elimina il layer N + rinumerazione (depth-1). |
| `LLM MODEL DIFF <a>, <b>, <out>` | 2 file GGUF + report | **VERIFICA**: report strutturale delle differenze fra due artefatti. v. §3ter-ter. |
| `LLM MODEL CHECKPOINT <src>, <store>, <name>` | file + dir + nome | **CHECKPOINT**: snapshot nel checkpoint-store (+ meta con hash). v. §3ter-ter. |
| `LLM MODEL ROLLBACK <store>, <name>, <dst>` | dir + nome + file | **ROLLBACK**: ripristina dal checkpoint-store (verifica integrità). v. §3ter-ter. |
| `LLM MODEL ADD <base>, <module>, <out>` | 3 file GGUF | **AGGIUNGERE**: aggiunge/sovrascrive i tensori di un modulo. v. §3ter-ter. |
| `LLM MODEL ADAPTER <base>, <adapter>, <out>[, <alpha>]` | base + `.safetensors` + out | **AGGIUNGERE**: applica un adapter LoRA. v. §3ter-ter. |
| `LLM MODEL MODULE <base>, <gmmod>, <module>, <out>` | base + `.gmmod` + modulo + out | **AGGIUNGERE**: modulo dichiarato (ABI + risoluzione + esecuzione). v. §3ter-ter. |
| `LLM MODEL MODALITY <base>, <mmproj>, <out.gmmod>` | base + mmproj + out | **AGGIUNGERE**: emette il `.gmmod` (modality dall'ABI). v. §3ter-ter. |

### 3ter-quater. `LLM QUANT … , <bpw>` — auto-allocazione bit per-tensore

LLM QUANT "modello.gguf", "compresso.gguf"        ' ricetta per-ruolo/policy
LLM QUANT "modello.gguf", "compresso_auto.gguf", 4.5   ' auto a 4.5 bpw


### 3ter-quinquies. `LLM TENSOR SENSITIVITY` — sensibilità **empirica** ΔPPL per ruolo (S2)

LLM TENSOR SENSITIVITY <model.gguf>, <dataset.txt>

| Argomento | Obbligatorio | Spiegazione | Default |
|---|---|---|---|
| `<model.gguf>` | sì | modello GGUF F16 da misurare. | — |
| `<dataset.txt>` | sì | testo di calibrazione (una riga = un prompt). | — |
| bits | no | bit del candidato per il ruolo (2/3/4/5/6/8), via `POLICY SET "SEN_BITS"`. | 4 |

### 3ter-sexies. `LLM TENSOR AUTOGATE` — gate di accettazione AUTO vs ricetta baseline (S3)

LLM TENSOR AUTOGATE <model.gguf>, <dataset.txt>

| Argomento | Obbligatorio | Spiegazione | Default |
|---|---|---|---|
| `<model.gguf>` | sì | modello GGUF F16 sorgente. | — |
| `<dataset.txt>` | sì | testo di calibrazione (una riga = un prompt). | — |
| bpw target | no | via `POLICY SET "AUTO_BPW"`. | 4.5 |

### 3ter-septies. `LLM TENSOR USAGEEVAL` / `USAGEGATE` — obiettivo d'**USO** (S6)

LLM TENSOR USAGEEVAL <model.gguf>, <bench.tsv>
LLM TENSOR USAGEGATE <model.gguf>, <bench.tsv>     ' bpw da POLICY "AUTO_BPW" (default 4.5)


## 3bis. Superficie `.gmbarch`: `LLM TENSOR` / `LLM KERNEL` / `LLM MODULE` / `LLM KV`

### `LLM TENSOR` — ruolo e formato dei tensori
| Forma | Parametri | Cosa fa |
|---|---|---|
| `LLM TENSOR LIST <dir>` | `<dir>` = cartella modello (con `config.json`) | Elenca ogni regola `match → ruolo → formato` della descrizione. |
| `LLM TENSOR ROLE <dir>, <tensor>` | `<tensor>` = nome tensore (es. `layers.0.attn.wkv.weight`) | Stampa `ruolo` + `formato` proposto per quel tensore; non mappato → `OTHER`. |

### `LLM KERNEL` — primitive (kernel) e composizione
| Forma | Parametri | Cosa fa |
|---|---|---|
| `LLM KERNEL SHOW <dir>` | `<dir>` | Elenca le strutture con `type`, `prim` e la pipeline `op->prim` degli `steps`. |
| `LLM KERNEL SHOW <dir>, <kind>` | `<kind>` = chiave struttura (es. `attention`) | Solo la pipeline di quella struttura. |
| `LLM KERNEL CODEC <dir>, <fmt>` | `<fmt>` = formato (es. `mxfp4`) | Risolve il **plugin codec** che implementa il formato (o «built-in» del runtime). |
| `LLM KERNEL PRIM <dir>, <kind>` | `<kind>` | Risolve il **plugin primitiva** dichiarato per la struttura (`prim`). |

### `LLM MODULE` — moduli (strutture) e supporto del converter
| Forma | Parametri | Cosa fa |
|---|---|---|
| `LLM MODULE SHOW <dir>` | `<dir>` | Elenca le strutture dichiarate e se il **converter** le sa mappare (supportata / NON supportata). |
| `LLM MODULE REGISTER` / `LLM MODULE PRIORITY` | — | **Riservato** su `LLM`: la registrazione reale di moduli/librerie usa il comando **`GMB MODULE`** (vedi §3septies). |

### `LLM KV` — metadati runtime (motore)
| Forma | Parametri | Cosa fa |
|---|---|---|
| `LLM KV SHOW <dir>` | `<dir>` | Stampa `state`, `kv.dtype`, `activation.dtype` dalla sezione `runtime` della `.gmbarch`. |
| `LLM KV SET` | — | **Non supportato qui**: la KV è configurazione del **motore**, non del compilatore (errore esplicito). |

## 3ter. Conversione di formato — `LLM MODEL CONVERT`
| Forma | Parametri | Cosa fa |
|---|---|---|
| `LLM MODEL CONVERT <src>, <dst>` | 2 path | Default `GGUF`: container esperti out-of-core (layer MoE). |
| `LLM MODEL CONVERT <src>, <dst>, "GGUF"` | + `"GGUF"` | Container esperti `GGUF` (percorso storico, invariato). |
| `LLM MODEL CONVERT <src>, <dst>, "GGUF"` | + `"GGUF"` | MLX/HF (safetensors) → **GGUF**, guidato dalla `.gmbarch` del sorgente (converter del progetto codec). |
| `LLM MODEL CONVERT <src>, <dst>, "MLX"` | + `"MLX"` | GGUF → **MLX** (safetensors), idem. |
| `LLM MODEL CONVERT <src>, <dst>, "F16"` | + `"F16"` | MLX/HF → **GGUF a tensori F16** (NON quantizzati): è la **sorgente per `LLM QUANT`/auto**, che elaborano **solo** tensori F16/F32. Default `GGUF` = MXFP4. |

## 3ter-bis. Ispezione ed EMITTER — `LLM MODEL INSPECT` | `LLM MODEL SAVE`
| Comando | Sintassi | Parametri | Descrizione |
|---|---|---|---|
| INSPECT | `LLM MODEL INSPECT <path>` | `path` = **dir** HF/MLX **oppure** file `.gguf` | Dump **read-only** del **descrittore `.gmbarch`** del modello (risolto via `ABI/*.gmbarch` + `config.json`), senza convertire né caricare i pesi. Stampa: `.gmbarch` (path), `arch`, `structures` (attention/moe/…), `recipe`/`boost_fmt`, `roles`/`quant`/`codecs` (conteggi), `modality`. **GMAD eliminato**: l'arch viene **solo** dal `.gmbarch`. |
| SAVE (**EMITTER**) | `LLM MODEL SAVE <src>, <dst>` | `src`, `dst` = path (stringhe: v. nota §3quater sugli argomenti) | **EMITTER / writer**: legge metadata KV + tensor-info da `src` (GGUF) e riscrive in `dst` un GGUF nuovo con gli **stessi metadati** + i **byte dei tensori**. È il pezzo che **produce** l'artefatto, indispensabile al ciclo di *model surgery*: `INSPECT → QUANT/EXTRACT/PRUNE → **SAVE** → LOAD → esegui`. Su errore (path inesistente, file non GGUF, tensori vuoti…) alza un **errore catturabile** con `ON ERROR GOTO`/`GOSUB`. |

IMPORT LLM
SUB Main()
  ON ERROR GOTO Err
  LLM MODEL INSPECT "modello.gguf"            ' 1) ispezione read-only
  LLM QUANT "modello.gguf", "compresso.gguf"  ' 2) trasformazione (ricetta per-ruolo)
  LLM MODEL SAVE "compresso.gguf", "finale.gguf"  ' 3) EMITTER: scrive l'artefatto
  PRINT "OK"
  GOTO Fine
Err:
  PRINT "errore: "; ERR
Fine:
END SUB


## 3ter-ter. Model surgery end-to-end — RIMUOVERE / MODIFICARE / AGGIUNGERE

INSPECT  →  QUANT / PRUNE / EXTRACT / MERGE  →  MODEL SAVE (EMITTER)  →  CONVERT MLX  →  LOAD → genera


IMPORT LLM
SUB Main()
  LLM PRUNE "modello.gguf", "potato.gguf", 48          ' 64 -> 48 esperti, router coerente
  LLM MODEL SAVE "potato.gguf", "potato_saved.gguf"    ' EMITTER
  LLM MODEL CONVERT "potato_saved.gguf", "potato_mlx", "MLX"
  ' poi AI REGISTER "potato_mlx" + AI AGENT ASK … -> genera
END SUB

| parametro | significato |
|---|---|
| `in`  | GGUF sorgente (MoE) |
| `out` | GGUF destinazione (potato) |
| `keep`| numero di esperti routed da **mantenere** (≤ n_esperti) |

IMPORT LLM
SUB Main()
  LLM MODEL ABLATE "modello.gguf", "ablato.gguf", "layers.17.mlp.switch_mlp"  ' azzera per pattern
  LLM MODEL SAVE "ablato.gguf", "ablato_saved.gguf"                            ' EMITTER
  LLM MODEL CONVERT "ablato_saved.gguf", "ablato_mlx", "MLX"
  ' poi AI REGISTER "ablato_mlx" + AI AGENT ASK … -> genera
END SUB

| parametro | significato |
|---|---|
| `in`  | GGUF sorgente |
| `out` | GGUF destinazione (modificato) |
| `pattern` | sottostringa del **nome tensore** da azzerare (es. `wo_b`, `layers.17.mlp.switch_mlp`, `mlp.gate`) |
| marcatori riga | significato |
|---|---|
| `ONLY_A <tensore>` / `ONLY_B <tensore>` | tensore presente solo in A / solo in B (add/prune) |
| `META_DIFF <tensore>` / `SHAPE_DIFF <tensore>` | differenza di tipo/dimensione o forma |
| `DIFF <tensore> (<n> bytes)` | contenuto byte diverso (quant/norm/ablazione) |
| `SUMMARY … equal=… differ=… only_a=… only_b=… bytes_differ=…` | riepilogo |

LLM MODEL DIFF "prima.gguf", "dopo.gguf", "work/diff.txt"   ' report delle differenze


LLM MODEL CHECKPOINT "modello.gguf", "work/ckpt", "prima"   ' snapshot (+ work/ckpt/prima.meta: size+fnv64)
LLM MODEL ABLATE "modello.gguf", "cand.gguf", "layers.17.mlp.switch_mlp"
LLM MODEL ROLLBACK "work/ckpt", "prima", "modello.gguf"      ' ripristino se il candidato non convince


LLM MODEL ADD "base.gguf", "adapter.gguf", "arricchito.gguf"   ' merge dei tensori del modulo


W ← W + (alpha / r) · B @ A        (r = rango, A=[r,in], B=[out,r])


LLM MODEL ADAPTER "base.gguf", "adapter.safetensors", "sft.gguf", "16"   ' alpha=16
LLM MODEL DIFF "base.gguf", "sft.gguf", "work/lora_delta.txt"            ' solo i tensori toccati


LLM MODEL MODALITY "base.gguf", "mmproj-...gguf", "mio.gmmod"   ' -> arch+modality dall ABI
LLM MODEL MODULE   "base.gguf", "mio.gmmod", "mmproj-...gguf", "multimodale.gguf"


{ "module": "vision-qwen2omni", "arch": "qwen2_omni", "modality": "vision",
  "source": { "format": "gguf", "mmproj": "mmproj-Qwen2.5-Omni-3B-Q8_0.gguf" } }


## 3quinquies. MODEL INSPECTOR / IR / Planner / GPU — sotto-verbi `LLM TENSOR`
| sotto-verbo | parametri | descrizione |
|---|---|---|
| `IR` | `<path>` | tabella per-tensore: nome, **ruolo**, tipo, dims, byte (GGUF o dir MLX). |
| `INFO` | `<path>` | riepilogo: format, arch, n-tensori, kv, n-layer, modality, byte. |
| `LAYERS` | `<path>` | indici di layer presenti + n-tensori per layer. |
| `CHECK` | `<path>` | validatore **spec-vs-modello**: arch risolta + 0 tensori `OTHER` (errore esplicito se no). |
| `ARCHRECON` | `<path>` | **valida la descrizione `.gmbarch`** risolta dal modello (UNICA fonte d'architettura; GMAD eliminato): stampa `RICONCILIATO` se il `.gmbarch` combacia con l'arch del modello, altrimenti `DIVERGENZA`. Richiede un file `.gguf`. MiniTest: `llm_tensor_archrecon_ok`/`_error`. |
| `MEMORY` | `<path>` | byte per categoria (embedding/attention/experts/router/ffn/norm/output) + %. |
| `COST` | `<path>` | pesi + **KV byte/token** (da metadati GGUF) + totale@contesto. |
| `DECODEPROFILE` | `<path>[, <ctx[:bw_gbs]>]` | **Profilo decode** (I3): param attivi + KV@ctx + banda -> **tok/s bound** (memory-bound/roofline). |
| `GRAPH` | `<path>` | grafo per layer (attention/moe/ffn/norm) + tipo attention/moe (I1). |
| `NUMGRAPH` | `<path>` | **NUMERIC GRAPH IR** (§4.9): catena di OP numeriche per-layer + precisione (weights/act/accum/out) + roofline. |
| `SPEC` | `<path>` | speculazione dichiarata dall'ABI (NONE/MTP/DSPARK) (I9). |
| `KV` | `<path>[, <mode>]` | **KV_FORMAT/KV_MODE**: kv.dtype + modo (FULL/PAGED/ROTATING/DISK_BACKED) + dettagli. |
| `STATE` | `<path>` | STATE IR: fase prefill/decode + state/modality/speculation/kv (I7). |
| `MOBJ` | `<path>` | MEMORY OBJECT: per regione (weights/kv/activation/expert_workset) device/lifetime/transfer (M10). |
| `CONTEXT` | `<path>` | budget di contesto: `max_tokens(kv_bound)` + policy TRIM/GC/PAGED. |
| `GPU` | `<path>` | analisi GPU: kernel specialization + **divergenza routing** (spread) + cost (§4.7). |
| `PROFILE` / `RESIDENCY` | `<path>[, <budget_gb>]` | PROFILER(I4): byte/categoria + KV/token + **RESIDENCY** (budget→RESIDENT/SWAP) (I12). |
| `DIST` / `DISTRIBUTION` | `<path>[, <world>]` | tensor-parallel planner: **EP**/TP/riplicated + byte/rank (§4.6). |
| `EPLAN` | `<path>[, <world>]` | assegnazione **esperti→rank** (round-robin) + conteggio per rank. |
| `SCHEDULE` / `BATCH` | `<path>[, <seqlen>]` | continuous batching: **max_batch** da budget KV (§4.6). |
| `CED` | `<path>[, <gate>[`:`<eps>]]` | Model-Aware Computation Elimination (§4.8): prova **ESATTA** (ALL-ZERO) + **BOUNDED** (`maxw<eps`, errore limitato `norma_inf<eps>`). `<gate>`=`on`/`1` abilita il kill-switch (COMPILE applica l'ESATTA); `on:<eps>` abilita la prova bounded. MiniTest: `llm_tensor_ced_ok`, `llm_tensor_ced_bounded`. |
| `PRUNEHEADS` | `<in>, <out>` | **pruning STRUTTURATO delle teste di attenzione** (L7/G4a): tiene le prime `n` teste (`n` da `LLM POLICY SET "PRUNE_HEADS", "<n>"`). Slice q/k/v (`ne[1]`) e attn_output (`ne[0]`), aggiorna `attention.head_count`/`head_count_kv`. F16/F32. MiniTest `llm_tensor_pruneheads`. |
| `PRUNEFFN` | `<in>, <out>` | **pruning STRUTTURATO dell'HIDDEN** (G4a): taglia la dimensione **intermedia del FFN denso** alle prime `k` unità (`k` da `LLM POLICY SET "FFN_KEEP","<k>"`). `ffn_gate/up` (`ne[1]`), `ffn_down` (`ne[0]`); aggiorna `feed_forward_length`. F16/F32. (Da usare dopo `MOEDENSE`.) MiniTest `llm_tensor_pruneffn`. |
| `MOEDENSE` | `<in>, <out>` | **MoE → denso** (G4b): fonde gli E esperti in **UN FFN denso** (media sugli esperti), elimina il router (`ffn_gate_inp`) e imposta `expert_count=1`; `*_exps` → `ffn_gate/up/down.weight`. F16/F32. MiniTest `llm_tensor_moedense`. |
| `EXECPLAN` / `PLAN` | `<path>, <out>` | **GMB_AI_PLAN** serializzabile (JSON: layers/ops/memory/kernels). Contiene anche l'array **`"layers"`** (posizionale, indice=layer) con `ops` per-layer — **formato consumato dal runtime MLX** (§9.A M10 EXECUTION PLANNER): puntando l'opzione registro `execplan` al file (`AI SET OPTION "execplan", "<file>"`), l'engine applica il **piano esecutivo per-layer** (pipeline per-layer + `skip` → layer saltato/identità). **Produttore di `skip` (CED)**: un layer con **tutti** i tensori **ALL-ZERO** viene marcato `{"skip":true}` (campionato come CED; riga di riepilogo con `skip=N`). MiniTest `llm_tensor_execplan_ok`/`_skip`/`_error` (5/5). Sample `ExecPlanRuntime.bas`, `ExecPlanSkip.bas`. |
| `KVCLASSES` / `KV_MEMORY` | `[, <out.json>]` | **classi KV** (§9.A M10 MEMORY OBJECT / M10d): **KV_RUNTIME** (RAM vivo) vs **KV_PERSISTENT** (disco, ricostruibile) — byte + numero oggetti, dal manager runtime dell'engine (`llm_mlx_kvclasses`). 2º arg `.json` = serializza. MiniTest `llm_tensor_kvclasses`. Sample `KvMemoryClasses.bas`. |
| `MOBJLIVE` / `MOBJ_RUNTIME` | `[, <out.json>]` | **MEMORY OBJECT runtime manager unificato** (§9.A M10): totali + quote per categoria (`weights`/`expert_ram`/`expert_device`/`kv_runtime`/`kv_persistent`/`prefix`/`temp`), budget globale e priorità di eviction (K3). Engine `llm_mlx_mobj`; i tier registrano i loro oggetti. MiniTest `llm_tensor_mobjlive`. Sample `MobjRuntime.bas`. |
| `KVDAG` / `KV_DAG` | `[, <out.json>]` | **KV come DAG** (§9.A M10b): nodi `KV_BASE`/`KV_DERIVED`/`KV_INDEX` con dipendenze; espansione **PREFETCH topologica** (`READ`/`REUSE`/`RECONSTRUCT`) del grafo CSA2 rappresentativo (engine `llm_mlx_kvdag`). MiniTest `llm_tensor_kvdag`. Sample `KvDagPrefetch.bas`. |
| `KVDECIDE` / `KVRECOMPUTE` | `<path>, "<n_tokens>,<kv_bytes>"` | **reconstruction_cost / policy RECOMPUTE↔PERSIST↔KEEP** (§9.A M10a): sceglie il minimo tra `COST_RECOMPUTE`=`n_tokens·costo/token`, `COST_PERSIST`=`kv_bytes/banda`, `COST_KEEP`; stampa la decisione dal modello di costo dell'engine (`llm_mlx_kvcost`). MiniTest `llm_tensor_kvdecide`. Sample `KvRecomputePolicy.bas`. |
| `KVFORMAT` / `KV_FORMAT` | `<path>[, "<dtype>"]` | **contratto GMB_KV_FORMAT** (§9.A M10c): dtype di STORAGE del KV. Senza `<dtype>` mostra il nativo + gli ammessi; con `<dtype>` **VALIDA** la richiesta (class `NATIVE`/`SUPPORTED`/`QUANTIZABLE`/`RECONSTRUCTABLE`/`UNSUPPORTED` → `ACCEPT`/`REJECT`). La famiglia **fp4** è ammessa **solo se nativa** del modello (mai conversione silenziosa). Engine: `llm_mlx_kvformat`. MiniTest `llm_tensor_kvformat`. Sample `KvFormatContract.bas`. |
| `GEXPORT` | `<path>, <out.dot>` | esporta il grafo in **DOT** (§4.9). |
| `COMPILE` | `<path>, <out>` | artefatto `.gmbllm` (graph+memory_plan+capacity+kernels). |
| `LOADPLAN` | `<file.gmbllm>` | **carica/valida** un artefatto `.gmbllm`. |
| `OPRECORD` | `<path>[, <out>]` | **GMB OPTIMIZATION RECORD** (§4.5): record serializzabile (model/arch/weights/kv + planner/residency/kv_mode/autotune/spec). |
| `QUANTIR` | `<path>` | Quant IR (I11): **4 precisioni per OP** (weights/activation/accum/output) per ruolo **+ blocco per-tensore** con la precisione effettiva risolta dalla policy (`SETITEM` > DSL > ruolo), override marcati con `*`. Gli assi **activation/accum/output** sono **risolti** dalla `.gmbarch` (default `f16`/`f32`/pesi) e dall'**azione a 4 assi** della policy (`w=q4_k,a=fp8,acc=f16,o=f16`); nella `.gmbarch` la forma oggetto `{"weights":..,"activation":..,"accum":..,"output":..}`. È un **IR modificabile**: si cambia con `LLM POLICY SETITEM`/`DEFINE` e si rilegge qui. MiniTest: `llm_tensor_quantir_ok`, `llm_quantir_i11`. |
| `OBUDGET` | `<path>[, <O0..O4>]` | **QUALITY BUDGET** (I6): mappa il livello a un budget bit/peso e verifica il piano per ruolo (fallback dai nomi tensore); verdetto ACCEPT/REJECT-FLAG. |
| `PHASE` (o `PREFILL`/`DECODE`) | `<path>[, <seqlen>]` | Piano fasi **PREFILL \| PREFILL_KV \| DECODE** (I7): geometria, KV/token, classe di op, roofline. |
| `LOGITS` (o `TOPK`) | `<model_dir>, <prompt>` | **LOGITS/TOP-K**: distribuzione del prossimo token — top-10 logprob+prob + **entropia** (bit). Richiede una dir MLX + tokenizer. |
| `CONTEXTGC` | `<path>[, <cur:target>]` | **CONTEXT GC** (§4.10): piano di eviction del prefisso KV (token/byte liberati, impatto prefix-cache). |
| `CONTEXTMAP` | `<path>[, <ctx>]` | **CONTEXT MAP** (§4.10): segmenti (SYSTEM/TOOL/HISTORY) + token budget + KV + strategia COMMIT/TRIM/GC. |
| `MEMPLAN` | `<path>` | **STATIC MEMORY PLAN** (arena): regioni weights/kv/scratch + lifetime. |
| `CAPACITY` | `<path>[, <ctx>]` | **EXACT CAPACITY** (§4.9): capacità esatta a ctx (weights + kv@ctx + scratch) + regioni arena + fit RAM. |
| `MPLAN` | `<path>[, <budget_gb>]` | **M10 MEMORY PLANNER** compile-time: MEMORY OBJECT per regione (device/lifetime/residency/prefetch/eviction/transfer_cost) + **fit** su budget + piano DATA-MOVEMENT. |
| `EXPOLICY` | `<path>[, <budget_gb>\|<out.json>]` | **EXPERT_RESIDENCY_POLICY** (LLAMA OPTIMIZER): n_esperti/top_k, policy STATIC/LRU+RECENCY dal budget + regola **router→PREFETCH**. Con 2º arg **`.json`** emette il **piano policy** (formato runtime MLX: `mode`/`hotness`/`prefetch_topn`) — **applicabile live** con `AI SET OPTION "expolicy", "<out.json>"` (§9.A). MiniTest `llm_tensor_expolicy_emit`. Sample `ExpertResidencyPolicy.bas`. |
| `KVPLAN` | `<path>[, <mode:ctx:page>]` | **KV IR/PAGED**: mode (FULL/PAGED/ROTATING/DISK_BACKED), pages, prefix-cache, prefetch, eviction, kv/token + kv@ctx. |
| `VIEW` | `<path>` | VIEW/LAYOUT IR: layout/dims/view zero-copy per tensore. |
| `POLICYIR` | `<path>` | Policy definibile (§4.3): regole dichiarative `WHEN role=… THEN fmt=…` + recipe. |
| `IRLEVELS` | `<path>` | Gerarchia IR **L0–L5** + fasi PREFILL/PREFILL_KV/DECODE (§4.2). |

### `LLM TENSOR` — tokenizer / vocab (B2, 2026-10-08)
| Comando (`LLM X` = alias di `LLM TENSOR X`) | Parametri | Semantica / output |
|---|---|---|
| `TOKENIZE` / `TOKENS` | `<model>, <testo>` | Tokenizza → `n=<K> ids=<id,id,…>`. |
| `DETOKENIZE` / `DETOK` | `<model>, <id,id,…>` | Decodifica gli id → `n=<K> text=<testo>`. |
| `VOCAB` | `<model>[, <filtro>]` | `vocab_size=<N>`; col **filtro** (opzionale) i primi (≤50) token che **contengono** la sottostringa + `match=<n>`. |
| `TOKENIZER` / `TOKINFO` | `<model>` | `vocab_size=<N> source=<gpt2\|tokenizer.json\|…>`. |
| `TEMPLATE` / `CHATTEMPLATE` | `<model>` | **Chat template** del modello (testo). Diagnostica "template errato = output diverso a parità di grafo". |
| `TEMPLATETEST` / `APPLYTEMPLATE` | `<model>, <role:content\|role:content…>` | **Rende** il template (Jinja) su un transcript → prompt formattato (equiv. `apply_chat_template`). |

#### Funzione — `LLM.TOKENCOUNT(<model>, <testo>)`

IMPORT LLM
tok$ = "../../samples/Fixtures/ai/tiny_tokenizer.json"
tpl$ = "../../samples/Fixtures/models/tiny_chat"
LLM TOKENIZER  tok$                                   ' vocab_size=259 source=gpt2
LLM VOCAB      tok$                                   ' vocab_size=259
LLM VOCAB      tok$, "he"                             ' vocab_size=259 + match del filtro
LLM TOKENIZE   tok$, "hello world"                    ' n=11 ids=74,71,78,78,81,223,89,81,84,78,70
LLM DETOKENIZE tok$, "74,71,78,78,81,223,89,81,84,78,70"   ' n=11 text=hello world
n = LLM.TOKENCOUNT(tok$, "hello world")               ' n = 11
LLM TEMPLATE     tpl$                                 ' il chat template del modello
LLM TEMPLATETEST tpl$, "system:You are helpful|user:Hi" ' prompt formattato


### `LLM REGISTRY` — registro / versioning dei modelli (B3, 2026-10-08)
| op | parametri | descrizione |
|---|---|---|
| `"CLEAR"` | — | azzera il registro. |
| `"ADD"` | `<path>, <tag>` | registra/aggiorna una versione. `tag` **obbligatorio**; se esiste, aggiorna il `path`. Annota **automaticamente** `size=<byte>` se il file esiste. |
| `"MEASURE"` | `<tag>[, <prompt\|dataset>]` | metriche **automatiche**: `size` sempre; con un prompt/dataset misura la **PPL** via `LLM QUALITY` (`ppl=<v>`, oppure `ppl=na` se il modello non è misurabile — mai silenzioso). |
| `"TAG"` | `<tag>, <k=v[;k=v…]>` | imposta/aggiorna le **metriche** del tag (semicolon-separated). |
| `"PROMOTE"` | `<tag>` | marca il tag come **corrente** (azzera gli altri). |
| `"LIST"` | — | elenca le versioni in ordine di registrazione → `n=<K> tags=<t1,t2,…>`. |
| `"SHOW"` | `<tag>` | dettaglio di una versione → `path=… promoted=<0\|1> metrics=…`. |
| `"REMOVE"` | `<tag>` | rimuove una versione (le successive slittano). |

IMPORT LLM
LLM REGISTRY "CLEAR"
LLM REGISTRY "ADD",     "models/qwen4-exp-q4.gguf", "v3-q4"
LLM REGISTRY "TAG",     "v3-q4", "ppl=25.9;size_gb=3.9"
LLM REGISTRY "PROMOTE", "v3-q4"
LLM REGISTRY "LIST"                                  ' n=1 tags=v3-q4
LLM REGISTRY "SHOW",    "v3-q4"                      ' path=… promoted=1 metrics=ppl=25.9;size_gb=3.9
LLM REGISTRY "MEASURE", "v3-q4"                      ' metrics=size=4135972864  (size automatica)
LLM REGISTRY "MEASURE", "v3-q4", "dataset.txt"       ' metrics=size=…;ppl=25.9 (PPL via LLM QUALITY)


### `LLM CALIBRATE` — calibrazione / imatrix (B4, 2026-10-08)
| parametro | tipo | descrizione |
|---|---|---|
| `<model>` | stringa | Modello **MoE GGUF caricabile** dal forward di calibrazione (es. `olmoe` F16). |
| `<dataset>` | stringa | Se termina in **`.bin`** = token **int32 grezzi** (nessun tokenizer); altrimenti un **file di testo**, tokenizzato col tokenizer del modello (`llmtoken`, fonte unica). |
| `<out.imatrix>` | stringa (opz.) | Path dell'imatrix di output. Default `work/model.imatrix.gguf`. |

LLM POLICY SET "IMATRIX", "work/calib.imatrix.gguf"     ' la quant pesata la usa per ruolo
LLM QUANT "modello_f16.gguf", "compresso.gguf"          ' esperti guidati dall'importanza reale


#### `LLM ACTIVATIONSTATS <model>, <dataset>` (alias di `LLM TENSOR ACTIVATIONSTATS`)

#### `LLM QUANTERROR <ref>, <cand>` (alias di `LLM TENSOR QUANTERROR`)

#### Chiavi di policy di prima classe: `ACTIVATION`

IMPORT LLM
LLM QUANTERROR "models/ref_f16.gguf", "models/q4.gguf"   ' rel_l2 + max_abs (ref vs cand)
LLM POLICY SET "ACTIVATION", "fp8"                       ' asse activation del QuantIR
LLM TENSOR QUANTIR "models/ref_f16.gguf"                 ' mostra a=fp8 per i tensori


### `LLM DERIVE` — compilatore RIPRODUCIBILE / ricetta `.gmbderive` (B5, 2026-10-08)
| istruzione | significato |
|---|---|
| `BASE <path>` | modello di partenza (**relativo alla dir della ricetta**; assoluto ammesso) |
| `POLICY <KEY> <VALUE>` | imposta una chiave di policy (es. `ROUTED_EXPERT iq2_xxs`, `PRUNE_EXPERT imp`) |
| `PRUNE <keep>` | `LLM PRUNE` (prune esperti, criterio da `POLICY PRUNE_EXPERT`) |
| `QUANT [<bpw>]` | `LLM QUANT` (policy per-ruolo) oppure **auto-bpw** se `<bpw>` |
| `ABLATE <pattern>` | `LLM MODEL ABLATE` |
| `ADAPTER <safetensors> [<alpha>]` | `LLM MODEL ADAPTER` (LoRA) |
| `OUT <path>` | output finale (**relativo alla CWD**) |

IMPORT LLM
LLM DERIVE "../../Fixtures/models/tiny.gmbderive"
' .gmbderive:
'   BASE tiny_moe.gguf
'   POLICY PRUNE_EXPERT top
'   PRUNE 2
'   OUT work/derived.gguf


LLM REGISTRY "ADD", "work/derived.gguf", "v2-pruned"
LLM REGISTRY "TAG", "v2-pruned", "recipe=tiny.gmbderive;ppl=24.1;experts=2"
LLM REGISTRY "SHOW", "v2-pruned"   ' mostra path + promoted + metriche (ricetta inclusa)


#### `LLM MAKEPATCH` / `LLM APPLYPATCH` — delta binario applicabile (B5)
| comando | parametri | significato |
|---|---|---|
| `LLM MAKEPATCH` | `<base>, <target>, <patch>` | crea il delta `.gmbpatch` (i tensori cambiati/nuovi del target). |
| `LLM APPLYPATCH` | `<base>, <patch>, <out>` | applica: `base + patch → out` (overwrite/add by-name = `LLM MODEL ADD`). |

IMPORT LLM
LLM DERIVE "../../Fixtures/models/tiny_ablate.gmbderive"        ' target = ABLATE(ffn_down)
LLM MAKEPATCH "../../Fixtures/models/tiny_moe.gguf", "work/target.gguf", "work/delta.gmbpatch"
LLM APPLYPATCH "../../Fixtures/models/tiny_moe.gguf", "work/delta.gmbpatch", "work/rebuilt.gguf"
LLM TENSOR QUANTERROR "work/rebuilt.gguf", "work/target.gguf"   ' rebuilt == target -> rel_l2=0


### Tool di misura (riga di comando, non BASIC)

## 3quater. Residenza e streaming out-of-core — `LLM MODEL` (LOAD/UNLOAD/CACHE/PREFETCH/DEVICE/PROMOTE/DEMOTE) e `LLM.STREAMSTATS`
| Comando | Sintassi | Parametri | Cosa fa |
|---|---|---|---|
| LOAD | `LLM MODEL LOAD <nome>, <path.gguf> [, RESIDENT]` | `nome` = handle (stringa); `path.gguf` = file GGUF MoE; clausola opzionale `RESIDENT` = tutto in RAM | Carica il modello e lo registra con `<nome>`. Path inesistente/formato non valido → **errore catturabile**. |
| UNLOAD | `LLM MODEL UNLOAD <nome>` | `nome` | Scarica il modello e libera la sua memoria. `<nome>` non caricato → **errore catturabile**. |
| CACHE | `LLM MODEL CACHE <nome>, <MB>` | `MB` = budget della expert cache in megabyte (`0` = disabilita) | Imposta il budget della cache degli esperti (SWAP). In RESIDENT la cache è fissa → errore. |
| PREFETCH | `LLM MODEL PREFETCH <nome>, <topn>` | `topn` = esperti da prefetchare per layer (`0` = off) | Attiva/dimensiona il prefetch speculativo cross-layer (usa l'I/O asincrono). |
| DEVICE | `LLM MODEL DEVICE <nome>, <MODE>` | `MODE` = `AUTO` \| `CPU` \| `METAL` | Seleziona il device di calcolo. `METAL` non disponibile → **errore esplicito**. |
| PROMOTE | `LLM MODEL PROMOTE <nome>` | `nome` | Porta la residenza ad **ACTIVE**: precarica **tutti** gli esperti in RAM. |
| DEMOTE | `LLM MODEL DEMOTE <nome>` | `nome` | Riporta la residenza a **WARM**: esperti on-demand da disco (libera la cache). |
| STREAMSTATS | `LLM.STREAMSTATS(<nome>)` | `nome` (tra parentesi: è una **funzione**) | Ritorna una **stringa JSON** con le statistiche di streaming/residenza. |
| HOTLIST | `LLM MODEL HOTLIST "<file>"` | `<file>` = hotlist `layer expert [count]` | **Selezione esperti** (MLX): gli esperti elencati restano **pinned** nella cache LRU (mai evitti). Zucchero per `AI SET OPTION "moe_hotlist", "<file>"`. Default: `<dir>/hotlist.txt`. |
| PROFILE | `LLM MODEL PROFILE "<file>"` | `<file>` = path di output | **Profilo routing** (MLX): al termine del modello l’engine scrive la hotlist osservata, riutilizzabile come `moe_hotlist`. Zucchero per `AI SET OPTION "moe_profile", "<file>"`. |
| Campo | Significato |
|---|---|
| `ok` | `true` se il modello è caricato |
| `model` | handle del modello |
| `mode` | `swap` \| `resident` |
| `device` | `auto` \| `cpu` \| `metal` |
| `layers`, `experts` | numero di layer e di esperti per layer |
| `bytesRead` | byte letti da disco nella sessione (rilevante in SWAP) |
| `expertBytes` | byte totali degli esperti |
| `residentBytes` | byte di esperti attualmente in cache |
| `cacheHits`, `cacheMisses`, `cacheEvictions`, `cacheHitPct` | metriche della cache LRU |
| `prefetchPredictions`, `prefetchHits`, `prefetchWasted`, `prefetchRecallPct` | metriche del prefetch |
| `residency` | `warm` (esperti on-demand) \| `active` (tutti residenti) |

## 3sexies. Fine-tune LoRA — `LLM FINETUNE` / `LLM LORA APPLY` (4 backend)

### `LLM FINETUNE <model> [, <opts>]` — comando
| Parametro | Tipo | Default | Descrizione |
|---|---|---|---|
| `<model>` | stringa | — | Cartella del modello MLX locale (deve contenere `config.json` + safetensors). Inesistente → errore catturabile. |
| `<opts>` | dict `{…}` o stringa JSON | `{}` | Opzioni di training (tabella sotto). Le chiavi sconosciute sono **ignorate**. |
| Chiave | Tipo | Default | Descrizione |
|---|---|---|---|
| `dataset` | stringa | `work/rule.train.bin` | File `.bin` del dataset di training (finestre di token del tokenizer del modello). |
| `holdout` | stringa | `work/rule.hold.bin` | File `.bin` del dataset di holdout (valutazione). |
| `ckpt` | stringa | `work/ft` | Prefisso del checkpoint: salva `<ckpt>_best_{A,B}.raw` + report. |
| `rank` | intero | `4` | Rango LoRA. |
| `layer` | intero | `15` | Layer target dell'adapter (auto-detect del modulo: `v_proj` → `kv_b_proj` → `q_proj` → …). |
| `steps` | intero | `60` | Passi di ottimizzazione. |
| `batch` | intero | `4` | Dimensione del batch. |
| `lr` | double | `0.01` | Learning rate. |
| `seed` | intero | `7` | Seme (riproducibilità). |

### `LLM LORA APPLY <ckpt> [, <opts>]` — comando
| Parametro | Tipo | Default | Descrizione |
|---|---|---|---|
| `<ckpt>` | stringa | — | **Prefisso** del checkpoint (es. `work/ft1`), NON il path `_best`. |
| `<opts>` | dict `{…}` o stringa JSON | `{}` | `model` e `dataset` (sotto). Entrambi obbligatori. |
| Chiave | Tipo | Obbligatoria | Descrizione |
|---|---|---|---|
| `model` | stringa | **sì** | Cartella del modello MLX (lo stesso usato per il fine-tune). Mancante → errore. |
| `dataset` | stringa | **sì** | File `.bin` per il prompt demo della verifica. Mancante → errore. |

### Funzioni di stato (parentesi, leggono l'ultima run)
| Funzione | Ritorna | Stato iniziale | Dopo errore |
|---|---|---|---|
| `LLM.FINETUNEDONE()` | `-1` se l'ultima run è terminata con successo, `0` altrimenti | `0` | `0` |
| `LLM.FINETUNELOSS()` | loss media train | `-1` | `-1` |
| `LLM.FINETUNEHOLD()` | loss holdout | `-1` | `-1` |
| `LLM.FINETUNEBASETRAIN()` | loss train del baseline (pre-fine-tune) | `-1` | `-1` |
| `LLM.FINETUNEBASEHOLD()` | loss holdout del baseline | `-1` | `-1` |
| `LLM.FINETUNEGREEDY()` | token greedy prima del training (negativo = n/d) | `-1` | `-1` |
| `LLM.FINETUNEGREEDYAFTER()` | token greedy dopo il training (negativo = n/d) | `-1` | `-1` |
| `LLM.FINETUNEDEMOCHANGED()` | `1` se la demo cambia dopo il fine-tune, `0` altrimenti | `0` | `0` |
| `LLM.FINETUNECKPT()` | path del checkpoint salvato (`""` se nessuna run ok) | `""` | `""` |
| `LLM.FINETUNESTATUS()` | stringa di stato: `"idle"` / `"running"` / `"ok"` / `"error: …"` | `"idle"` | `"error: …"` |

IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM FINETUNE "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit", { "dataset": "work/rule.train.bin", "holdout": "work/rule.hold.bin", "ckpt": "work/ft1", "rank": 4, "layer": 6, "steps": 40, "batch": 2, "lr": 0.02, "seed": 7 }
  IF LLM.FINETUNEDONE() = -1 THEN
    PRINT "loss:"; LLM.FINETUNELOSS(); " (baseline:"; LLM.FINETUNEBASETRAIN(); ") hold:"; LLM.FINETUNEHOLD()
    PRINT "greedy base:"; LLM.FINETUNEGREEDY(); " after:"; LLM.FINETUNEGREEDYAFTER(); " changed:"; LLM.FINETUNEDEMOCHANGED()
    PRINT "ckpt:"; LLM.FINETUNECKPT()
  ELSE
    PRINT "errore:"; LLM.FINETUNESTATUS()
  END IF
  LLM LORA APPLY "work/ft1", { "model": "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit", "dataset": "work/rule.train.bin" }
  PRINT "apply:"; LLM.FINETUNESTATUS()
  GOTO D
H:
  PRINT "caught:"; LLM.FINETUNESTATUS()


## 3septies. Registry di moduli esterni — `GMB MODULE` (4 backend)
| Comando | Parametri | Cosa fa |
|---|---|---|
| `GMB MODULE REGISTER "<slot>", "<dylib>"` | slot (stringa), path dylib | Registra uno slot → dylib (dlopen subito); ri-registrare lo stesso slot+dylib aggiorna (priorità 0). |
| `GMB MODULE PRIORITY "<slot>", <n>` | slot, intero | Imposta la **priorità** dello slot. A pari priorità vince l'**ultima** registrata. |
| `GMB MODULE LIST` | — | Elenca i moduli: una riga per modulo `slot<TAB>dylib<TAB>priority`. |
| `GMB MODULE SYM "<slot>", "<sym>"` | slot, simbolo | Risolve il simbolo dalla dylib **vincente** dello slot (errore se non risolto). |
| `GMB MODULE CALL "<slot>", "<sym>\|<sig>\|<args>"` | slot, spec chiamata | Chiama l'export: `sig` = prima lettera **ritorno**, poi i **parametri** (`i`=int64, `d`=double, `s`=stringa); `args` separati da `,`. Stampa il valore ritornato. |

GMB MODULE REGISTER "math.slot", "work/mymath.dylib"
GMB MODULE PRIORITY "math.slot", 10
GMB MODULE LIST
GMB MODULE CALL "math.slot", "mymath_Add|iii|2,3"    ' stampa 5


## 4. Esempio completo

## 5. Nota

## 6. Come gestire un modello NUOVO (modularità)

### Livello 1 — struttura nota (misure/nomi/quant diversi): **solo una `.gmbarch`**

### Livello 2 — FORMATO numerico nuovo (dtype mai visto): **una dylib "codec"**

### Livello 3 — struttura nuova = **COMPOSIZIONE di primitive**: solo `.gmbarch`

"structures": {
  "attention": {
    "type": "mla",
    "steps": [
      { "op": "q_lora",        "prim": "mlx_attn_q_lora" },   // solo la primitiva
      { "op": "rope_compress", "prim": "mlx_rope_compress" },
      { "op": "sdpa",          "prim": "" }                    // "" = built-in
    ]
  }
}


### Livello 4 (raro) — primitiva davvero NUOVA (algoritmo mai visto): **una dylib "primitiva"**

## Parametri comuni (tutti i comandi `LLM TENSOR`/`LLM MODEL`)

### AI-Distill-Guida-Utente

*Fonte: `AI-Distill-Guida-Utente`*


## 1. Pipeline — i 6 comandi

AI DISTILL BUILD   "pairs.jsonl", { "model": dir, "out": "work/ds", "window": 16, "holdout": 0.2, "max": 40 }
AI DISTILL TRAIN   "work/ds.train.bin", { "model": dir, "holdout": "work/ds.hold.bin", "ckpt": "work/ck", "frozen": "all-but-head", "rank": 4, "steps": 20 }
AI DISTILL SAMPLE  "prompts.jsonl", { "model": dir, "out": "work/pairs.jsonl", "k": 2, "temp": 0.7, "maxlen": 48, "verify": "rule" }
AI DISTILL EVAL    "prompts.jsonl", { "model": dir, "ckpt": "work/ck", "out": "work/eval.json", "repeat": 2, "verify": "rule" }
AI DISTILL APPLY   "work/ck", { "model": dir, "dataset": "work/ds.train.bin", "backup": "work/head.orig" }
AI DISTILL RESTORE "work/head.orig"

| Comando | Fa | Legge |
|---|---|---|
| `BUILD` | Trasforma un `.jsonl` di coppie (prompt,risposta) in dataset `.bin` a finestre | tokenizer in `<model>/tokenizer.json` o `tokenizer` esplicito |
| `TRAIN` | Addestra l'adapter lm_head-only; salva `<ckpt>_best_{A,B}.raw` + `.meta` + `.json` | dataset `.bin` di BUILD |
| `SAMPLE` | Genera `k` risposte per prompt a `temp`, filtra con VERIFY, scrive le coppie accettate | modello + tokenizer |
| `EVAL` | Valutazione paired: idx 0 = base, idx 1 = modello+adapter | modello (+ adapter `ckpt`) |
| `APPLY` | Verifica l'adapter sul modello e (se `backup`) salva i file del checkpoint | adapter + modello |
| `RESTORE` | Ricopia il checkpoint dal backup all'origine (registrata in `<backup>.src`) | backup |

### 1.1 Parametri per comando
| Comando | Chiave | Tipo | Default | Significato |
|---|---|---|---|---|
| BUILD | `model` | string | — | dir del modello (da cui `tokenizer.json`) |
| BUILD | `tokenizer` | string | `<model>/tokenizer.json` | path esplicito del tokenizer |
| BUILD | `out` | string | `<jsonl senza .jsonl>` | prefisso di output (`<out>.train.bin` / `.hold.bin`) |
| BUILD | `window` | int | 8 | lunghezza T della finestra di contesto |
| BUILD | `holdout` | double | 0.1 | quota di sample nel file hold |
| BUILD | `max` | int | 0 (tutte) | numero massimo di coppie |
| TRAIN | `model` | string | — | dir del modello |
| TRAIN | `holdout` | string | da `<dataset>` `.train.bin`→`.hold.bin` | file hold `.bin` |
| TRAIN | `ckpt` | string | `<dataset>_ckpt` | prefisso checkpoint |
| TRAIN | `frozen` | string | `"all-but-head"` | congela tutto tranne l'head |
| TRAIN | `rank` | int | 4 | rango dell'adapter head (1–64) |
| TRAIN | `layer` | int | 15 | (solo LoRA v_proj storico; head usa −1) |
| TRAIN | `steps` | int | 60 | passi SPSA |
| TRAIN | `batch` | int | 4 | sample per passo |
| TRAIN | `lr` | double | 0.01 | learning rate AdamW |
| TRAIN | `seed` | int | 7 | seed (riproducibilità) |
| SAMPLE | `model` | string | — | dir del modello |
| SAMPLE | `out` | string | `<prompts senza .jsonl>.distill.jsonl` | jsonl delle coppie accettate |
| SAMPLE | `k` | int | 4 | risposte generate per prompt (1–64) |
| SAMPLE | `temp` | double | 0.7 | temperatura di sampling |
| SAMPLE | `maxlen` | int | 256 | token massimi per risposta (1–4096) |
| SAMPLE | `verify` | string | `""` | `"rule"` = risposta non vuota e, se il prompt ha `answer`/`solution`, lo contiene. `"code"` non è supportato in nativo (errore esplicito) |
| SAMPLE | `seed` | int | 20260909 | seed |
| EVAL | `model` | string | — | dir del modello |
| EVAL | `ckpt` | string | `""` | prefisso adapter (se assente: solo base) |
| EVAL | `out` | string | `work/eval_distill.json` | report JSON delle metriche |
| EVAL | `repeat` | int | 3 | generazioni per prompt (1–16) |
| EVAL | `maxlen` | int | 256 | token massimi |
| EVAL | `temp` | double | 0.7 | temperatura |
| EVAL | `verify` | string | `""` | come SAMPLE |
| EVAL | `seed` | int | 20260909 | seed |
| APPLY | `model` | string | — | dir del modello |
| APPLY | `dataset` | string | `""` | dataset `.bin` per la demo OFF/ON |
| APPLY | `backup` | string | `""` | se dato, salva i file del checkpoint in `<backup>_{A,B}.raw`, `<backup>.meta`, `<backup>.src` |
| APPLY | `ngen` | int | 6 | token generati nella demo OFF/ON |
| RESTORE | — | string | — | unico argomento: prefisso del backup (usa `<backup>.src`) |

## 2. Funzioni di stato `LLM.DISTILL*()`
| Funzione | Ritorna | Dopo |
|---|---|---|
| `LLM.DISTILLSTATUS()` | string JSON: `{"pairs","train","hold","samplen","samplefail","error"}` | BUILD |
| `LLM.DISTILLSAMPLEN()` | double — coppie accettate | SAMPLE |
| `LLM.DISTILLSAMPLEFAIL()` | double — risposte scartate dal filtro | SAMPLE |
| `LLM.DISTILLSAMPLEDONE()` | double — `1` se l'ultimo SAMPLE è completato | SAMPLE |
| `LLM.DISTILLLOSS()` | double — loss train (`-1` = n/d) | TRAIN |
| `LLM.DISTILLHOLD()` | double — loss holdout (`-1` = n/d) | TRAIN |
| `LLM.DISTILLCKPT()` | string — path del checkpoint | TRAIN |
| `LLM.DISTILLDONE()` | double — `1` se l'ultimo TRAIN è completato | TRAIN |
| `LLM.DISTILLEVALTOK(idx)` | double — token mediani della config `idx` (`0`=base, `1`=adapter; `-1`=n/d) | EVAL |
| `LLM.DISTILLEVALPASS(idx)` | double — pass-rate VERIFY della config `idx` | EVAL |
| `LLM.DISTILLEVALACCEPT(idx)` | double — acceptance rate spec-decode (drafter n-gram) della config `idx`; `-1` se non misurabile | EVAL |

## 3. Esempio completo (pipeline)

IMPORT AI
IMPORT LLM
IMPORT IO

SUB Main()
    DIM model AS STRING = "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit"
    IF NOT IO.FILEEXISTS("/Users/gaetano/ai/Models/.gmbasic-allow-load") THEN PRINT "modello non caricato"; END

    ' 1) dataset
    AI DISTILL BUILD "../../Fixtures/ai/gsm8k_300.jsonl", { "model": model, "out": "work/ds", "window": 16, "holdout": 0.2, "max": 40 }
    PRINT LLM.DISTILLSTATUS()

    ' 2) training head-only
    AI DISTILL TRAIN "work/ds.train.bin", { "model": model, "holdout": "work/ds.hold.bin", "ckpt": "work/ck", "frozen": "all-but-head", "rank": 4, "steps": 20 }
    PRINT "done="; LLM.DISTILLDONE(); " loss="; LLM.DISTILLLOSS(); " hold="; LLM.DISTILLHOLD(); " ckpt="; LLM.DISTILLCKPT()



## 4. Note operative

## 5. Riferimenti

### Embedded-LLM-Infrastruttura-Guida

*Fonte: `Embedded-LLM-Infrastruttura-Guida`*


## 1. Architettura — dove gira l'LLM
| Backend | Libreria | Uso | In-process? | Matrice |
|---|---|---|---|---|
| **MLX** | `LLMEnv/LLMEngine/mlx/` (motore C hand-written) | LLM locali Apple Silicon (`.safetensors` MLX) | ✅ sì (via `libgmb_ai_mlx.dylib`) | macOS Apple Silicon |
| **llama.cpp** | `LLMEnv/LLMEngine/llama/` (wrapper C su llama.cpp) | Modelli GGUF, OCR (PaddleOCR-VL), audio (Parakeet), visione (Qwen2.5-Omni) | ✅ sì | macOS, Linux (CUDA/ROCm/Vulkan), CPU |

## 2. Il motore MLX nativo — struttura

### 2.1 Prefill vs decode
| Fase | Funzione | n_tokens | Descrizione |
|---|---|---|---|
| **Prefill** | `forward_tokens_batch` | >1 (chunk 2048/4096) | elabora il prompt in batch, un solo grafo per chunk |
| **Decode** | `forward_one_token` | 1 | genera un token alla volta (autoregressivo) |

## 3. GDN / linear-attention (Qwen3.5, Qwen3.8, Qwen3-Coder-Next)

### 3.0b Qwen3-Coder-Next (`qwen3_next`) — FATTO 2026-09-02 (`03f1b765`)

### 2.2 UPGRADE MLX vendored 0.31.1 -> 0.32.0 (2026-09-02)

## 3. GDN / linear-attention (Qwen3.5, Qwen3.8)

### 3.1 Blocco GDN (`gdn_attention_block`)

h = input_layernorm(x)                      # [B,S,5120]
qkv = in_proj_qkv(h);  z = in_proj_z(h)     # proiezioni (quantizzate)
conv_out = silu(conv1d(concat([conv_state,qkv])))   # depthwise, groups=conv_dim
q,k,v = split(conv_out)                     # reshape a teste
q = inv^2 * rms_norm(q);  k = inv * rms_norm(k)
beta = sigmoid(b);  g = exp(-exp(A_log)*softplus(a+dt_bias))
for t:  # ricorrenza (stato S [Hv,Dv,Dk] costante)
    state = state*g + k⊗(beta*(v - state@k))
    y_t   = state·q
out = out_proj(RMSNormGated(y, z))          # RMSNormGated = silu(z)*rms_norm(y)
h   = x + out                               # residual


#### 3.1.1 Kernel Metal fuso per il prefill GDN (2026-09-01)

### 3.2 Layer full con output gate (`qwen3_next_attention_block`)

## 4. Speculative / accelerazione decode

## 4bis. Continuous batching (multi-sessione) — Fase 3

## 5. Prefix cache e gestione memoria

### ⚠️ 5.1 REGOLA MEMORIA — test con LLM locali reali (obbligatoria)

### 5.2 Moduli low-level LMM ABI (piano `AI-LongContext-Modular-Plan.md` §8 — implementati 2026-09-01)
| Modulo | Funzioni | Scopo | Verifica |
|---|---|---|---|
| **Cache ABI** (§8.4) | `llm_mlx_cache_checkpoint/rollback/commit/trim` | base per speculative su hybrid (ripristino KV + stato GDN) | harness C puro |
| **Model Inspector** (§8.1) | `llm_ai_mlx_model_manifest`, `_cache_type`, `_layer_type` | descrizione strutturale (manifest) per i plugin | Qwen3.6-27B: HYBRID, 48 GDN+16 full |
| **N-Gram** (§8.10) | `llm_mlx_ngram_*` (tabella) + `llm_ai_mlx_session_build_ngram`, `_ngram_predict`, `_generate_ngram` | speculative self-consistente (draft batch + verifica + rollback) | SAME vs greedy |
| **Prefix cache nativa** (§3.1) | `llm_ai_mlx_model_save_prefix`, `llm_ai_mlx_session_append_reuse_prefix` | riuso del prefill di un prefisso (system/RAG) tra sessioni | SAME vs prefill completo |
| **KV quant** (E3, commit `eeb17397`) | `llm_ai_mlx_model_set_kv_quantize`, cache `kvq_*` per-layer in `layer_forward` | risparmio memoria a contesto lungo: parte COLD della cache mantenuta quantizzata (Q8/Q4), recenti FP16; dequantizza per la SDPA al volo | prefill 4K soglia 512 Q8 = append 0, greedy 113 114 |

## 6. Stato attuale e gap noti (2026-09-01)
| Componente | Stato | Note |
|---|---|---|
| Prefill MLX batched+chunked | ✅ | corretto su Qwen3.8-27B, parità layer 0-7 vs mlx-lm |
| GDN linear-attention (ricorrenza) | ✅ | `gdn_attention_block` + **kernel Metal fuso prefill** (`llm_mlx_gdn_kernel.h`, commit `74127729`) ~2-4x |
| Full-attn output gate (qwen3_5) | ✅ | `qwen3_next_attention_block` implementato e verificato |
| Prefill target 27B end-to-end | ✅ | `Qwen38TargetSmokeTests` PASS |
| **Decode GDN persistente** | ✅ | **fatto (§13.5, commit `ae423ed5`)**: stato ricorrente riusato tra i passi di decode (`k_cache`/`v_cache`/`has_cache` dei layer lineari). Decode 27B corretto: prompt raw → `\n\nParis` (parità esatta con mlx-lm). |
| DFlash2 speculativo end-to-end | ⏳ | **sbloccato**: il target ora genera correttamente → si può validare draft+verify batch (T3) |
| Sampling configurabile (llamacpp+MLX) | ✅ | `gmb_ai_*_session_set_sampling` |
| N-Gram speculative (T2) | ✅ | drafter + engine + verify batch |
| **Cache ABI** (§8.4) | ✅ | `llm_mlx_cache_checkpoint/rollback/commit/trim` (base speculative hybrid) |
| **Model Inspector** (§8.1) | ✅ | `llm_ai_mlx_model_manifest/_cache_type/_layer_type` |
| **N-Gram draft batch** (§8.10) | ✅ | `llm_ai_mlx_session_generate_ngram` (SAME vs greedy) |
| **Prefix cache nativa** (§3.1) | ✅ | `llm_ai_mlx_model_save_prefix`/`append_reuse_prefix` (SAME vs full) |
| **KV Cache per il RAG** (§3.1/E7) | ⏳ | estendere la prefix cache ai chunk RAG (L4 retrieval→prefill, NTP) — riuso del prefill del contesto condiviso tra domande sullo stesso vault. Tracciato come T1 in `AI-Prefill-Acceleration-TODO (partial).md`. Base pronta: prefix cache nativa + radix-trie automatico. |
| **KV quant** (E3) | ✅ **fatto** (commit `eeb17397`) | risparmio memoria cache cold a contesto lungo (Q8/Q4, recenti FP16) |

### 6.1 Come procedere (prossimi passi)

## 7. Collegamenti

## 8. Qwen4-Exp / Flash-Next — fondamenti nativi (port in corso, 2026-09-02)

### 8.0 Inquadramento architetturale (per capire i gruppi sotto)

### 8.1 Gruppo A — Normalizzazione e gate (primitivi RMS)
| Funzione C | Classe reference | Tipo di modulo | Cosa fa nello specifico | Modello |
|---|---|---|---|---|
| `q4_zc_rmsnorm(res, x, w, eps, group_size, s)` | `ZeroCenteredRMSNorm` | primitiva di normalizzazione | `norm = x/sqrt(mean(x²)+eps)` in fp32, poi `·(1+w)`: il peso salvato è un **delta additivo da 1** (non il peso moltiplicativo classico). Supporta **group_size**: se attivo rishapa a `(…, n_groups, group)` e normalizza per gruppo (peso `(n_groups, group)`). Serve a `hc_norm` della hyper (gruppo = hidden) e alle q/k_norm zero-centered. | `qwen4_exp` |
| `q4_sigmoid_rmsnorm_gated(res, hidden, gate, w, eps, silu, s)` | `SigmoidRMSNormGated` | primitiva norm+gate | `rms_norm(hidden, w)` (peso ones) in fp32, gate = `sigmoid(gate)` (o silu) in fp32, output = `norm·gate` riportato alla dtype di `hidden`. È il **trasform di uscita "norm-before-gate"** dei blocchi GDN di Flash-Next (`output_gate_type=sigmoid`). | `qwen4_exp` |

### 8.2 Gruppo B — Hyper-connections (mixer residuale a stream)
| Funzione C | Classe reference | Tipo di modulo | Cosa fa nello specifico | Modello |
|---|---|---|---|---|
| `q4_linear_helper(res, x, w, s)` | (Linear) | utility matmul | `x @ w.T` **nativo** (nessun cast a fp32 del peso, promotion MLX come nel reference) — il matmul usato da tutti i moduli qwen4 per i pesi non quantizzati (mixer lowrank, out_proj). | `qwen4_exp` |
| `q4_gated_residual(m, x, w_dn, w_up, w_inj, norm_w, &mixed, &resid, &inj, s)` | `GatedResidual` | mixer hyper-connection (4-stream) | Su `x=[…, hc·H]`: (1) `hc_norm` = ZeroCentered **per gruppo hidden**; (2) `mix = sigmoid(up( silu(dn(norm)/hc) ))` → reshape `(…,hc,H)`; (3) `mixed = mean_{hc}(mix·streams)` (un solo vettore `[…,H]`); (4) se `use_combine` (`w_inj` presente): `residual = x` e `injection = 2·sigmoid(inj(norm)/hc)` `[…,hc]` — i tre output servono a `_combine` del layer. Se non `use_combine`: ritorna solo `mixed`. | `qwen4_exp` — per-layer `attn_hyper_connection`/`mlp_hyper_connection` (use_combine=true) e **mixer top-level** `hyper_connection_mixer` (use_combine=false) |

### 8.3 Gruppo C — Blocco lineare GatedDeltaNet (linear-attention ricorrente)
| Funzione C | Classe reference | Tipo di modulo | Cosa fa nello specifico | Modello |
|---|---|---|---|---|
| `qwen4_linear_gdn_block(m, layer, …)` | `Qwen4Exp…GatedDeltaNet` | blocco di sequence-mixing (ricorrente) | Proiezioni separate `in_proj_qkv/z/b/a` (come il gdn qwen3_5), `conv1d` depthwise + silu su `qkv`, split q/k/v, e poi le differenze qwen4: **niente input_layernorm** (il norm lo fa la hyper upstream); **L2-norm di q e k** (`x·rsqrt(sum(x²,last)+1e-6)`, eps dopo la somma — non mean) con `q·=inv_scale(Dk^-0.5)`; ricorrenza GDN (kernel o loop, riusata dal motore) con `A_log`/`dt_bias` e gate `g`/`β`; uscita `SigmoidRMSNormGated` (gate sigmoid su `z`); ritorna **solo `r = out_proj(...)`** (senza somma residuale: la fa `_combine` della hyper). Cache per layer: `conv_state` + stato `S` (come il gdn). | `qwen4_exp` — layer lineari (36/48) |

### 8.4 Come si collegano i gruppi (mappa dei flussi)

input [B,S,H]
  └─ embed → tile ×hc → [B,S,hc·H]
       └─ per ogni layer:
            attn_hyper_connection (Gruppo B: q4_gated_residual) → mixed [B,S,H]
                ├─ lineare → qwen4_linear_gdn_block (Gruppo C)      [36 layer]
                └─ full    → QSAAttention+indexer+ngram (St.4, GRUPPO FUTURO)  [12 layer]
            _combine(output, residual, injection)   (St.5)
            mlp_hyper_connection (Gruppo B) → MoE/MLP → _combine
       └─ hyper_connection_mixer top-level (Gruppo B, use_combine=false) → [B,S,H]
  └─ lm_head


### 8.5 Stato e prossimi stadi (riepilogo)
| Stadio | Contenuto | Stato |
|---|---|---|
| 1 | mappa + contract hyper | ✅ (doc, vault) |
| 2 | Gruppo A (primitivi RMS) — `70b694f7` | ✅ |
| 3a | Gruppo B (GatedResidual) — `365b2721` | ✅ |
| 3b | Gruppo C (blocco lineare qwen4) — `a6f0278e` | ✅ |
| 4 | QSA/N-gram + indexer + cache (nuovo gruppo, full attention) | ⏳ sessione dedicata |
| 5 | path hyper + mixer top-level + MoE + MTP + dispatch + RUN 97G | ⏳ |
| 6 | parità vs reference + benchmark + doc | ⏳ |

## 9. Catalogo componenti del motore MLX (tutti i modelli)

### 9.0 Come è strutturato il motore (in generale)

### 9.1 Dispatch e selezione d'architettura
| Simbolo | Tipo | Cosa fa | Modelli |
|---|---|---|---|
| `load_config` | parser config | Legge `config.json` (radice o `text_config` annidata per gemma4/qwen3_5/qwen4_exp), dims MoE, quant, rope, sliding, layer ibridi, campi dflash/linear | tutti |
| `ModelConfig` | struct di config | Parametri risolti a runtime per blocco (hidden, heads, kv, MoE, quant, GDN, DFlash2, kv-quant, NSA) | tutti |
| `ARCH_TABLE` + `find_arch_ops` | tabella di dispatch | `model_type → (attn_fn, ffn_fn)` | llama, qwen2, qwen3, qwen3_moe, qwen3_5, qwen3_5_moe, qwen3_next, gemma4, mixtral, gpt_oss, glm4_moe, qwen4_exp |
| `layer_forward` | esecutore layer | Per un layer: `attn(...)` poi `ffn(...)` con KV-cache; gestisce trim sliding e kv-quant cold | tutti |
| `forward_tokens_batch` / `forward_one_token` | forward | Batch (prefill) o singolo (decode) token su tutti i layer | tutti |
| `qwen3_5_attention_block` | dispatcher ibrido | layer lineare→`gdn_attention_block`; layer full→`qwen3_next_attention_block` (se output-gate) altrimenti `qwen3_moe_attention_block` | qwen3_5, qwen3_5_moe |
| `qwen3_next_attention_dispatch` | dispatcher ibrido | lineare→`qwen3_next_linear_block`; full→`qwen3_next_attention_block` | qwen3_next |

### 9.2 Attenzione (sequence-mixing classica)
| Simbolo | Tipo | Cosa fa | Modelli |
|---|---|---|---|
| `qwen3_moe_attention_block` | attention GQA standard | q/k/v/o proj + qk-norm (RMS su head) + rope + SDPA causale; usato anche dai layer full densi | qwen3, qwen3_moe, qwen3 full, qwen3_next full |
| `qwen2_attention_block` | attention GQA | come sopra senza qk-norm | qwen2 |
| `llama_attention_block` | attention | RoPE classica (+ variante llama3 a 3 fasce) | llama, DeepSeek-R1-Distill (qwen2) |
| `mixtral_attention_block` | attention GQA | q/k/v/o con sliding_window opzionale | mixtral |
| `gemma4_attention_block` | attention con layertype | dispatch per-layer sliding/full, k_eq_v, V-norm (no_scale), rope proporzionale | gemma4 |
| `qwen3_next_attention_block` | attention con **output gate** | q_proj produce 2× (queries+gate), `o_proj(sdpa·sigmoid(gate))`, qk-norm | qwen3_5 full, qwen3_next full |
| `gpt_oss_attention_block` | attention speciale | bias su q/k/v/o, **attention sinks** appresi, layer sliding alternati | gpt-oss |
| `glm4_moe_attention_block` | attention speciale | bias lineari, **partial rotary** (metà dims), qk-norm | glm4_moe |
| `llm_mlx_gqa_sdpa(_sinks)` | primitiva SDPA | fast SDPA causale o **a finestra** (mask host) con sinks opzionali | gemma4, gpt_oss |
| SDPA diretta (`mlx_fast_scaled_dot_product_attention` "") | primitiva | attenzione **non causale** (es. draft DFlash2) | DFlash2 |

### 9.3 Linear-attention / GatedDeltaNet (ricorrenza)
| Simbolo | Tipo | Cosa fa | Modelli |
|---|---|---|---|
| `gdn_attention_block` | blocco GDN | proiezioni separate `in_proj_qkv/z/a/b` + conv1d depthwise + silu + split q/k/v; rms-norm q/k con `inv_scale`; g=exp(-exp(A_log)·softplus(a+dt)), β=sigmoid(b); ricorrenza (kernel o loop); norm-gate silu(z); cache conv_state+state nei 2 slot | qwen3_5 (Qwen3.8/3.6), qwen3_5_moe |
| `llm_mlx_gdn_prefill` (kernel Metal) | kernel fuso | intera ricorrenza di un prefill in un kernel Metal (S>1); fallback al loop | qwen3_5, qwen3_5_moe |
| `llm_mlx_gated_delta_step` | step ricorrente | passo per-token della ricorrenza (modulo riusabile) | qwen3_5, qwen4_exp |
| `qwen3_next_linear_block` | blocco GDN (merged) | come gdn ma proiezioni **merged** `in_proj_qkvz`/`in_proj_ba` con split per-k-head (ordinamento reference) | qwen3_next |
| `qwen4_linear_gdn_block` | blocco GDN qwen4 (in corso) | proiezioni separate + **L2-norm** q/k (rsqrt(sum+ε)) + `q·inv_scale`, gate **sigmoid** (`SigmoidRMSNormGated`), no input-layernorm, no residual (combine upstream) | qwen4_exp (36 layer lineari) |

### 9.4 FFN denso e MoE
| Simbolo | Tipo | Cosa fa | Modelli |
|---|---|---|---|
| `dense_swiglu_ffn` | FFN denso | `h + down(silu(gate(x))·up(x))` | qwen2/3/3.5 densi, llama, qwen3_next? no; qwen3 full? no (vedi per-arch) |
| `dense_geglu_ffn` | FFN denso | GeGLU (gelu approssimato) per gemma4 (con pre/post norm e layer_scalar) | gemma4 |
| `qwen3_moe_ffn_block` | MoE | router argpartition top-k, softmax(norm_topk?), esperti (gather_qmm o per-esperto), somma | qwen3_moe |
| `qwen3_5_moe_ffn_block` | MoE + shared | switch_mlp esperti **stacked** (top-k via argpartition + take_along) + **shared_expert** gated | qwen3_5_moe (Qwen3.6-35B, Qwen3.5-122B), qwen3_next, qwen4_exp |
| `mixtral_ffn_block` | MoE | softmax sui top-k, esperti separati | mixtral |
| `gpt_oss_ffn_block` | MoE speciale | SwiGLU con clip `±7` e alpha 1.702, esperti con bias, mxfp4 per-tensore | gpt_oss |
| `glm4_moe_ffn_block` | MoE | primi N layer densi (`first_k_dense_replace`) + esperti con **shared** | glm4_moe |
| `dense_swiglu`/MLP nei layer MoE full | FFN | dense quando il layer non è MoE | qwen3_next/qwen4 (mlp_only…) |

### 9.5 Primitivi di normalizzazione, attivazione e algebra
| Simbolo | Tipo | Cosa fa | Modelli |
|---|---|---|---|
| `rms_norm_cfg` | norm | RMSNorm con cast bf16-aware (`g_bf16_all`) | tutti |
| `mlx_fast_rms_norm_no_scale` | norm | RMSNorm senza peso (per V-norm / q/k GDN) | gemma4, qwen3_5 |
| `q4_zc_rmsnorm` | norm zero-centered | peso = delta da 1, group_size opzionale | qwen4_exp |
| `q4_sigmoid_rmsnorm_gated` | norm+gate | rms×sigmoid(gate) fp32 | qwen4_exp |
| `mlx_fast_rope` | rope | RoPE con offset assoluto; freq da base o array esplicito | tutti |
| `llm_mlx_proportional_rope_freqs` | freq rope | frequenze proporzionali (layer full gemma4) | gemma4 |
| `llm_mlx_llama3_rope_freqs` | freq rope | RoPE llama3 a 3 fasce di frequenza | llama |
| partial rotary | rope parziale | ruota solo i primi `partial_rotary_factor·head_dim` | qwen3_5, glm4_moe |
| `quantized_linear_cfg` | matmul quantizzato | x·W^T quantizzato affine/mxfp4/…; instrada a `plain_linear` se plain | tutti (pesi quantizzati) |
| `llm_mlx_quant_derive` | derive per-tensore | da w/scales ricava (group,bits,mode) per tensore (affine 64/8, 4 g64, mxfp4 g32, 2-bit g128) | per-tensor quant (gpt-oss, Ternary, 122B) |
| `plain_linear` | matmul | x·W^T con cast a fp32 | modelli BF16/draft, pesi plain |
| `embed_tokens_batch`/`embed_token` | embedding | gather righe + dequant (bf16/fp32) + reshape | tutti |
| `mlx_sigmoid`/`mlx_multiply` ecc. | attivazioni | silu = x·sigmoid(x) | tutti |

### 9.6 KV-cache, memoria e long-context
| Simbolo | Tipo | Cosa fa | Modelli |
|---|---|---|---|
| `GmbMlxSession` k/v_cache | KV cache | array per layer, concat su asse sequenza | tutti |
| trim sliding in `layer_forward` | sliding window | scarta i token oltre la finestra per i layer sliding | gemma4, gpt_oss, DFlash2 |
| `cfg.kv_max_tokens` / rolling | rolling-window KV | trim della cache ai più recenti N | long-context (config) |
| kv-quant (Q8/Q4 cold) | quant KV | quantizza la parte fredda oltre soglia, dequant a inizio forward | config (qwen3_5…) |
| prefix-cache (save/restore/eviction LRU) | riuso prefill | riusa la KV di un system prompt ripetuto su file | tutti (config) |
| swap-out/in su disco | persistenza | salva/ripristina la sessione (KV+pending) su file | tutti |
| Cache ABI `llm_mlx_cache_{checkpoint,rollback,commit}` | rollback | snapshot intera KV per rollback del verify speculativo | speculative, DFlash2 |
| `llm_mlx_active_guard` / memory stats / limit | protezione | guardia rc=-2 oltre il tetto attivo; stats/limite allocatore | big-eval |
| `mlx_clear_cache` dopo chunk | memoria | libera il pool GPU tra chunk di prefill | prefill chunked |

### 9.7 Speculative decoding e decode avanzato
| Simbolo | Tipo | Cosa fa | Modelli |
|---|---|---|---|
| `NGramDrafter` / `SuffixDrafter` | drafter (C#) | propone token da tabella n-gram / suffissi del contesto | tutti (config speculation ngram/suffix) |
| `SpeculativeDecode.Generate` (C#) | loop speculativo | draft→verify→walk→commit con parità greedy | tutti |
| `llm_ai_mlx_session_verify_token` | verify 1-token | accetta il draft se = greedy corrente | tutti |
| `llm_ai_mlx_session_verify_draft_batch` | verify batch | forward dei K draft in un grafo, confronto, rollback parziale | tutti |
| `llm_ai_mlx_session_truncate` | rollback | tronca la KV a n token | MTP/draft |
| `llm_ai_mlx_session_greedy_prob` | prob | P(greedy\|ctx) per gate di confidenza | DSpark |
| `MtpSemiArDrafter` (C#) + session persistente | drafter MTP | drafter semi-autoregressivo su sessione draft sincronizzata | spec mtp |
| DFlash2 runtime (`llm_ai_mlx_dflash2_{create,propose,generate,debug}`) | drafter block-diffusion | forward nativo del draft DFlash2 (conv+selector+fc), bind embed/lm_head target, round-loop con capture hidden ai layer target | Qwen3.8-27B-DFlash2 + target 27B |
| `df2_convolve` / `df2_selector`/`DFlash2Selector` (C#) | conv/selector | two-tap conv + lattice top-k e greedy path | DFlash2 |
| `llm_ai_mlx_session_capture_hidden` / df2_capture_batch | capture | hidden del target per data-gen / ctx DFlash2 | data-gen, DFlash2 |
| constrained decode (`session_generate_constrained`) | tool decode | argmax ristretto agli allowed (tool call) | tool-calling |
| sampling (`session_set_sampling`) | campionamento | temperature/top_p/top_k/seed per sessione | tutti |
| LoRA (`llm_mlx_lora_apply_mlx`, adapter) | adapter | aggiunge contributo LoRA per layer/proiezione | tutti (fase 4) |
| imatrix (`llm_mlx_model_imatrix_*`) | collect | raccoglie attivazioni per quantizzazione selettiva | glm/draft (DS4) |

### 9.8 Integrazione e superficie (C#/BASIC)
| Componente | Tipo | Cosa fa |
|---|---|---|
| `MlxLlmCompleter` (C#) | provider | carica modello+draft, sessioni, chat template, sampling, speculative (ngram/mtp/dflash), prefix-cache |
| `llm_ai_mlx_load_model` / session API | C API | carica modello, crea/distrugge sessione, append/generate/verify/capture |
| config keys | surface | `modelDir`, `draftModelDir`, `speculation`, `temperature`, `kvQuantize`, `kvMaxTokens`, `nsaSparse`, `prefixCache`, `mlxMemoryLimitMB`, `prefillChunk`, `temperature…` |
| `AI REGISTER`/`AI SET ACTIVE` + agent | surface BASIC | registra un provider mlx con cfg (speculation=dflash etc.) e lo usa in chat |

## Navigazione canonica — AI embedded

### GMBasic-Modelli-AI-Locali

*Fonte: `GMBasic-Modelli-AI-Locali`*


## 1. AIPATH — la radice di ricerca dei modelli

// config/ai.json → "modelPaths"
"modelPaths": ["/Users/gaetano/ai/Models", "/Users/gaetano/ai/Models/gguf"]


## 2. REGISTER per nome — capability chat/ocr/audio

### Chat (LLM remoto via unified-gateway)

AI REGISTER "gw", %CHAT, "http://127.0.0.1:11435/v1", "qw3635"
AI SET ACTIVE %CHAT, "gw"
AI SET TEMP "gw", "0.2"                 ' config reale (persistita)
risposta$ = AI.AGENTASK("a1", "7*8?")   ' → "56"


### OCR (PaddleOCR-VL, GGUF + mmproj embedded)

AI REGISTER "ocr-local", %OCR, "PaddleOCR-VL-1.6-GGUF"
AI SET ACTIVE %OCR, "ocr-local"
testo$ = AI.OCR("screenshot.png")       ' → testo estratto (verificato: "85°F")


### OCR con anonimizzazione automatica (PII redaction)

' 3° argomento = TRUE → redige le PII (email, telefono, CF, P.IVA, carta, IBAN, date, IP, indirizzi)
testo$ = AI.OCR("contratto.png", "Extract all text.", TRUE)
' → "Contatto [EMAIL] tel [PHONE] CF [TAX_ID] P.IVA [VAT_ID] ..."


### Audio / trascrizione (Qwen2.5-Omni o Parakeet, GGUF + mmproj)

AI REGISTER "audio-local", %AUDIO, "qwen2.5-omni-3b-Q4_K_M"
AI SET ACTIVE %AUDIO, "audio-local"
trans$ = AI.TRANSCRIBE("audio.wav")     ' → trascrizione (verificata)


### Config utili per i GGUF embedded

AI REGISTER "ocr-local", %OCR, "PaddleOCR-VL-1.6-GGUF", "{""gpuLayers"":""0""}"
' gpuLayers: 0 = CPU (evita problemi Metal), default = GPU
' contextSize: default 8192


## 3. Capability e ruoli
| Costante | Ruolo | Uso |
|---|---|---|
| `%CHAT` (0) | completion | chat, agenti, tool-use |
| `%OCR` (1) | ocr | `AI.OCR(path [, prompt])` |
| `%AUDIO` (2) | audio | `AI.TRANSCRIBE(path)` |
| `%EMBEDDING` | embedding | RAG vettoriale (`AI.EMBED`) |

## 4. Layer config reale

## 4bis. Sampling configurabile per-sessione (P0, 2026-08-31)

AI REGISTER "local", %CHAT, "mlx", "{""modelDir"":""~/ai/Models/Llama-3.2-1B-Instruct-4bit"",""temperature"":""0.7"",""topP"":""0.9"",""topK"":""40"",""seed"":""42""}"
AI SET ACTIVE %CHAT, "local"

AI REGISTER "greedy", %CHAT, "llamacpp", "{""modelPath"":""...gguf"",""temperature"":""0""}"
' temperature=0 → greedy deterministico (argmax)


## 5. Test reali eseguiti (2026-08-19)
| Capability | Modello | Input | Output |
|---|---|---|---|
| Chat | `qw3635` (gateway 11435) | "7*8?" | "56" |
| Chat | `qw3635` | capitale Italia | "Roma" |
| OCR | `PaddleOCR-VL-1.6-GGUF` | screenshot | "85°F" |
| Audio | `qwen2.5-omni-3b-Q4_K_M` | test_audio.wav | trascrizione inglese |
| Sampling (2026-08-31) | `Llama-3.2-1B-Instruct-4bit` (MLX) | temp=0 → deterministico; temp=0.8+seed → riproducibile; seed diverso → output diverso | verificato in `AI.Tests` (`MlxProviderTests.Sampling_WhenModelEnvSet...`) |

## 6. L'architettura del modello è DENTRO GMBasic — la `.gmbarch` e la cartella `ABI/`

## Riferimenti

## Navigazione canonica — AI embedded

### KV-Quant-Hetero-KV-Guida-Utente

*Fonte: `KV-Quant-Hetero-KV-Guida-Utente`*


## 1. Come funziona (in breve)

## 2. Attivazione

### Config del modello (JSON)

{
  "kvQuantize": true,
  "kvQuantizeThreshold": 256,
  "kvQuantizeBits": 8
}


### Variabili d'ambiente (aggiuntive)

GMB_KV_DEQUANT_READ=1          # legge il cold col kernel (dequant-on-read). Default OFF.
GMB_KV_HETERO=K8V4             # Hetero-KV: K a 8 bit, V a 4 bit (K<bits>V<bits>, 2..8, V<K).


### Da BASIC

## 3. Scegliere i bit (memoria vs qualità)
| modalità | byte/token/layer | vs FP16 | note |
|---|---|---|---|
| FP16 | `4·nkv·hd` | — | nessuna compressione |
| K4 + V FP16 | `2,5·nkv·hd` | −37% | simmetrico storico (solo K) |
| **K8V4** | `1,5·nkv·hd` | −62% | **consigliato**: K preciso, V compresso |
| K4V4 | `1·nkv·hd` | −75% | massimo risparmio simmetrico |
| **K4V2** | `0,75·nkv·hd` | −81% | spinto; qualità da verificare |

## 4. Prestazioni attese
| contesto | READ vs BASE |
|---|---:|
| 2048 | **+4,8%** |
| 4096 | **+22,5%** |
| 8192 | **+27,3%** |

## 5. Quando usarla

## 6. Limiti e avvertenze

## 7. Diagnostica
| variabile | effetto |
|---|---|
| `GMB_KV_DEQUANT_READ=1` | abilita il kernel dequant-on-read |
| `GMB_KV_HETERO=K<bits>V<bits>` | Hetero-KV (es. `K8V4`) |
| `GMB_KVQ_VERBOSE=1` | stampa il kernel Metal generato |
| `GMB_KVQ_SYNC=1` | forza la valutazione (isola problemi di lazy eval) |
| `GMB_KVQ_MAN=1` | nel corebench, verifica vs softmax manuale f32 |
| `GMB_CB_KVBITS=K:V` | nel corebench, testa bit arbitrari |

test_mlx_bench <modelDir> decodectx 8192 32 kvq=256:8          # KV-quant, K8
GMB_KV_DEQUANT_READ=1 test_mlx_bench <modelDir> decodectx 8192 32 kvq=256:8
GMB_KV_HETERO=K8V4 GMB_KV_DEQUANT_READ=1 test_mlx_bench <modelDir> decodectx 8192 32 kvq=256:8


## 8. Riferimenti

### LLM-Server-Guida

*Fonte: `LLM-Server-Guida`*


## 1. Cosa sono (e perché due versioni)
| | **Server C** (nativo) | **Server C#** (managed) |
|---|---|---|
| Sorgente | `GMBasic/native/gmb_llm_server/gmb_llm_server.c` | `GMBasicSharp/common/AI/OpenAiServer.cs` (+ host `OpenAIServer/Program.cs`) |
| Parla con | engine C `libgmb_ai_mlx` + tokenizer BPE **direttamente** | provider gestito `ProviderFactory` → `MlxLlmCompleter` (P/Invoke) |
| Chat template | motore **Jinja C** (`LLMEnv/LLMJinja`) | Jinja del provider gestito |
| Avviato da | `AI SERVER START` (BASIC) **o** CLI | CLI (host `gmb_openai_server`) |
| Multi-sessione | job queue + scheduler (cap `GMB_MAX_ACTIVE`) | coda + scheduler (cap `GMB_MAX_ACTIVE`) |
| Continuous batching (Fase 3b) | ✅ `GMB_SERVER_BATCH` (default ON) | ✅ `GMB_SERVER_BATCH` (default ON) |

## 2. Uso dall'**utente** (riga di comando + client HTTP)

### 2a. Server C

# build (una volta)
cmake -S GMBasic/native/gmb_llm_server -B GMBasic/native/gmb_llm_server/build && cmake --build GMBasic/native/gmb_llm_server/build -j4
# avvio (porta default 8020)
GMBasic/native/gmb_llm_server/build/gmb_llm_server ~/ai/Models/<dir> [porta]

# variabili d'ambiente utili:
#   GMB_MAX_ACTIVE=4       richieste attive massime (default 2) -> oltre: 503
#   GMB_SERVER_BATCH=0     disabilita il continuous batching del motore (default ON)
#   GMB_PREFIX_MIN_SAVE_LEN, GMB_PREFIX_MIN_MATCH   prefix-cache automatico


### 2b. Server C#

dotnet build OpenAIServer -c Release
dotnet OpenAIServer/bin/Release/net10.0/gmb_openai_server.dll \
    --model ~/ai/Models/<dir> --port 8020 --temperature 0 [--top-p 0] [--top-k 0] [--seed 0] [--max-tokens 256]

# stesse env: GMB_MAX_ACTIVE (default 2), GMB_SERVER_BATCH (default ON)


### 2c. Client (qualsiasi cosa parli OpenAI)

curl http://127.0.0.1:8020/v1/models
curl http://127.0.0.1:8020/v1/chat/completions -H 'Content-Type: application/json' \
  -d '{"model":"m","messages":[{"role":"user","content":"Ciao"}],"max_tokens":64,"temperature":0}'


## 3. Uso da **BASIC**

IMPORT AI WITH ALIAS
AI SERVER START <modelDir$> [port]   ' avvia gmb_llm_server (default 8020) e attende readiness
AI SERVER STATUS                     ' stato live (running/pid/port/model/loaded/busy)
AI SERVER MODEL <modelDir$>          ' residenza: carica/switch del modello residente
AI SERVER UNLOAD                     ' residenza: libera il modello residente
AI SERVER STOP                       ' ferma il processo


IMPORT HTTP
IMPORT JSON
DIM co AS HTTPOPTIONS = NEW HTTPOPTIONS
co.Method = "POST"
co.Headers.ContentType = "application/json"
co.Body = "{""model"":""Llama-3.2-1B-Instruct-4bit"",""messages"":[{""role"":""user"",""content"":""Ciao""}]}"
DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8020/v1/chat/completions", co)
PRINT JSON.PARSE(r.Body)("choices")(0)("message")("content")


tools/sh/parity4 shared/minitests/ai/ai_server_errors.bas   # 4 backend (IL/PCode/VM-C/C99Native)
tools/sh/test_minitests_parity.sh ai_server             # oracolo IL/PCode


## 4. Continuous batching (Fase 3b) — cosa cambia per l'utente

## 5. Errori/limiti comuni

## 6. Collegamenti

## Throughput con continuous batching (misurato 2026-09-12)
| N sessioni | seq tot tok/s | batch tot tok/s | speedup |
|---|---|---|---|
| 4 | 490,1 | 806,0 | **1,64×** |
| 8 | 487,5 | 877,8 | **1,80×** |
| 16 | 482,5 | 898,9 | **1,86×** |

## Com'è integrato nell'engine (BASIC → batching)

BASIC: AI SERVER START <modelDir> [, <port>] [, <max_active>]
  ├─ mondo C   (VM/C gmb_builtin.c:14399, C99) → gmb_ai_server_json_start()
  │                                              → gmb_ai_server_start() → fork/exec gmb_llm_server
  └─ mondo C#  (IL) IlRuntime.AiServerStart()  → AiServerManager.Start() → spawn gmb_llm_server
                                                                              │
                          gmb_llm_server (processo esterno C) ── GMB_SERVER_BATCH=1 (default)
                                                                              │
                                            llm_ai_mlx_session_generate_batch()  (varlen + FFN batch)


---- VM/C OUTPUT ----
start_ok
stop_ok
  exit=0


### LLM-Server-OpenAI-Compatibile

*Fonte: `LLM-Server-OpenAI-Compatibile`*


## Perché due versioni

## Dove sono e come si avviano

### Versione C (core nativo)

Sorgente : GMBasic/native/gmb_llm_server/gmb_llm_server.c  (+ CMakeLists.txt)   [versionato]
Build    : cc -O2 -o gmb_llm_server gmb_llm_server.c -I ../LLMEngine/mlx -I ../LLMToken \
             -L ../LLMEngine/mlx/build -L ../LLMToken/build \
             -Wl,-rpath,../LLMEngine/mlx/build -Wl,-rpath,../LLMToken/build \
             -lgmb_ai_mlx -lLLMToken
Esegui   : ./gmb_llm_server ~/ai/Models/<dir> [porta]      (default porta 8020)


### Versione C# (core managed)

Sorgente : GMBasicSharp/common/AI/OpenAiServer.cs  (host: OpenAIServer/)               [versionato]
Esegui   : dotnet build OpenAIServer -c Release
            dotnet OpenAIServer/bin/Release/net10.0/gmb_openai_server.dll \
                --model ~/ai/Models/<dir> --port 8020 --temperature 0


## Endpoint (superficie attuale 2026-09-07)
| Endpoint | C | C# | Note |
|---|---|---|---|
| `GET /v1/models` | ✅ | ✅ | elenco modello **residente** (vuoto se nessuno) |
| `GET /v1/status` | ✅ | ✅ | health + `loaded` + `busy` (busy = coda job attiva) |
| `POST /v1/completions` | ✅ | ✅ | prompt testo o lista id; stream SSE e non-stream; **`stop`** (stringa o array) |
| `POST /v1/chat/completions` | ✅ | ✅ | **C**: chat-template del tokenizer_config reso dal **motore Jinja C** (`LLMEnv/LLMJinja`, specchio di `GMBasicSharp/common/Jinja`); C#: via provider. Stream e non-stream; **`stop`** |
| `POST /v1/models/load` · `/v1/models/{id}/load` | ✅ | — | residenza: carica e tiene residente (body `{"model_dir": …}`); transazionale (su load fallito resta il precedente) |
| `POST /v1/models/unload` · `/v1/models/{id}/unload` | ✅ | — | residenza: libera handle+tokenizer+template |
| `GET /metrics` | ✅ | ✅ | contatori (richieste/completions/chat) |
| `n>1/best_of>1/suffix/logprobs` | 400 | 400 | fail-loud |
| `tools`/`tool_calls` (tool calling) | ✅ stub | ✅ | C: stub `tool_calls` con primo tool (200, `arguments:"{}"`, constrained dispatch completo in C#); C#: dispatch vincolato `INativeToolDispatchCompleter` (enum/bool/number, via `llm_ai_mlx_session_generate_constrained`) → `tool_calls` OpenAI, altrimenti `FinalAnswer` |
| `response_format: json_object|json_schema` (structured output) | — | ✅ | C#: `json_object`/`json_schema` → `RunWithJsonSchema` (prompt + schema + validazione `ExtractJson` + retry, garanzia JSON valido); estensione a oggetti annidati/array/enum via stesso motore vincolato di §2 |
| content non-stringa (vision) | 400 | — | fail-loud |
| concurrency | multi-sessione | multi-sessione | C: job per richiesta + scheduler (cap `GMB_MAX_ACTIVE`, default 2 via env) → 503 solo oltre cap; 409 su load/unload con job attivi; C#: coda + scheduler (cap `GMB_MAX_ACTIVE`, default 2) → 503 solo oltre cap. **Entrambi** con continuous batching (Fase 3b, `GMB_SERVER_BATCH` default ON) |

## Integrazione BASIC

IMPORT HTTP
IMPORT JSON
DIM co AS HTTPOPTIONS = NEW HTTPOPTIONS
co.Method = "POST"
co.Headers.ContentType = "application/json"
co.Body = "{""model"":""Llama-3.2-1B-Instruct-4bit"",""messages"":[{""role"":""user"",""content"":""Ciao""}]}"
DIM r AS FETCHRESULT = FETCH("http://127.0.0.1:8020/v1/chat/completions", co)
PRINT JSON.PARSE(r.Body)("choices")(0)("message")("content")


## Comando BASIC `AI SERVER` (gestione del server)

IMPORT AI WITH ALIAS
AI SERVER START <modelDir$> [port] ' avvia gmb_llm_server (porta opzionale, default 8020) e attende readiness
AI SERVER STOP                  ' ferma il processo
AI SERVER STATUS                ' stato live (running/pid/port/model/loaded/busy)
AI SERVER MODEL <modelDir$>     ' residenza: carica/swap del modello residente (v2 2f9209b5)
AI SERVER UNLOAD                ' residenza: libera il modello residente


## Verifica/benchmark

## Aperti / roadmap

## Collegamenti

### LLM-Tensor-Compare-Dense-Guida-Utente

*Fonte: `LLM-Tensor-Compare-Dense-Guida-Utente`*


## 1. `LLM TENSOR COMPARE <a>, <b>[, <glob>]`

### 1.1 Sintassi

LLM TENSOR COMPARE "<modello_a>", "<modello_b>"[, "<glob>"]


### 1.2 Parametri
| # | Parametro | Tipo | Obbligatorio | Descrizione |
|---|---|---|---|---|
| 1 | `<modello_a>` | stringa (path) | sì | Primo **GGUF** (o dir MLX/HF). È il modello di RIFERIMENTO. |
| 2 | `<modello_b>` | stringa (path) | sì | Secondo modello da confrontare col primo. |
| 3 | `<glob>` | stringa | no | Filtro **glob** sui nomi tensore (es. `"blk.*.ffn_*"`, `"*.attn_q.weight"`). Assente = tutti i tensori. Supporta `*`, `?`, `[...]`, `[^...]`, `[a-z]`, `\` di escape. |

### 1.3 Metriche (per-tensore)

### 1.4 Output

  [<nome>] <tipoA>/<tipoB> n=<numel> rel_l2=<..> avg_diff=<..>% max_abs=<..>
LLM TENSOR COMPARE '<a>' vs '<b>': n_a=<..> n_b=<..> matched=<..> only_a=<..> only_b=<..> mismatch=<..> skipped=<..> rel_l2=<..> avg_diff=<..>% max_diff=<..>% @<nome>


### 1.5 A cosa serve

### 1.6 Esempi

' Lineage: cosa è cambiato in un finetune?
LLM TENSOR COMPARE "base.gguf", "finetune.gguf"

' Solo l'attenzione (glob)
LLM TENSOR COMPARE "q4.gguf", "q8.gguf", "blk.*.attn_*"

' Un solo tensore
LLM TENSOR COMPARE "a.gguf", "b.gguf", "token_embd.weight"


## 2. `LLM TENSOR DENSE <in>, <out>[, <spec-esperti>]`

### 2.1 Sintassi

LLM TENSOR DENSE "<modello_moe.gguf>", "<out.gguf>"[, "<spec>"]


### 2.2 Parametri
| # | Parametro | Tipo | Obbligatorio | Descrizione |
|---|---|---|---|---|
| 1 | `<modello_moe.gguf>` | stringa (path) | sì | GGUF di un modello **MoE** (esperti impilati: `blk.N.ffn_{gate,up,down}_exps.weight`, ultima dim = numero esperti). |
| 2 | `<out.gguf>` | stringa (path) | sì | GGUF di uscita **denso**. |
| 3 | `<spec>` | stringa | no | Selezione dell'esperto. `""`/`"0"` = esperto 0 ovunque; `"d"` (una cifra) = esperto `d` ovunque; `"d0d1d2…"` = **una cifra per LAYER** (l'ultima si ripete per i layer successivi), stile `split-mixtral` di antirez. Default: `0`. |

### 2.3 Semantica

### 2.4 Output

LLM TENSOR DENSE: in=<in> out=<out> spec=<spec>


### 2.5 Esempi

' Esperto 0 per tutti i layer
LLM TENSOR DENSE "moe.gguf", "work/dense0.gguf", "0"

' Esperto diverso per i primi layer (poi l'ultima cifra si ripete)
LLM TENSOR DENSE "moe.gguf", "work/dense_mix.gguf", "0132"


## 3. Disponibilità sui backend e parità
| Backend | Comando | MiniTest |
|---|---|---|
| IL | ✅ | `shared/minitests/llm/llm_compare.bas`, `shared/minitests/llm/llm_dense.bas` |
| PCodeVM | ✅ | idem |
| VM/C | ✅ | idem |
| C99Native | ✅ | idem (`tools/sh/test_minitests_c99native`) |

## 4. Riferimenti

### LLM-Tokens-Guida-Utente

*Fonte: `LLM-Tokens-Guida-Utente`*


## 0. Riepilogo
| funzione | comando corrispondente (stampa) | forma con INTO (assegna) |
|---|---|---|
| `LLM.TOKENS(model, testo)` | `LLM TOKENS model, testo` | `LLM TOKENS model, testo INTO ids$` |
| `LLM.DETOKENIZE(model, ids)` | `LLM DETOKENIZE model, ids` | `LLM DETOKENIZE model, ids INTO txt$` |
| `LLM.TOKENCOUNT(model, testo)` | — | — |
| `LLM.TENSOR(sub[, a[, b[, c]]])` | `LLM TENSOR sub a, b, c` | `LLM TENSOR sub … INTO out$` |
| `LLM.POLICY(sub[, a[, b[, c]]])` | `LLM POLICY sub …` | `LLM POLICY sub … INTO out$` |
| `LLM.ARCH(sub[, a[, b[, c]]])` | `LLM ARCH sub …` | `LLM ARCH sub … INTO out$` |
| `LLM.MODULE(sub[, a[, b[, c]]])` | `LLM MODULE sub …` | `LLM MODULE sub … INTO out$` |
| `LLM.KERNEL(sub[, a[, b[, c]]])` | `LLM KERNEL sub …` | `LLM KERNEL sub … INTO out$` |
| `LLM.KV(sub[, a[, b[, c]]])` | `LLM KV sub …` | `LLM KV sub … INTO out$` |

## 1. `LLM.TOKENS(model, testo)` → stringa
| parametro | tipo | obbligatorio | descrizione |
|---|---|---|---|
| `model` | STRING | **sì** | percorso del modello/tokenizer: **cartella HF** (con `tokenizer.json`) o **file** `tokenizer.json` |
| `testo` | STRING | **sì** | testo da tokenizzare |

DIM ids$ AS STRING
ids$ = LLM.TOKENS(m$, "hello world")      ' "74,71,78,78,81,223,89,81,84,78,70"
LLM TOKENS m$, "hello world" INTO ids$    ' identico, forma-comando
PRINT LLM.TOKENS(m$, "hello")             ' "74,71,78,78,81"


## 2. `LLM.DETOKENIZE(model, ids)` → stringa
| parametro | tipo | obbligatorio | descrizione |
|---|---|---|---|
| `model` | STRING | **sì** | come sopra (stesso tokenizer usato per gli id) |
| `ids` | STRING | **sì** | id separati da virgola; spazi e virgole multiple tollerati |

PRINT LLM.DETOKENIZE(m$, "74,71,78,78,81")   ' hello


## 3. `LLM.TOKENCOUNT(model, testo)` → numero
| parametro | tipo | obbligatorio | descrizione |
|---|---|---|---|
| `model` | STRING | **sì** | modello/tokenizer |
| `testo` | STRING | **sì** | testo da contare |

## 4. `LLM.TENSOR(sub[, a[, b[, c]]])` → stringa
| parametro | tipo | obbligatorio | descrizione |
|---|---|---|---|
| `sub` | STRING | **sì** | sotto-verbo: `TOKENIZE`, `DETOKENIZE`, `VOCAB`, `TOKENIZER`, `TEMPLATE`, `CALIBRATE`, `ACTIVATIONSTATS`, `REGISTRY`, `QUANTERROR`, `COMPARE`, `DERIVE`, `MAKEPATCH`, `APPLYPATCH`, … |
| `a` | STRING o numerico | no | primo argomento del sotto-verbo (es. modello, testo, dir) |
| `b` | STRING o numerico | no | secondo argomento (es. out, formato) |
| `c` | STRING o numerico | no | terzo argomento (es. bpw) |

PRINT LLM.TENSOR("TOKENIZER", m$)               ' … vocab_size=259 source=gpt2
PRINT LLM.TENSOR("TOKENIZE", m$, "hello")       ' … n=5 ids=74,71,78,78,81
LLM TENSOR TOKENIZE m$, "hello" INTO out$       ' identico, forma-comando


## 5. `LLM.POLICY(...)` → stringa (20 sotto-verbi)
| parametro | tipo | obbligatorio | descrizione |
|---|---|---|---|
| `sub` | STRING | **sì** | `SHOW` (tabella ruolo→formato), `SET` (ruolo, formato), `RESET`/`CLEAR`, `SETITEM` (glob, formato), `CLEARITEM`/`CLEARITEMS`, … |
| `a`, `b` | STRING | per `SET`/`SETITEM`: **sì** (ruolo/glob e formato) | argomenti del sotto-verbo |
| `c` | STRING | no | usato da alcuni sotto-verbi |

DIM t AS STRING
t = LLM.POLICY("SHOW")                 ' tabella ruolo → formato
DIM u AS STRING
u = LLM.POLICY("SET", "FFN", "bf16")   ' imposta una regola ("" se accettata)
u = LLM.POLICY("RESET")                ' ripristina i default
LLM POLICY SHOW INTO t                 ' forma-comando


## 6. `LLM.ARCH(...)` → stringa (5 sotto-verbi)
| parametro | tipo | obbligatorio | descrizione |
|---|---|---|---|
| `sub` | STRING | **sì** | `SHOW`, `LIST`, `RESOLVE` (dir modello), `CHECK` (dir), `PLAN` (dir, out) |
| `a` | STRING | per `RESOLVE`/`CHECK`/`PLAN`: **sì** | directory del modello (o destinazione) |
| `b` | STRING | per `PLAN`: **sì** | file di output del piano |

PRINT LLM.ARCH("SHOW")                 ' descrizione risolta o "nessuna descrizione…"
PRINT LLM.ARCH("LIST")                 ' elenco dei descrittori
DIM a AS STRING
LLM ARCH RESOLVE m$ INTO a             ' richiede un modello HF reale


## 7. `LLM.MODULE(...)` → stringa (5 sotto-verbi)
| parametro | tipo | obbligatorio | descrizione |
|---|---|---|---|
| `sub` | STRING | **sì** | `SHOW`, `SUPPORT`, `MODALITY`, `PRIORITY`, `REGISTER` |
| `a`, `b` | STRING | secondo il sotto-verbo | es. direttiva/modulo (`SUPPORT TEXT`), `REGISTER <dir>` |

## 8. `LLM.KERNEL(...)` → stringa (2 sotto-verbi)
| parametro | tipo | obbligatorio | descrizione |
|---|---|---|---|
| `sub` | STRING | **sì** | `CODEC` (dir, formato), `PRIM` (dir) |
| `a`, `b` | STRING | **sì** per entrambi | `CODEC`: directory + formato target; `PRIM`: directory |

## 9. `LLM.KV(...)` → stringa (2 sotto-verbi)
| parametro | tipo | obbligatorio | descrizione |
|---|---|---|---|
| `sub` | STRING | **sì** | `SHOW` (stato KV del runtime), `SET` (coppia chiave/valore, formato dipendente dal runtime) |
| `a`, `b` | STRING | per `SET`: **sì** | parametri della coppia |

## 10. Forme-comando con `INTO` (regole comuni)

LLM <FAMIGLIA> <sotto-verbo> [argomenti…] INTO variabile$


## 11. Errori
| situazione | comportamento |
|---|---|
| tokenizer/modello illeggibile | **errore catturabile** (`ON ERROR`), mai stringa vuota |
| sotto-verbo inesistente per la famiglia | errore esplicito, catturabile |
| argomenti obbligatori mancanti | errore di compilazione con l'arità richiesta |
| `LLM.KV("SET")` senza coppia | errore esplicito |

## 12. Disponibilità e verifica (§4.1)
| backend | TOKENS | DETOKENIZE | TOKENCOUNT | TENSOR | POLICY | ARCH | MODULE | KERNEL | KV | `INTO` |
|---|---|---|---|---|---|---|---|---|---|---|
| IL | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| PCodeVM | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| VM/C | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| C99Native | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| verifica | file |
|---|---|
| MiniTest superficie completa (funzioni + INTO + arità + errori) | `shared/minitests/llm/llm_surface_value.bas` — **PASS 4/4** |
| MiniTest tokenizer (TOKENS/DETOKENIZE/TENSOR/INTO) | `shared/minitests/llm/llm_tokens_value.bas` — **PASS 4/4** |
| MiniTest famiglie (POLICY/ARCH + INTO + errore) | `shared/minitests/llm/llm_verb_families_value.bas` — **PASS 4/4** |
| MiniTest comando che stampa (regressione) | `shared/minitests/llm/llm_tensor_tokenize.bas` — **PASS 4/4** |
| Sample completo e complesso (usa OGNI funzione e ogni INTO) | `shared/samples/AiLLM/PromptBudgetReport.bas` — **parity4 PASS** |
| Sample tokenizer (esempio minimo) | `shared/samples/AiLLM/TokenizeRoundTrip.bas` — **parity4 PASS** |
| Gate | `check_basic_surface_parity.sh` 4/4 · `check_enum_ids.bas` · `no_stub.bas` · `check_file_size.sh` |

### LLVM-Modifica-LLM-Flusso-Completo-Guida-Utente

*Fonte: `LLVM-Modifica-LLM-Flusso-Completo-Guida-Utente`*


## 0. Il modello mentale: l'LLM come "programma" compilabile

              ┌──────────── IR ────────────┐
sorgente  →  ANALISI  →  DECISIONE  →  PASS  →  CODEGEN  →  EMISSIONE  →  VERIFICA  →  deploy
 (HF/GGUF)   (TENSOR   (POLICY/     (QUANT/   (COMPILE    (MODEL SAVE /   (QUALITY/    (AI
             IR/INFO)   ricette)     PRUNE/    .gmbllm)    CONVERT)       EVAL GATE)   REGISTER)
                                    MOEDENSE/
                                    DISTILL/
                                    TRAIN)

| Domanda | Fase | Comando chiave |
|---|---|---|
| Com'è fatto il modello? | 1 | `LLM TENSOR IR/GRAPH/MEMORY/COST` |
| Quali pezzi posso togliere/tenere? | 1–2 | `SENSITIVITY`, `USAGEEVAL`, `POLICY` |
| Togliere/aggiungere esperti, teste, layer | 3 | `PRUNE`, `PRUNEHEADS`, `MOEDENSE`, `EXTRACT`, `ABLATE`, `REMOVE`, `ADD` |
| Quanti bit a ciascun tensore | 4 | `LLM QUANT` / `MODEL QUANTIZE` |
| Insegnare/migliorare (teacher, pref., reward) | 5–6 | `TRAIN DISTILL` / `FINETUNE`/`DPO`/`GRPO` |
| Fondere modelli (soup) | 3/6 | `LLM MERGE` |
| Salvare, convertire, **verificare che risponda** | 7–8–9 | `MODEL SAVE/CONVERT`, `QUALITY`/`EVAL GATE`, `DIFF` |

## 1. Prerequisiti e convenzioni

## 2. Fase 0 — Sorgente F16 (il prerequisito di tutto)

LLM MODEL CONVERT "<dir_mlx>", "work/src_f16.gguf", "F16"   ' MLX -> GGUF a tensori F16


## 3. Fase 1 — Analisi (IR): capire prima di toccare

### 3.1 Struttura e inventario

LLM TENSOR INFO      "work/src_f16.gguf"   ' format, arch, n-tensori, kv, n-layer, modality, byte
LLM TENSOR IR        "work/src_f16.gguf"   ' tabella per-tensore: nome, RUOLO, tipo, dims, byte
LLM TENSOR LAYERS    "work/src_f16.gguf"   ' indici layer + n-tensori per layer
LLM TENSOR GRAPH     "work/src_f16.gguf"   ' grafo per-layer (attention/moe/ffn/norm) + tipo
LLM TENSOR CHECK     "work/src_f16.gguf"   ' validatore spec-vs-modello (0 tensori OTHER)
LLM TENSOR ARCHRECON "work/src_f16.gguf"   ' valida la descrizione .gmbarch risolta (RICONCILIATO/DIVERGENZA)
LLM MODEL INSPECT    "work/src_f16.gguf"   ' dump del descrittore .gmbarch (arch, roles, quant, modality)


### 3.2 Profilo di costo e roofline (dove andrà il tempo)

LLM TENSOR MEMORY         "work/src_f16.gguf"          ' byte per categoria + %
LLM TENSOR COST           "work/src_f16.gguf"          ' pesi + KV byte/token + totale@contesto
LLM TENSOR DECODEPROFILE  "work/src_f16.gguf","4096:800" ' tok/s bound (memory-bound/roofline)
LLM TENSOR NUMGRAPH       "work/src_f16.gguf"          ' catena OP numeriche per-layer + precisioni
LLM TENSOR CAPACITY       "work/src_f16.gguf","8192"   ' capacità esatta a ctx + fit RAM


### 3.3 Sensibilità: quali ruoli soffrono la quant

LLM TENSOR SENSITIVITY "work/src_f16.gguf", "work/eval_ds.txt"   ' ΔPPL per ruolo
LLM TENSOR USAGEEVAL   "work/src_f16.gguf", "work/bench.txt"     ' obiettivo d'USO (non solo PPL)
LLM TENSOR EXPOLICY    "work/src_f16.gguf","work/expolicy.json"  ' policy residenza esperti (hot set) -> applicabile live


### 3.4 Uso degli esperti (quali esperti contano davvero)

' (imatrix prodotta dalla pipeline imatrix; usata da PRUNE_EXPERT=usage/imp e da QUANT con IMATRIX)
LLM POLICY SET "IMATRIX", "work/olmoe.imatrix.gguf"


## 4. Fase 2 — Decisione: policy e ricette

LLM POLICY SET "ROUTED_EXPERT", "iq2_xxs"    ' esperti MoE: ~2.06 bit, guidati dall'importanza
LLM POLICY SET "FFN",           "q4_0"       ' FFN densi: ~4.5 bit
LLM POLICY SET "ATTENTION",     "q8_0"       ' proiezioni attention: ~8.5 bit
LLM POLICY SET "EMBEDDING",     "f16"        ' embedding/output: nessuna perdita
LLM POLICY SET "PRUNE_EXPERT",  "imp"        ' criterio pruning: first|top|bottom|ctop|cbot|rand|usage|imp
LLM POLICY SET "IMATRIX",       "work/olmoe.imatrix.gguf"


LLM POLICY DEFINE "compressa", { ROUTED_EXPERT: "iq2xxs", FFN: "q4_0", ATTENTION: "q8_0" }
LLM POLICY SETITEM "ROUTED_EXPERT", "iq1_s"   ' override per-tensore/ruolo (marcato * nel QUANTIR)
LLM TENSOR QUANTIR "work/src_f16.gguf"        ' IR modificabile: 4 precisioni per OP + blocco per-tensore


LLM POLICY RECOVER …   ' recupera qualità dopo un REJECT (vedi guida model compiler §2bis-bis)
LLM TENSOR OBUDGET "work/src_f16.gguf","O2"   ' mappa livello -> budget bit/peso + verdetto ACCEPT/REJECT


## 5. Fase 3 — Trasformazioni strutturali (surgery)

### 5.1 Prune esperti — **la leva che funziona**

LLM PRUNE "work/src_f16.gguf", "work/pruned48.gguf", 48


### 5.2 Prune teste attenzione / hidden FFN (structured pruning densa)

LLM POLICY SET "PRUNE_HEADS", "16"
LLM TENSOR PRUNEHEADS "work/m.gguf", "work/heads16.gguf"   ' slice q/k/v + attn_output, update head_count

LLM POLICY SET "FFN_KEEP", "7168"
LLM TENSOR PRUNEFFN "work/m.gguf", "work/ffn7168.gguf"     ' taglia dim intermedia FFN, update feed_forward_length


### 5.3 MoE → denso (merge degli esperti)

LLM TENSOR MOEDENSE "work/moe.gguf", "work/dense.gguf"   ' media esperti -> 1 FFN denso, elimina router, expert_count=1


### 5.4 Estrarre un esperto / rivellare

LLM EXTRACT            "work/moe.gguf", "work/exp3.gguf", 17, 3   ' layer 17, esperto 3 (frammento)
LLM TENSOR SPLITEXPERTS "work/moe.gguf", "work/split.gguf"        ' esperti STACKATI *_exps[d0,d1,E] -> individuali


### 5.5 Ablazione e rimozione layer (studi di ridondanza)

LLM MODEL ABLATE "work/m.gguf", "work/ablato.gguf", "layers.17.mlp.switch_mlp"  ' azzera i tensori col pattern
LLM MODEL REMOVE "work/m.gguf", "work/no_l17.gguf", "17"                        ' rimuove un layer
LLM MODEL DIFF   "work/m.gguf", "work/ablato.gguf", "work/diff.txt"             ' cosa è cambiato (giustifica)


### 5.6 Aggiungere moduli/adapter/modality

LLM MODEL ADAPTER  "work/base.gguf", "adapter.safetensors", "work/sft.gguf", "16" ' W += (alpha/r)·B@A (LoRA cotto)
LLM MODEL ADD      "work/base.gguf", "work/module.gguf", "work/arricchito.gguf"   ' aggiunge/sovrascrive tensori per nome
LLM MODEL MODALITY "work/base.gguf", "mmproj-...gguf", "work/mio.gmmod"           ' emette descriptor .gmmod (vision/audio/ocr/mtp)
LLM MODEL MODULE   "work/base.gguf", "work/mio.gmmod", "mmproj-...gguf", "work/mm.gguf"


### 5.7 Model soup — fondere due modelli

LLM POLICY SET "MERGE_METHOD", "ties"     ' mean (default) | dare | ties
LLM POLICY SET "MERGE_P", "0.1"           ' P(drop) DARE
LLM POLICY SET "MERGE_K", "0.2"           ' frazione keep TIES
LLM MERGE "work/a.gguf", "work/b.gguf", "work/soup.gguf"


## 6. Fase 4 — Compressione numerica (quant)

' A) per-ruolo / policy:
LLM QUANT "work/pruned48.gguf", "work/compressed.gguf"

' B) AUTO a budget bpw (bit per-TENSORE da sensibilità):
LLM QUANT "work/pruned48.gguf", "work/auto_b35.gguf", 3.5

' C) affine MLX completo (group/imatrix/expert_bits):
LLM MODEL QUANTIZE "work/m.gguf", "work/m_q4.gguf", 4, 64, "work/m.imatrix.gguf", 2


## 7. Fase 5 — Distillazione (un maestro ne insegna uno)

TRAIN DISTILL BUILD jsonl, { "model": mdl$, "out": "work/ds", "window": 16, "holdout": 0.2, "max": 40 }
TRAIN DISTILL TRAIN "work/ds.train.bin", { "model": mdl$, "out": "work/dt" }
TRAIN DISTILL SAMPLE jsonl, { "tokenizer": tok }
TRAIN DISTILL EVAL   jsonl, { "model": mdl$ }
TRAIN DISTILL APPLY  "work/dt", { "model": mdl$, "dataset": "work/ds.train.bin", "backup": "work/bk" }
TRAIN DISTILL RESTORE "work/bk"


## 8. Fase 6 — Addestramento (istruire davvero)

### 8.1 SFT — LoRA (leggero)

TRAIN DATASET BUILD ds, { "tokenizer": tok, "out": "work/rule", "window": 4, "holdout": 0.5 }
TRAIN FINETUNE mdl$, { "dataset": "work/rule.train.bin", "holdout": "work/rule.hold.bin", _
                       "ckpt": "work/ft1", "rank": 4, "layer": 6, "steps": 40, "batch": 2, "lr": 0.02, "seed": 7 }
PRINT "loss:"; LLM.FINETUNELOSS(); " hold:"; LLM.FINETUNEHOLD(); " ckpt:"; LLM.FINETUNECKPT()
TRAIN LORA APPLY opz  ' verifica: OFF vs ON su un prompt
TRAIN LORA MERGE      ' cottura dell'adapter nel modello
TRAIN LORA SAVE       ' raw -> safetensors (modello standalone)


### 8.2 SFT — full-parameter (tutti i pesi)

LLM TENSOR DEQUANT "work/m_q4.gguf", "work/m_f16.gguf"        ' prerequisito: affine -> F16 (vjp non deriva su affine)
TRAIN FINETUNE "work/m_f16", { "full": 1, "dataset": "work/rule.train.bin", "steps": 200, "lr": 0.0001 }


### 8.3 Preferenze — DPO (forma §4 e forma dict)

TRAIN DPO mdl$, "work/pairs", "work/out"
TRAIN DPO mdl$, { "chosen": "work/no_c.bin", "rejected": "work/no_r.bin", "steps": 4, "exact": 1 }
' -> checkpoint <ckpt>_best_A.raw (VIN·rank), _best_B.raw, .meta con dpo_engine=exact


### 8.4 RLHF / RLVR — GRPO (senza critic, premio a regola)

TRAIN GRPO mdl$, { "dataset": "work/rl.bin", "group": 8, "maxnew": 64, "clip": 0.2, "kl": 0.04, _
                   "lr": 0.0001, "steps": 50, "temperature": 0.9, "seed": 7 }


## 9. Fase 7 — Compilazione (`.gmbllm`) e kernel

LLM TENSOR COMPILE "work/compressed.gguf", "work/model.gmbllm"   ' graph+memory_plan+capacity+kernels
LLM COMPILE        "work/compressed.gguf", "work/model.gmbllm"   ' top-level (equivalente)
LLM TENSOR LOADPLAN "work/model.gmbllm"                          ' carica/valida l'artefatto
LLM TENSOR CED "work/compressed.gguf", "on"                      ' kill-switch: elimination esatta ALL-ZERO applicata in COMPILE
LLM TENSOR EXECPLAN "work/compressed.gguf", "work/execplan.json" ' piano per-layer (skip identity); runtime: AI SET OPTION "execplan", "<file>"


## 10. Fase 8 — Emissione e conversione di formato

LLM MODEL SAVE    "work/compressed.gguf", "work/final.gguf"   ' EMITTER: metadata + byte dei tensori
LLM MODEL CONVERT "work/final.gguf", "work/final_mlx", "MLX"  ' GGUF -> MLX (safetensors) eseguibile
LLM MODEL VALIDATE "work/final.gguf", "work/report.txt"       ' validazione strutturale


## 11. Fase 9 — Verifica (quality gate): "il modello risponde?"

' A) misure oggettive (PPL/NLL):
LLM QUALITY "work/final.gguf", "work/eval_ds.txt"                 ' alias TRAIN EVAL
TRAIN EVAL  "work/final.gguf", "work/eval_ds.txt"

' B) gate DEDICATO riferimento vs candidato:
TRAIN EVAL GATE "work/baseline_mlx", "work/final_mlx"             ' ACCEPT / REJECT / ERROR

' C) verifica vs vault di riferimento:
TRAIN EVAL VALIDATE "risultato" AGAINST "vault_folder"

' D) delta strutturale (cosa hai davvero cambiato):
LLM MODEL DIFF "work/baseline.gguf", "work/final.gguf", "work/diff.txt"


# 1. produci il modello trasformato in MLX (l'output lo decide il comando):
#    LLM MODEL CONVERT "work/final.gguf", "work/final_mlx", "MLX"
# 2. valuta originale e ridotto con lo STESSO benchmark:
python3 LLMEnv/LLMEngine/tools/llm_eval_ds4.py --model <dir-mlx-ORIGINALE>  --suite hard_smoke --json work/base.json
python3 LLMEnv/LLMEngine/tools/llm_eval_ds4.py --model work/final_mlx       --suite hard_smoke --json work/final.json


## 12. Fase 10 — Deploy e serving (anche >RAM)

' Modello MLX piccolo/medio:
AI REGISTER "final", %CHAT, "work/final_mlx", "{""engine"":""mlx"",""swap"":""true""}"
' ...poi AI AGENT ASK / generazione.

' Modello MoE GGUF out-of-core (streaming esperti):
LLM MODEL LOAD     final, "work/model.gguf"          ' default SWAP (esperti on-demand da disco)
LLM MODEL CACHE    final, 2048                        ' budget expert-cache (MB)
LLM MODEL PREFETCH final, 2                           ' prefetch cross-layer
LLM MODEL PROMOTE  final                              ' RESIDENT: tutto in RAM
PRINT LLM.STREAMSTATS("final")                        ' JSON: mode/device/bytesRead/cacheHits/prefetch…


## 12bis. Il lato runtime (dopo la surgery) — generation, KV, speculative, multimodal
| Capacità | Superficie | Note |
|---|---|---|
| Serving | `AI SERVER`, LLM server OpenAI-compatibile (C+C#) | `Guide/utente/LLM-Server-OpenAI-Compatibile.md` |
| Provider | `AI REGISTER alias, %CHAT, "mlx"\|"llamacpp"\|"openai"\|"ollama"` | `AI-Providers-Guide.md` |
| Generation | `AI AGENT ASK …`, `ASK STREAM INTO` | parametri `temp/top_p/top_k/seed/max_tokens/stop` |
| Distribuzione | `LLM TENSOR LOGITS/TOPK <model_dir>,<prompt>` | top-10 logprob+prob + entropia |
| KV cache | `LLM TENSOR KVPLAN/KVCLASSES/KVDAG/KVDECIDE/KVFORMAT`, hetero-KV, `kvSwapEnabled` | `KV-Quant-Hetero-KV-Guida-Utente.md` |
| Speculative | `LLM TENSOR SPEC` (NONE/MTP/DSPARK), DFlash2, n-gram drafter | `MTPLX-MTP-Speculative-Decode`, `DFlash2-…` |
| Streaming >RAM | `LLM MODEL LOAD/CACHE/PREFETCH/DEVICE/PROMOTE/DEMOTE` + `LLM.STREAMSTATS()` | §12 |
| Multimodal | `AI.OCR`, `AI.TRANSCRIBE`, `AI.ANALYZEIMAGE`; `MODEL MODALITY/MODULE` | `AI-OCR-Audio-Guide.md`, `AI-Immagini-Guide.md` |
| Structured output / tool calling | `AI.BROWSEREXTRACT(url,prompt,schema)`, agenti con tool-calling | `Server-API-Surface-Structured-Output-Tool-Calling` |
| Embeddings / RAG / memoria | `AI.EMBED`, `AI RAG ENSURE/INDEX/LOAD`, `AI.RAGCONTEXT/ASK`, `AI MEMORY` | `AI-RAG-Guide.md`, `AI-Memory-Guide.md` |

## 12ter. MODEL IR (mappa) — cosa è "l'IR" del compilatore
| Componente | Forma | Prodotto/letto da |
|---|---|---|
| **Descrizione arch** | `.gmbarch` (arch, roles, quant, scale_suffix, modality, runtime) | `LLM ARCH`, `LLM MODEL INSPECT` |
| **Artefatto compilato** | `.gmbllm` (graph + memory_plan + capacity + kernels) | `LLM COMPILE`, `LLM TENSOR LOADPLAN` |
| **Quant IR** | 4 precisioni per OP (weights/activation/accum/output) + per-tensore | `LLM TENSOR QUANTIR`, `POLICY SETITEM` |
| **State IR** | fase prefill/decode + state/modality/speculation/kv | `LLM TENSOR STATE` |
| **KV IR** | full/paged/rotating/disk-backed + prefetch/eviction | `LLM TENSOR KVPLAN/KVCLASSES/KVDAG/KVDECIDE` |
| **Memory Object** | regioni weights/kv/activation/expert_workset | `LLM TENSOR MOBJ/MPLAN` |
| **Numeric Graph** | catena OP numeriche per-layer + precisioni | `LLM TENSOR NUMGRAPH` |
| **Livelli IR** | gerarchia L0–L5 + fasi | `LLM TENSOR IRLEVELS` |

## 12quater. Superficie LLM estesa (B2–B5) — tokenizer, template, registry, calibrazione, derive, patch

### Tokenizer / chat template (B2)
| comando | uso |
|---|---|
| `LLM TOKENIZE <model>, <testo>` / `LLM DETOKENIZE <model>, <id,id,…>` | testo ↔ ids |
| `LLM VOCAB <model>[, <filtro>]` · `LLM TOKENIZER <model>` | vocab_size (± match) / fonte |
| `LLM TEMPLATE <model>` · `LLM TEMPLATETEST <model>, <role:content\|…>` | mostra / **rende** (Jinja) il chat template |
| `n = LLM.TOKENCOUNT(<model>, <testo>)` | **funzione**: numero di token |

IMPORT LLM
tok$ = "../../samples/Fixtures/ai/tiny_tokenizer.json"
LLM TOKENIZE tok$, "hello world"                 ' n=11 ids=74,71,78,78,81,223,89,81,84,78,70
n = LLM.TOKENCOUNT(tok$, "hello world")          ' n = 11
LLM TEMPLATETEST "../../samples/Fixtures/models/tiny_chat", "system:aiuta|user:ciao"


### Model registry / versioning (B3)

LLM REGISTRY "ADD",     "models/v3-q4.gguf", "v3-q4"
LLM REGISTRY "TAG",     "v3-q4", "ppl=25.9;recipe=tiny_ablate.gmbderive"
LLM REGISTRY "PROMOTE", "v3-q4"
LLM REGISTRY "SHOW",    "v3-q4"


### Calibrazione / quant-error (B4)
| comando | uso |
|---|---|
| `LLM CALIBRATE <model>, <dataset>[, <out.imatrix>]` | costruisce l'**imatrix** (dataset: testo o `.bin` di token) |
| `LLM ACTIVATIONSTATS <model>, <dataset>` | statistiche di routing (`layers/experts/tokens/active`) |
| `LLM QUANTERROR <ref>, <cand>` | errore di quantizzazione `rel_l2`/`max_abs` (tensori F32/F16 comuni) |
| `LLM POLICY SET "ACTIVATION", "fp8"` | chiave di 1ª classe = asse `activation` del QuantIR |

LLM CALIBRATE "../../samples/Fixtures/models/tiny_moe.gguf", "../../samples/Fixtures/models/tiny_tokens.bin", "work/im.gguf"
LLM QUANTERROR "work/a.gguf", "work/b.gguf"        ' rel_l2 + max_abs
LLM POLICY SET "ACTIVATION", "fp8"                 ' poi `LLM TENSOR QUANTIR` mostra a=fp8


### Compilatore riproducibile + delta binario (B5)
| comando | uso |
|---|---|
| `LLM DERIVE <ricetta>[, <out>]` | esegue un `.gmbderive` (pipeline di pass `BASE → PASS… → OUT`) |
| `LLM MAKEPATCH <base>, <target>, <patch>` | crea il **delta** (GGUF coi soli tensori cambiati/nuovi) |
| `LLM APPLYPATCH <base>, <patch>, <out>` | `base + patch → out` |

LLM DERIVE "../../samples/Fixtures/models/tiny_ablate.gmbderive"                  ' → work/target.gguf
LLM MAKEPATCH "../../samples/Fixtures/models/tiny_moe.gguf", "work/target.gguf", "work/delta.gmbpatch"
LLM APPLYPATCH "../../samples/Fixtures/models/tiny_moe.gguf", "work/delta.gmbpatch", "work/rebuilt.gguf"
LLM TENSOR QUANTERROR "work/rebuilt.gguf", "work/target.gguf"                     ' rebuilt == target (rel_l2=0)


## 13. L'ESEMPIO COMPLETO end-to-end (pipeline massima)

' work/ModelloSurgeryFull.bas — pipeline massima di model surgery
IMPORT LLM
IMPORT AI

SUB Main()
  ON ERROR GOTO H
  src$    = "work/src_f16.gguf"
  pruned$ = "work/pruned48.gguf"
  comp$   = "work/compressed.gguf"
  final$  = "work/final.gguf"
  mlx$    = "work/final_mlx"

  ' ── Fase 0: sorgente F16 (da MLX/HF) ────────────────────────────────
  LLM MODEL CONVERT "/Users/gaetano/ai/Models/gguf/olmoe-1b-7b", src$, "F16"

  ' ── Fase 1: analisi (IR) ────────────────────────────────────────────


### Sample di riferimento (già nel repo)

## 14. Riferimento rapido: comando per fase
| Fase | Comandi |
|---|---|
| 0 Sorgente | `LLM MODEL CONVERT <mlx>,<gguf>,"F16"` |
| 1 Analisi | `LLM TENSOR INFO/IR/LAYERS/GRAPH/CHECK/ARCHRECON/MEMORY/COST/DECODEPROFILE/NUMGRAPH/CAPACITY` · `LLM MODEL INSPECT` |
| 2 Decisione | `LLM POLICY SET/DEFINE/SETITEM/RECOVER` · `LLM TENSOR QUANTIR/OBUDGET/SENSITIVITY/USAGEEVAL/EXPOLICY` |
| 3 Surgery | `LLM PRUNE` · `LLM EXTRACT` · `LLM TENSOR PRUNEHEADS/PRUNEFFN/MOEDENSE/SPLITEXPERTS` · `LLM MODEL ABLATE/REMOVE/ADD/ADAPTER/MODALITY/MODULE` · `LLM MERGE` |
| 4 Quant | `LLM QUANT <in>,<out>[,<bpw>]` · `LLM MODEL QUANTIZE <src>,<dst>,<bits>[,<group>][,<imatrix>][,<expert_bits>]` |
| 5 Distill | `TRAIN DISTILL BUILD/SAMPLE/TRAIN/EVAL/APPLY/RESTORE` |
| 6 Training | `TRAIN DATASET BUILD` · `TRAIN FINETUNE [{full:1}]` · `TRAIN LORA APPLY/MERGE/SAVE/LOAD/INFO` · `TRAIN DPO` · `TRAIN GRPO` |
| 7 Compilazione | `LLM COMPILE` · `LLM TENSOR COMPILE/CED/EXECPLAN/LOADPLAN` |
| 8 Emissione | `LLM MODEL SAVE/CONVERT/VALIDATE` · `LLM MODEL CHECKPOINT/ROLLBACK` |
| 9 Verifica | `LLM QUALITY` · `TRAIN EVAL` / `EVAL GATE` / `EVAL VALIDATE` · `LLM MODEL DIFF` |
| 10 Deploy | `AI REGISTER` · `LLM MODEL LOAD/CACHE/PREFETCH/DEVICE/PROMOTE/DEMOTE` · `LLM.STREAMSTATS()` |
| **Estesa (B2–B5)** | `LLM TOKENIZE/DETOKENIZE/VOCAB/TOKENIZER/TEMPLATE/TEMPLATETEST` · `LLM.TOKENCOUNT()` · `LLM REGISTRY` · `LLM CALIBRATE/ACTIVATIONSTATS/QUANTERROR` · `LLM DERIVE/MAKEPATCH/APPLYPATCH` (§12quater) |

## 15. Errori comuni e trappole

## Vedi anche

### Modelli-IA-Locali

*Fonte: `Modelli-IA-Locali`*


## 1. Percorsi di base (config GMBasic)

{
  "modelPaths": [
    "/Users/gaetano/ai/Models",
    "/Users/gaetano/ai/Models/gguf"
  ],
  "skillPaths": [ "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/skills" ]
}

| alias | role | kind |
|---|---|---|
| fixture-embedding | embedding | fixture |
| fixture-completion | completion | fixture |
| keyword-reranker | reranker | keyword |
| openrouter | completion | openai |
| deepseek | completion | openai |

## 2. Modelli MLX (safetensors) in `/Users/gaetano/ai/Models/`
| Modello | Path | Vision | Note |
|---|---|---|---|
| Qwen3.5-122B-A10B-mxfp4 | `~/ai/Models/Qwen3.5-122B-A10B-mxfp4` | ✅ | MoE mxfp4, ctx 262K (default gateway) |
| Qwen3.8-27B-8bit | `~/ai/Models/Qwen3.8-27B-8bit` | ✅ | 8-bit, vision |
| Qwen3.6-27B-4bit | `~/ai/Models/Qwen3.6-27B-4bit` | ✅ | 4-bit, vision |
| Qwen3.5-27B-Claude-4.6-Opus-Distilled-MLX-6bit | `~/ai/Models/Qwen3.5-27B-Claude-4.6-Opus-Distilled-MLX-6bit` | ✅ | 6-bit |
| Qwen3.6-35B-A3B-OptiQ-4bit-textonly | `~/ai/Models/Qwen3.6-35B-A3B-OptiQ-4bit-textonly` | ❌ | MoE 4-bit OptiQ |
| Qwen3-Coder-30B-A3B-Instruct-8bit | `~/ai/Models/Qwen3-Coder-30B-A3B-Instruct-8bit` | ❌ | MoE official 8-bit |
| Qwen3-Coder-Next-mxfp4-mlx | `~/ai/Models/Qwen3-Coder-Next-mxfp4-mlx` | ❌ | mxfp4 |
| GLM-4.5-Air-4bit | `~/ai/Models/GLM-4.5-Air-4bit` | ❌ | MoE 4-bit |
| gemma-4-31b-it-4bit | `~/ai/Models/gemma-4-31b-it-4bit` | ❌ | 4-bit |
| DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit | `~/ai/Models/DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit` | ❌ | R1 distill 8-bit |
| gpt-oss-120b-mxfp4-bf16 | `~/ai/Models/gpt-oss-120b-mxfp4-bf16` | ❌ | MoE mxfp4/bf16 |
| gpt-oss-20b-MXFP4-Q8 | `~/ai/Models/gpt-oss-20b-MXFP4-Q8` | ❌ | 20B |
| Llama-3.2-1B-Instruct-4bit | `~/ai/Models/Llama-3.2-1B-Instruct-4bit` | ❌ | piccolo (smoke) |
| Ternary-Bonsai-4B-mlx-2bit | `~/ai/Models/Ternary-Bonsai-4B-mlx-2bit` | ❌ | kind=small |
| **Qwen3.8-Flash-Next-tq4a-tq2e-g64** | `~/ai/Models/Qwen3.8-Flash-Next-tq4a-tq2e-g64` | ✅ | **qwen4_exp (97G)** — sperimentale Qwen4, GDN/QSA/N-gram; NON ancora supportato dal motore MLX GMBasic |
| **Qwen3.8-27B-DFlash2** | `~/ai/Models/Qwen3.8-27B-DFlash2` | ❌ | **draft DFlash2 (BF16, 3.6G, hidden 5120)** — draft model per speculative decoding del `Qwen3.8-27B-8bit` (gateway `qw38dflash2`); NON un chat standalone (embed/norm/lm_head assenti: il load-test MLX 2026-09-02 lo carica ma non lo esegue standalone). |

### OCR MLX — **`~/ai/Models/media/ocr/PaddleOCR-VL-8bit`** ← IL MODELLO OCR

### Image-gen (mflux) in `~/ai/Models/media/image-gen/`

## 3. Modelli GGUF in `/Users/gaetano/ai/Models/gguf/`
| File | Dimensione | Uso |
|---|---|---|
| `qwen2.5-omni-3b-Q4_K_M.gguf` | 2.0 GB | vision/multimodale |
| `mmproj-Qwen2.5-Omni-3B-Q8_0.gguf` | 1.4 GB | mmproj del Qwen2.5-Omni (da passare a `mmprojPath`) |
| `PaddleOCR-VL-1.6-GGUF.gguf` | 892 MB | **OCR** GGUF |
| `PaddleOCR-VL-1.6-GGUF-mmproj.gguf` | 841 MB | mmproj OCR |
| `parakeet-tdt-0.6b-v3-q8_0.gguf` | 897 MB | ASR/audio |

## 4. Altri modelli/progetti locali
| Posizione | Contenuto | Backend |
|---|---|---|
| `~/ds4/` | progetto **DeepSeek v4** agent C (`ds4` binary, `ds4_agent.c`); `ds4flash.gguf` è uno **stub 154B** — scaricarlo con `./download_model.sh ds4f-q4` (repo `antirez/deepseek-v4-gguf`) | ds4 |
| `~/llamacpp/models/` | solo `ggml-vocab-*.gguf` (vocab, nessun modello completo) | llama.cpp |
| Ollama | `qwen2.5-coder:32b` (19 GB), `gemma4:31b-mlx` (18 GB) — `ollama serve` su porta 11434 | ollama |
| `~/.gmbasic/libs/ml/osx-arm64/` | librerie runtime torch scaricate (libtorch_cpu, libtorch, libomp, libgmb_torch...) | ML runtime GMBasic |
| `~/ai/hf-cache/` | cache HuggingFace (hub/modules/xet) | — |
| `~/ai/env-*` | venv Python (env312, env-mflux, env-vllmlx, mineru-env) | rapid-mlx/mflux/mineru |

## 5. Registry dei gateway (settings letti)

### `unified-gateway/models.json` (registry canonica backend)
| alias | backend | vision | kind | path |
|---|---|---|---|---|
| antirez | ds4 | ❌ | chat | `~/ds4/ds4flash.gguf` |
| qw122 / qw38 / qw3627 / qw27opus | mlx | ✅ | chat | `~/ai/Models/...` |
| qw3635 / qw3coder8bit / glm45air / ds32 / gemma4 / mixtral / gptoss120 / gptoss20 / qw3codernext | mlx | ❌ | chat | `~/ai/Models/...` |
| bonsai4b | mlx | ❌ | **small** | `~/ai/Models/Ternary-Bonsai-4B-mlx-2bit` |
| gemma4o | ollama | ❌ | chat | (`ollama_model` tag) |
| **ocr** | **mlx** | ✅ | **media** | `~/ai/Models/media/ocr/PaddleOCR-VL-8bit` |
| flux1-dev / flux2-klein-4b / z-image-turbo / qwen-image | mflux | ❌ | **media** | `~/ai/Models/media/image-gen/...` |

### `unicorn-server/data/`

## 6. Come registrare l'OCR in GMBasic (per `CONVERT.EXTRACT`)

AI REGISTER alias, capability, source [, param]


AI REGISTER "local-ocr", %OCR, "apple-vision"
AI SET ACTIVE %OCR, "local-ocr"
desc = AI.OCR("immagine.png")


AI REGISTER "gguf-ocr", %OCR, "llamacpp-vision", "{""modelPath"":""/Users/gaetano/ai/Models/gguf/PaddleOCR-VL-1.6-GGUF.gguf"",""mmprojPath"":""/Users/gaetano/ai/Models/gguf/PaddleOCR-VL-1.6-GGUF-mmproj.gguf""}"
AI SET ACTIVE %OCR, "gguf-ocr"


## 7. Riferimenti

## Navigazione canonica — AI embedded

### TRAIN-Namespace-Guide

*Fonte: `TRAIN-Namespace-Guide`*


## 1. Cosa significa "addestrare un modello" (spiegato semplice)

### Addestrare ≠ usare

## 2. Il ciclo di lavoro (dall'esempio al modello migliore)

1. PREPARA   →  TRAIN DATASET BUILD        (trasforma i tuoi testo/esempi in dataset .bin)
2. ADDESTRA  →  TRAIN FINETUNE  (o DPO)    (scrive l'adapter dai dataset)
3. VALUTA    →  TRAIN EVAL                 (misura se ha migliorato; gate accetta/rifiuta)
4. APPLICA   →  TRAIN LORA APPLY / MERGE   (usa l'adapter addestrato)
5. TRACCIA   →  LLM.RUNSJSON() / RUNSCOUNT (storia delle run per confronto)


## 3. I concetti chiave, in breve
| Concetto | Cosa è | Dove lo trovi |
|---|---|---|
| **Dataset** | Esempi in formato binario (`.bin`): sequenze di numeri-token + la risposta attesa. | prodotto da `DATASET BUILD`/`DISTILL BUILD` |
| **Train / Holdout** | Il dataset si divide: una parte per *imparare* (train), una per *controllare* (holdout). | opzione `holdout` (es. 0.2 = 20% di controllo) |
| **Adapter (LoRA)** | L'appendice di numeri che impara. | file `<ckpt>_best_A.raw` / `_B.raw` |
| **Checkpoint** | Salvataggio dei pesi in un momento preciso. | prefisso `ckpt` (es. `work/ft`) |
| **Step** | Un "giro" di aggiornamento dei pesi. Più step = impara di più (ma rischia di "studiare a memoria"). | opzione `steps` |
| **Batch** | Quanti esempi guarda insieme ad ogni step. | opzione `batch` |
| **Rank** | Quanto è "grande" l'appendice LoRA. 4 = piccola/economica, 16 = più capiente. | opzione `rank` |
| **Layer** | *Quale* punto interno del modello modificare. "auto" scegli l'ultimo strato. | opzione `layer` |
| **Learning rate (lr)** | La "sensibilità" con cui i pesi cambiano. Troppo alto = instabile; troppo basso = lento. | opzione `lr` |
| **Loss** | L'"errore" del modello sugli esempi. Scende mentre impara. | riportato in output (`train`/`hold`) |
| **Overfitting** | Quando impara a memoria gli esempi e non generalizza. Si scopre confrontando `train` (molto basso) con `hold` (alto). | metriche `train` vs `hold` |
| **Eval/perplexity** | La misura "quanto è sorpreso" il modello: più bassa = meglio. | `TRAIN EVAL` |
| **DPO** | Insegna con **coppie**: "questa risposta è migliore di quella". Più diretto del fine-tune. | comando `TRAIN DPO` |
| **Distill** | Un modello grande "insegna" a uno piccolo (o a se stesso) di comportarsi uguale. | comandi `TRAIN DISTILL …` |

## 4. Riferimento completo dei comandi

### 4.1 `TRAIN DATASET BUILD` — trasforma i tuoi esempi in dataset

TRAIN DATASET BUILD <jsonl> [, {opts}]

| Parametro | Obbligatorio | Tipo | Spiegazione | Default |
|---|---|---|---|---|
| `<jsonl>` | sì | stringa | File di esempi (una riga JSON per esempio, es. `{"prompt":"...","response":"..."}`). | — |
| `{opts}.tokenizer` | sì | stringa | File del **tokenizer** (il "dizionario" testo→numeri) da usare. | — |
| `{opts}.out` | sì | stringa | Prefisso dei file di output: scrive `<out>.train.bin` e `<out>.hold.bin`. | — |
| `{opts}.window` | no | int | Lunghezza (in token) di ogni blocco di esempio. | 16 |
| `{opts}.holdout` | no | double | Frazione di esempi messi da parte per il **controllo** (0.2 = 20%). | 0.2 |

IMPORT AI
IMPORT IO

SUB Main()
    ' 1) scrivi un piccolo file di esempi
    DIM h AS LONG = IO.FCREATE("work/esempi.jsonl")
    IO.FWRITE(h, "{""prompt"": ""ciao"", ""response"": ""buongiorno""}" + CHR(10))
    IO.FCLOSE(h)

    ' 2) trasformalo in dataset (train + holdout)
    TRAIN DATASET BUILD "work/esempi.jsonl", { "tokenizer": "tokenizer.json", _
                                               "out": "work/mio_ds", "window": 16, "holdout": 0.2 }
    PRINT "creati: work/mio_ds.train.bin e work/mio_ds.hold.bin"
END SUB


### 4.2 `TRAIN FINETUNE` — insegna col "completamento" (LoRA SFT)

TRAIN FINETUNE <modelDir> [, {opts}]

| Parametro | Obbligatorio | Tipo | Spiegazione | Default |
|---|---|---|---|---|
| `<modelDir>` | sì | stringa | Cartella del **modello base** (MLX) da cui partire. | — |
| `{opts}.dataset` | no | stringa | Dataset di **training** (prodotto al §4.1). | `work/rule.train.bin` |
| `{opts}.holdout` | no | stringa | Dataset di **controllo** (per verificare che non "studi a memoria"). | `work/rule.hold.bin` |
| `{opts}.ckpt` | no | stringa | **Prefisso** con cui salvare l'adapter (e i checkpoint). | `work/ft` |
| `{opts}.rank` | no | int | Dimensione dell'appendice LoRA: **4**=piccola/economica, **8–16**=più capiente. | 4 |
| `{opts}.layer` | no | int | Strato interno da modificare; **auto** = ultimo strato (di solito il migliore). | auto |
| `{opts}.steps` | no | int | Quante volte ripetere l'aggiornamento. Più step = impara di più (ma rischia l'overfitting). | 60 |
| `{opts}.batch` | no | int | Quanti esempi considerare insieme per step. | 4 |
| `{opts}.lr` | no | double | **Learning rate**: quanto "forte" cambiano i pesi. Troppo alto = instabile. | 0.01 |
| `{opts}.seed` | no | int | Seme casuale: stesso seed = risultato riproducibile. | 7 |

IMPORT AI

SUB Main()
    ON ERROR GOTO Errore
    TRAIN FINETUNE "~/ai/Models/Llama-3.2-1B-Instruct-4bit", _
        { "dataset": "work/mio_ds.train.bin", "holdout": "work/mio_ds.hold.bin", _
          "ckpt": "work/mio_ft", "rank": 8, "steps": 60, "lr": 0.01 }
    PRINT "loss train="; LLM.FINETUNELOSS(); " hold="; LLM.FINETUNEHOLD()
    GOTO Fine
Errore:
    PRINT "addestramento fallito: "; LLM.FINETUNESTATUS()
Fine:
END SUB


### 4.3 `TRAIN LORA APPLY` — metti alla prova/produci con l'adapter

TRAIN LORA APPLY <ckpt> [, {opts}]

| Parametro | Obbligatorio | Tipo | Spiegazione | Default |
|---|---|---|---|---|
| `<ckpt>` | sì | stringa | Prefisso del checkpoint addestrato (es. `work/mio_ft`). | — |
| `{opts}.model` | **sì** | stringa | Cartella del modello MLX su cui fare la prova. | — |
| `{opts}.dataset` | **sì** | stringa | File `.bin` da cui prendere un prompt di prova. | — |

IMPORT AI
TRAIN LORA APPLY "work/mio_ft", { "model": "~/ai/Models/Llama-3.2-1B-Instruct-4bit", _
                                  "dataset": "work/mio_ds.train.bin" }


### 4.4 `TRAIN LORA MERGE` — fonde l'adapter dentro il modello

TRAIN LORA MERGE <base>, <adapter>, <out> [, <alpha>]

| Parametro | Obbligatorio | Tipo | Spiegazione | Default |
|---|---|---|---|---|
| `<base>` | sì | stringa | Modello base. | — |
| `<adapter>` | sì | stringa | Adapter LoRA (`.safetensors`) da fondere. | — |
| `<out>` | sì | stringa | Dove scrivere il modello fuso. | — |
| `<alpha>` | no | double | Quanto "pesa" l'adapter nel merge (1.0 = pieno). | 1.0 |

### 4.5 `TRAIN LORA SAVE` / `TRAIN LORA LOAD`

TRAIN LORA SAVE "work/mio_ft", "work/adapter.safetensors"   ' RAW <ckpt>_best_*.raw -> .safetensors
TRAIN LORA LOAD "work/adapter.safetensors", "work/ripreso"  ' .safetensors -> RAW <out>_best_*.raw + .meta

| Parametro | Obbligatorio | Tipo | Spiegazione |
|---|---|---|---|
| `<ckpt>`/`<adapter>` | sì | stringa | SAVE: prefisso del checkpoint RAW (es. `work/mio_ft`). LOAD: file `.safetensors`. |
| `<out>` | sì | stringa | SAVE: file `.safetensors` di destinazione. LOAD: prefisso del RAW da scrivere. |

### 4.6 `TRAIN LORA LIST` — quali adapter ho addestrato?

PRINT LLM.LORALIST()   ' -> {"count":2,"adapters":["work/mio_ft","work/altro_ft"]}


### 4.7 `TRAIN DPO` — insegna con le "preferenze" (quale risposta è migliore)

TRAIN DPO <model>, { "chosen": <ds.bin>, "rejected": <ds.bin>, "ckpt": <prefisso>, ... }

| Parametro | Obbligatorio | Tipo | Spiegazione | Default |
|---|---|---|---|---|
| `<model>` | sì | stringa | Cartella del modello MLX locale. | — |
| `{opts}.chosen` | **sì** | stringa | Dataset `.bin` con le risposte **preferite**. | — |
| `{opts}.rejected` | **sì** | stringa | Dataset `.bin` con le risposte **rifiutate**. | — |
| `{opts}.ckpt` | no | stringa | Prefisso dell'adapter di output. | `work/dpo` |
| `{opts}.beta` | no | double | Quanto il modello "osa" allontanarsi dal base. Basso = prudente. | 0.1 |
| `{opts}.steps`/`rank`/`batch`/`lr`/`seed` | no | — | Come per `FINETUNE`. | 40/4/2/5e-3/7 |

IMPORT AI
SUB Main()
    ON ERROR GOTO E
    TRAIN DPO "~/ai/Models/Llama-3.2-1B-Instruct-4bit", _
        { "chosen": "work/mie_preferite.bin", "rejected": "work/mie_rifiutate.bin", _
          "ckpt": "work/mio_dpo", "beta": 0.1, "steps": 40 }
    GOTO F
E:
    PRINT "DPO fallito (modello/dataset?)"
F:
END SUB


### 4.7b `TRAIN GRPO` — impara "provando" e premiando i tentativi (RLHF senza critic)

TRAIN GRPO <model>, { "dataset": <ds.bin>, "ckpt": <prefisso>, "group": 4, ... }

| Parametro | Obbligatorio | Tipo | Spiegazione | Default |
|---|---|---|---|---|
| `<model>` | sì | stringa | Cartella del modello MLX locale. | — |
| `{opts}.dataset` | **sì** | stringa | Dataset `.bin` dei prompt (col token atteso come premio). | — |
| `{opts}.ckpt` | no | stringa | Prefisso dell'adapter di output. | `work/grpo` |
| `{opts}.group` | no | int | Completamenti campionati per prompt (≥2). | 4 |
| `{opts}.maxnew` | no | int | Token generati per tentativo. | 1 |
| `{opts}.clip` | no | double | Raggio del clip del surrogate (PPO). | 0.2 |
| `{opts}.kl` | no | double | Coefficiente KL vs il modello base (0 = off). | 0.0 |
| `{opts}.temperature` | no | double | Temperatura di campionamento (esplorazione). | 1.0 |
| `{opts}.steps`/`rank`/`lr`/`seed` | no | — | Come per `FINETUNE`. | 40/4/5e-3/7 |

IMPORT AI
SUB Main()
    ON ERROR GOTO E
    TRAIN GRPO "~/ai/Models/Llama-3.2-1B-Instruct-4bit", _
        { "dataset": "work/miei_prompt.bin", "ckpt": "work/mio_grpo", _
          "group": 4, "maxnew": 1, "clip": 0.2, "temperature": 1.0, "steps": 40 }
    GOTO F
E:
    PRINT "GRPO fallito (modello/dataset?)"
F:
END SUB


### 4.8 `TRAIN EVAL` (e `TRAIN QUALITY`) — misura la qualità

TRAIN EVAL <model>, <prompt/dataset>[, <out>][, <ref>]
TRAIN QUALITY <model>, <prompt>, <out>         ' alias F0
TRAIN EVAL GATE <ref>, <cand>                  ' gate DEDICATO -> stampa ACCEPT/REJECT/ERROR
TRAIN EVAL VALIDATE <result> AGAINST [VAULT] <folder>   ' alias di AI EVAL VALIDATE

| Parametro | Obbligatorio | Tipo | Spiegazione |
|---|---|---|---|
| `<model>` | sì | stringa | Modello da valutare: una **directory MLX/HF** (con `tokenizer.json`) **oppure un file `.gguf`** (il tokenizer è ricostruito dai token/merges embedded nel GGUF). In entrambi i casi è caricato dall'**engine MLX/Metal**. |
| `<prompt/dataset>` | sì | stringa | Se è un **file esistente**, viene letto come **dataset** (ogni riga non vuota è un prompt) e si riporta la **NLL/perplexity MEDIA**; altrimenti è il testo del prompt. |
| `<out>` | no | stringa | File dove salvare il risultato (NLL/perplexity). Se assente, il valore resta nello stato. |
| `<ref>` | no | stringa | Modello di referenza per il gate automatico. |

### 4.9 `TRAIN DISTILL …` — un modello "maestro" ne insegna uno piccolo

TRAIN DISTILL BUILD   <jsonl>, {opts}     ' prepara i dati
TRAIN DISTILL SAMPLE  <prompts>, {opts}   ' genera risposte (serve un modello)
TRAIN DISTILL EVAL    <jsonl>, {opts}     ' valuta gli esempi filtrati
TRAIN DISTILL TRAIN   <ds.bin>, {opts}    ' addestra dal dataset distillato
TRAIN DISTILL APPLY   <ckpt>, {opts}      ' applica l'adapter
TRAIN DISTILL RESTORE <backup>            ' ripristina un backup


TRAIN DISTILL TRAIN "work/kd.train.bin", {model:"<studente>", teacher:"<maestro>", ckpt:"work/kd", steps:40, rank:4, temperature:2.0}


## 5. Le funzioni di stato e registro (`LLM.*`)
| Funzione | Ritorna | Spiegazione |
|---|---|---|
| `LLM.FINETUNELOSS()` | double | Loss finale su training dell'ultima run. |
| `LLM.FINETUNEHOLD()` | double | Loss finale su controllo (holdout). |
| `LLM.FINETUNEBASETRAIN()` / `...BASEHOLD()` | double | Loss **prima** dell'addestramento (confronto). |
| `LLM.FINETUNECKPT()` | stringa | Checkpoint salvato (`""` se nessuna run ok). |
| `LLM.FINETUNESTATUS()` | stringa | `idle`/`running`/`ok`/`error…`. |
| `LLM.FINETUNEDEMOCHANGED()` | double | `1` se l'adapter cambia la generazione di prova. |
| `LLM.RUNSCOUNT()` | int | Quante run (training/apply) sono registrate. |
| `LLM.RUNSJSON()` | stringa | Storia completa in JSON (vedi sotto). |
| `LLM.RUNSITEM("<id>")` | stringa | I dettagli di **una** run (`""` se l'id non esiste). |
| `LLM.RUNSCLEAR()` | int | Svuota il registro; ritorna quante run ha rimosso. |
| `LLM.LORALIST()` | stringa | `{"count":N,"adapters":[...]}`. |
| `LLM.LORASAVE("<ckpt>","<out>")` | int | Esporta l'adapter RAW del fine-tune in `.safetensors` (`TRAIN LORA SAVE`). |
| `LLM.LORALOAD("<adapter>","<out>")` | int | Importa un `.safetensors` in adapter RAW (`TRAIN LORA LOAD`). |
| `LLM.TRAINEXPORT("<out>")` | int | Esporta l'adapter dell'ultima run (`TRAIN EXPORT`). |
| `LLM.RUNSTOP("<id>")` | int | Rimuove una run dal registro (`TRAIN STOP`). |
| `LLM.TRAINCONFIG()` | stringa | I parametri di sessione correnti (default). |
| `LLM.TRAINCONFIGSET("k","v")` | stringa | Cambia un default; ritorna il JSON aggiornato. |
| `LLM.DATASETSTATS("<file>")` | stringa | Statistiche del dataset: `bytes`; se è un `.bin` del fine-tune anche `samples`, `tokens`, `target_min`, `target_max`, `target_mean`. |
| `LLM.EVALGATE("<ref>","<cand>")` | stringa | Verdetto del gate ref/cand (`TRAIN EVAL GATE`): `ACCEPT`/`REJECT-FLAG`/`ERROR`. |

{"count":2,"runs":[
  {"id":1,"kind":"finetune","status":"ok","train":0.42,"hold":0.51,
   "baseTrain":0.90,"baseHold":0.88,"greedyBefore":-1,"greedyAfter":-1,
   "demoChanged":1,"ckpt":"work/mio_ft"},
  {"id":2,"kind":"lora_apply","status":"error","train":-1,"hold":-1,
   "baseTrain":-1,"baseHold":-1,"greedyBefore":-1,"greedyAfter":-1,
   "demoChanged":0,"ckpt":"work/no_ckpt"}
]}


### Comandi di registro e configurazione (§4)

TRAIN RUNS LIST              ' stampa le run (JSON di LLM.RUNSJSON())
TRAIN RUNS SHOW "<id>"       ' stampa il dettaglio di una run (LLM.RUNSITEM("<id>"))
TRAIN RUNS PRUNE             ' svuota il registro (stampa quante rimosse, LLM.RUNSCLEAR())
TRAIN CONFIG SHOW            ' stampa i default di sessione (LLM.TRAINCONFIG())
TRAIN CONFIG SET "<k>","<v>" ' cambia un default (stampa il JSON aggiornato)
TRAIN STATUS                 ' stampa lo stato del registro run (LLM.RUNSJSON())
TRAIN STATUS "<id>"          ' stampa lo stato di una run (LLM.RUNSITEM("<id>"))
TRAIN LORA INFO              ' stampa gli adapter registrati (LLM.LORALIST())
TRAIN DATASET STATS "<file>" ' stampa le statistiche del dataset (LLM.DATASETSTATS("<file>"))
TRAIN EXPORT "<out>"         ' esporta l'adapter dell'ultima run (LLM.TRAINEXPORT) -> stampa n file
TRAIN EXPORT "<run>", "<out>"' esporta l'adapter di UNA run specifica (per id) -> n file
TRAIN STOP "<id>"            ' rimuove una run dal registro (stampa 1 se trovata)
TRAIN EVAL GATE "<ref>", "<cand>"  ' gate ref/cand dedicato -> stampa il verdetto


## 6. Errori e problemi comuni
| Sintomo | Cosa significa | Cosa fare |
|---|---|---|
| `model_dir/dataset/holdout mancanti` | hai dimenticato un parametro obbligatorio | controlla la tabella del comando |
| `dataset vuoto` / `tokenizer non caricabile` | il `jsonl`/tokenizer non sono validi o il path è sbagliato | verifica i percorsi (relativi alla cartella del programma) |
| `hold` sale, `train` scende | **overfitting**: impara a memoria | riduci `steps`, aumenta `holdout`, abbassa `rank` |
| loss che non scende | learning rate troppo basso o dataset troppo piccolo | alza un po' `lr`, aggiungi esempi |
| loss che "esplode" (NaN) | learning rate troppo alto | abbassa `lr` (es. 0.005) |
| il comando "non esiste" (`AI DISTILL`, `LLM FINETUNE`) | sono stati **spostati** in `TRAIN` (F4) | usa `TRAIN DISTILL` / `TRAIN FINETUNE` |
| serve ripartire | hai il checkpoint | `TRAIN RESUME` con `{ "ckpt": ... }` |

ON ERROR GOTO Errore
TRAIN FINETUNE "modello"
GOTO Ok
Errore:
PRINT "fallito: "; LLM.FINETUNESTATUS()
Ok:


## 7. Un esempio completo commentato

IMPORT AI
IMPORT LLM
IMPORT IO

SUB Main()
    ' (1) PREPARA: scrivo un file di esempi e lo trasformo in dataset.
    DIM tok AS STRING = PROJECTFOLDER() + "/../../Fixtures/ai/tiny_tokenizer.json"
    DIM h AS LONG = IO.FCREATE("work/train_tour.jsonl")
    IO.FWRITE(h, "{""prompt"": ""hello"", ""response"": ""world""}" + CHR(10))
    IO.FCLOSE(h)
    TRAIN DATASET BUILD "work/train_tour.jsonl", { "tokenizer": tok, "out": "work/ds", "window": 4 }

    ' (2) FUNZIONI di stato/config/dataset.
    PRINT LLM.DATASETSTATS("work/ds.train.bin")
    PRINT LLM.TRAINCONFIG()
    LLM.TRAINCONFIGSET("steps", "12")


## 8. Glossario

## Vedi anche

<!-- END SURFACE -->




<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **LLM.ARCH(x1)** — `arch$ = LLM.ARCH("SHOW")` (shared/samples/AiLLM/PromptBudgetReport.bas:104)
- **LLM.BENCHRESULT(x1)** — `PRINT "LLM BENCHMARK + LLM.BENCHRESULT — sessione KV di un agente"` (shared/samples/Ai/AiBenchmark.bas:34)
- **LLM.CACHESTATS(x1)** — `PRINT "LLM.CACHESTATS — KV-cache di un agente"` (shared/samples/Ai/KvCacheStats.bas:24)
- **LLM.CALLMAP(x1)** — `PRINT "CALLMAP  UnitScale    -> "; LLM.CALLMAP("UnitScale")` (shared/samples/Ai/SemanticGraphDemo.bas:70)
- **LLM.CHECKREV(x1)** — `PRINT "CHECKREV(match)       -> "; LLM.CHECKREV("rev=eeb9e87611bf1779")` (shared/samples/Ai/SemanticGraphDemo.bas:86)
- **LLM.COMMIT()** — `PRINT LLM.COMMIT()` (shared/samples/Ai/SemanticPatchApply.bas:39)
- **LLM.CONTEXT(x1)** — `PRINT "CONTEXT  UnitScale(1) -> "; LLM.CONTEXT("UnitScale", 1)` (shared/samples/Ai/SemanticGraphDemo.bas:80)
- **LLM.DATASETSTATS(x1)** — `PRINT "DATASETSTATS : "; LLM.DATASETSTATS(ds + ".train.bin")` (shared/samples/AiLLM/TrainCommandsTour.bas:42)
- **LLM.DECODESTATS(x1)** — `DIM ds$ = LLM.DECODESTATS("nessun-agente")` (shared/samples/Ai/AiBenchmark.bas:80)
- **LLM.DETOKENIZE(x1, x2)** — `ritorno$ = LLM.DETOKENIZE(model$, ids$)` (shared/samples/AiLLM/TokenizeRoundTrip.bas:36)
- **LLM.DISTILLCKPT()** — `PRINT "         ckpt="; LLM.DISTILLCKPT()` (shared/samples/AiLLM/DistillPipeline.bas:60)
- **LLM.DISTILLDONE()** — `PRINT "TRAIN  : done="; LLM.DISTILLDONE(); " loss="; LLM.DISTILLLOSS(); " hold="; LLM.DISTILLHOLD()` (shared/samples/AiLLM/DistillPipeline.bas:59)
- **LLM.DISTILLEVALACCEPT(x1)** — `PRINT "EVAL   : baseTok="; LLM.DISTILLEVALTOK(0); " basePass="; LLM.DISTILLEVALPASS(0); " baseAcc=";…` (shared/samples/AiLLM/DistillPipeline.bas:72)
- **LLM.DISTILLEVALPASS(x1)** — `PRINT "EVAL   : baseTok="; LLM.DISTILLEVALTOK(0); " basePass="; LLM.DISTILLEVALPASS(0); " baseAcc=";…` (shared/samples/AiLLM/DistillPipeline.bas:72)
- **LLM.DISTILLEVALTOK(x1)** — `PRINT "EVAL   : baseTok="; LLM.DISTILLEVALTOK(0); " basePass="; LLM.DISTILLEVALPASS(0); " baseAcc=";…` (shared/samples/AiLLM/DistillPipeline.bas:72)
- **LLM.DISTILLHOLD()** — `PRINT "TRAIN  : done="; LLM.DISTILLDONE(); " loss="; LLM.DISTILLLOSS(); " hold="; LLM.DISTILLHOLD()` (shared/samples/AiLLM/DistillPipeline.bas:59)
- **LLM.DISTILLLOSS()** — `PRINT "TRAIN  : done="; LLM.DISTILLDONE(); " loss="; LLM.DISTILLLOSS(); " hold="; LLM.DISTILLHOLD()` (shared/samples/AiLLM/DistillPipeline.bas:59)
- **LLM.DISTILLSAMPLEDONE()** — `" sampledone="; LLM.DISTILLSAMPLEDONE()` (shared/samples/AiLLM/DistillPipeline.bas:66)
- **LLM.DISTILLSAMPLEFAIL()** — `PRINT "SAMPLE : samplen="; LLM.DISTILLSAMPLEN(); " samplefail="; LLM.DISTILLSAMPLEFAIL(); _` (shared/samples/AiLLM/DistillPipeline.bas:65)
- **LLM.DISTILLSAMPLEN()** — `PRINT "SAMPLE : samplen="; LLM.DISTILLSAMPLEN(); " samplefail="; LLM.DISTILLSAMPLEFAIL(); _` (shared/samples/AiLLM/DistillPipeline.bas:65)
- **LLM.DISTILLSTATUS()** — `PRINT "BUILD  : "; LLM.DISTILLSTATUS()` (shared/samples/AiLLM/DistillPipeline.bas:49)
- **LLM.EVALGATE(x1, x2)** — `PRINT "EVALGATE     : "; LLM.EVALGATE("work/no_ref", "work/no_cand")` (shared/samples/AiLLM/TrainCommandsTour.bas:66)
- **LLM.FILEOUTLINE(x1)** — `PRINT "outline = "; LLM.FILEOUTLINE("ContextBuilderDemo.bas")` (shared/samples/Ai/ContextBuilderDemo.bas:57)
- **LLM.FINDSYMBOL(x1)** — `PRINT "FINDSYMBOL Unit    -> "; LLM.FINDSYMBOL("Unit")` (shared/samples/Ai/SemanticGraphDemo.bas:97)
- **LLM.FINETUNEBASEHOLD()** — `PRINT "  baseline hold = "; LLM.FINETUNEBASEHOLD()` (shared/samples/AiLLM/ModelFinetuneDemo.bas:54)
- **LLM.FINETUNEBASETRAIN()** — `PRINT "  train="; LLM.FINETUNELOSS(); " base="; LLM.FINETUNEBASETRAIN(); _` (shared/samples/AiLLM/LoraFinetuneMultiArch.bas:54)
- **LLM.FINETUNECKPT()** — `" changed="; LLM.FINETUNEDEMOCHANGED(); " ckpt="; LLM.FINETUNECKPT()` (shared/samples/AiLLM/LoraFinetuneMultiArch.bas:55)
- **LLM.FINETUNEDEMOCHANGED()** — `" changed="; LLM.FINETUNEDEMOCHANGED(); " ckpt="; LLM.FINETUNECKPT()` (shared/samples/AiLLM/LoraFinetuneMultiArch.bas:55)
- **LLM.FINETUNEDONE()** — `IF LLM.FINETUNEDONE() = -1 THEN` (shared/samples/AiLLM/LoraFinetuneMultiArch.bas:53)
- **LLM.FINETUNEGREEDY()** — `PRINT "  greedy base = "; LLM.FINETUNEGREEDY()` (shared/samples/AiLLM/ModelFinetuneDemo.bas:49)
- **LLM.FINETUNEGREEDYAFTER()** — `PRINT "  greedy after = "; LLM.FINETUNEGREEDYAFTER()` (shared/samples/AiLLM/ModelFinetuneDemo.bas:51)
- **LLM.FINETUNEHOLD()** — `PRINT "  hold  loss = "; LLM.FINETUNEHOLD()` (shared/samples/AiLLM/ModelFinetuneDemo.bas:47)
- **LLM.FINETUNELOSS()** — `PRINT "  train="; LLM.FINETUNELOSS(); " base="; LLM.FINETUNEBASETRAIN(); _` (shared/samples/AiLLM/LoraFinetuneMultiArch.bas:54)
- **LLM.FINETUNESTATUS()** — `PRINT "  non riuscito: "; LLM.FINETUNESTATUS()` (shared/samples/AiLLM/LoraFinetuneMultiArch.bas:57)
- **LLM.IMPACT(x1)** — `PRINT "IMPACT   BoundVal     -> "; LLM.IMPACT("BoundVal")` (shared/samples/Ai/SemanticGraphDemo.bas:75)
- **LLM.INSPECT(x1)** — `PRINT "INSPECT  UnitScale    -> "; LLM.INSPECT("UnitScale")` (shared/samples/Ai/SemanticGraphDemo.bas:93)
- **LLM.KERNEL(x1)** — `f = LLM.KERNEL("CODEC")` (shared/samples/AiLLM/PromptBudgetReport.bas:44)
- **LLM.KV(x1)** — `f = LLM.KV("SHOW")` (shared/samples/AiLLM/PromptBudgetReport.bas:49)
- **LLM.LORALIST()** — `PRINT "LORALIST     : "; LLM.LORALIST()` (shared/samples/AiLLM/TrainCommandsTour.bas:72)
- **LLM.LORALOAD(x1, x2)** — `LLM.LORALOAD("work/no_ad.safetensors", "work/train_tour_imp")` (shared/samples/AiLLM/TrainCommandsTour.bas:59)
- **LLM.LORASAVE(x1, x2)** — `LLM.LORASAVE("work/no_ckpt", "work/train_tour_ad.safetensors")` (shared/samples/AiLLM/TrainCommandsTour.bas:52)
- **LLM.MODULE(x1)** — `f = LLM.MODULE("SHOW")` (shared/samples/AiLLM/PromptBudgetReport.bas:39)
- **LLM.PATCH(x1)** — `PRINT "PATCH    UnitScale    -> "; LLM.PATCH("UnitScale")` (shared/samples/Ai/SemanticGraphDemo.bas:91)
- **LLM.POLICY(x1)** — `tmp = LLM.POLICY("SET", "FFN", "bf16")` (shared/samples/AiLLM/PromptBudgetReport.bas:29)
- **LLM.PREFILLSTATS(x1)** — `DIM ps$ = LLM.PREFILLSTATS("nessun-agente")` (shared/samples/Ai/AiBenchmark.bas:78)
- **LLM.REPOMAP()** — `PRINT "REPOMAP            -> "; LLM.REPOMAP()` (shared/samples/Ai/SemanticGraphDemo.bas:99)
- **LLM.REVISION()** — `PRINT "REVISION              -> "; LLM.REVISION()` (shared/samples/Ai/SemanticGraphDemo.bas:85)
- **LLM.RUNSTOP(x1)** — `PRINT "RUNSTOP      : "; LLM.RUNSTOP("99")` (shared/samples/AiLLM/TrainCommandsTour.bas:48)
- **LLM.RUNSCLEAR()** — `PRINT "RUNSCLEAR    : "; LLM.RUNSCLEAR()` (shared/samples/AiLLM/TrainCommandsTour.bas:142)
- **LLM.RUNSCOUNT()** — `PRINT "run registrate: "; LLM.RUNSCOUNT()` (shared/samples/AiLLM/TrainRuns.bas:22)
- **LLM.RUNSITEM(x1)** — `PRINT "RUNSITEM(1)  : "; LLM.RUNSITEM("1")` (shared/samples/AiLLM/TrainCommandsTour.bas:71)
- **LLM.RUNSJSON()** — `DIM js AS STRING = LLM.RUNSJSON()` (shared/samples/AiLLM/TrainRuns.bas:24)
- **LLM.STREAMSTATS(x1)** — `PRINT "m1 (swap)      : "; LLM.STREAMSTATS("m1")` (shared/samples/AiLLM/MoestreamModels.bas:49)
- **LLM.SYMBOL(x1)** — `PRINT "SYMBOL   BoundVal     -> "; LLM.SYMBOL("BoundVal")` (shared/samples/Ai/SemanticGraphDemo.bas:63)
- **LLM.TENSOR(x1)** — `info$ = LLM.TENSOR("TOKENIZER", model$)` (shared/samples/AiLLM/TokenizeRoundTrip.bas:42)
- **LLM.TOKENCOUNT(x1, x2)** — `PRINT "[token] "; LLM.TOKENCOUNT(model$, testo$)` (shared/samples/AiLLM/TokenizeRoundTrip.bas:34)
- **LLM.TOKENS(x1, x2)** — `ids$ = LLM.TOKENS(model$, testo$)` (shared/samples/AiLLM/TokenizeRoundTrip.bas:24)
- **LLM.TRAINCONFIG()** — `PRINT "TRAINCONFIG  : "; LLM.TRAINCONFIG()` (shared/samples/AiLLM/TrainCommandsTour.bas:43)
- **LLM.TRAINCONFIGSET(x1, x2)** — `LLM.TRAINCONFIGSET("steps", "12")` (shared/samples/AiLLM/TrainCommandsTour.bas:44)
- **LLM.TRAINEXPORT(x1)** — `PRINT "TRAINEXPORT  : "; LLM.TRAINEXPORT("work/train_tour_exp_f")` (shared/samples/AiLLM/TrainCommandsTour.bas:47)
- **LLM.VERIFY()** — `PRINT LLM.VERIFY()` (shared/samples/Ai/SemanticPatchApply.bas:38)
<!-- END DOMAIN TABLE -->
