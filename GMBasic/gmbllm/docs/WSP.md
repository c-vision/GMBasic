# WSP — Weight-Space Programming in `gmbllm` (C99)

Strumenti per **leggere, sezionare, modificare e riscrivere i pesi** di un modello, senza Python e senza ricompilare il
motore. Piano e storia delle decisioni: nota di piano "Weight-Space-Programming-LLVM-Pesi-Editabili"

**Stato (2026-11-05).** Libreria C completa per i modelli **densi** Qwen3/Llama (HF safetensors, anche MLX quantizzato in sola
lettura) con **superficie BASIC `WSP.*`** via FFI: libreria `libgmbwsp` (`shared/libs/wsp`, sorgenti venduti da qui con `build.sh`),
export `shared/include/wsp.inc`, 7 MiniTest in `shared/minitests/wsp/`, 4 sample in `shared/samples/Wsp/`, capitolo di manuale
`docs/manual/capitoli/V.37-weight-space-programming.md`. Verificata a parità sulle 4 gambe (`tools/sh/parity4`). Restano fuori P/Invoke C# dedicato
(non serve: l'FFI è la via) e la registrazione degli strumenti per `AI AGENT`.

## Moduli (tutti in `gmbllm_core`, test in `tests/test_wsp_*.c`, registrati in `ctest`)

| Fase | Header | Cosa fa |
|---|---|---|
| W1 Weight Store | `llmwsp_store.h` | apre dir safetensors (shard o file singolo), indice, **slice-read** senza leggere il tensore intero, sha256, copertura dei byte, **writer a streaming** |
| W2 Documento | `llmwsp_doc.h` | sezionamento completo `gmb.wsp@1`: embed → layer (norme, attn, mlp) → norma finale → lm_head; ruoli; **partizioni** (`heads`, `kv_heads`, `neurons`, `tokens`); indirizzi di slice; check di copertura/coerenza |
| W3 Materializer | `llmwsp_mat.h` | scrive un checkpoint da store + sostituzioni di tensore; shard intatti **clonati** (`clonefile`), solo lo shard modificato riscritto |
| W4 Analisi | `llmwsp_linalg.h`, `llmwsp_analyze.h`, `llmwsp_fixture.h` | SVD di Jacobi, autovalori non simmetrici, angoli principali; norme per testa, spettro **OV/QK** (GQA), **copy score** (2 definizioni), composizione/Projection Kernel; fixture compilate a verità nota |
| W6 Overlay | `llmwsp_overlay.h` | edit copy-on-write (`SET/SCALE/ZERO/ADDDELTA/COPY/PROJECTOUT`), journal, undo/redo, **diff**, `RECIPE`/`REPLAY`, **patch sparsa** a slice |
| W7 Chirurgia | `llmwsp_derive.h` | layer remove/dup/reorder, potatura di **gruppi kv** (GQA) e di **neuroni MLP**, `config.json`/`layer_types`/`.gmbarch` coerenti, validazione prima e dopo la scrittura |
| W8 Verifica | `llmwsp_verify.h` | `VERIFY` (documento, copertura, finitezza, confronto con la base), **gate** con soglie dichiarate, hash di modello, catena di provenienza |
| W9 Sessione | `llmwsp_agent.h` | sessione a verbi testuali (sandbox di dati) con **budget**, trascrizione JSONL e **replay deterministico** |

Dipendenza dal motore (solo per W5): `llm_ai_mlx_capture_layers` in LLMEnv (cattura del residual per layer).

## Formati

- **`gmb.wsp@1`** (`llmwsp_doc_to_json`): `{format, model_type, config{hidden_size,…}, ties[], coverage{tensors,bytes,unclassified}, leaves[]}`.
  Ogni foglia: `addr`, `role`, `layer`, `tensor`, `dtype`, `shape`, `file`, `offset`, `nbytes`, `partitions[{name,axis,count,size}]`.
  Invariante: ogni tensore dello store è in esattamente una foglia; Σbyte foglie = Σbyte store; le partizioni tassellano l'asse.
- **Indirizzi**: `layers.<L>.attn.{q,k,v,o,q_norm,k_norm}`, `layers.<L>.mlp.{gate,up,down}`, `layers.<L>.{attn_norm,mlp_norm}`,
  `embed`, `final_norm`, `lm_head`; suffissi `_bias/_scales/_biases`; slice: `<foglia>.<partizione>.<indice>` (es. `layers.5.attn.o.heads.7`)
  oppure `@<asse>:<da>:<a>:<nome tensore>` per una slice arbitraria (qualunque famiglia).
- **`recipe.gmbderive`** (overlay): una riga per edit (`SCALE addr %a`, `ZERO`, `COPY`, `SET addr n blob_N.f64`, `ADDDELTA`,
  `PROJECTOUT addr asse flags ndirs len blob`); i vettori in `blob_N.f64` (float64 LE). `REPLAY` produce lo stesso sha256.
- **Patch** (`WSPPTCH1`): per tensore modificato, la slice più stretta (righe o colonne) con i byte nuovi.
- **Trascrizione** (`transcript.jsonl`): `{seq, cmd, status, out_sha256, out, ms}` per comando; `run.json` con audit e budget.
- **Provenienza** (`chain.json`): `base` → `ops` (= sha256(hash_base ‖ ricetta)) → `output`, più ricetta e gate. `wsp_summary.txt` accompagna ogni
  `llmwsp_ov_materialize` (nessuna scrittura silenziosa); `provenance.json` per-edit è scritto dal materializer.

## Strumenti da riga di comando (`tools/`, si compilano con `cc`, nessuna dipendenza)

| Tool | Scopo |
|---|---|
| `wsp_gen` | carica un modello sul motore MLX (dlopen) e genera greedy N token da id: verifica che un checkpoint materializzato/derivato **si carichi e generi** |
| `wsp_cap` | verifica `capture_layers` (ultimo layer ≡ `capture_hidden`, decode invariato) |
| `wsp_probe` | sceglie il blocco di layer da rimuovere per **similarità fra rappresentazioni** (Gromov) e, se richiesto, deriva il modello |
| `wsp_eval` | **PPL** con teacher forcing e **KL troncata ai top-32** contro un riferimento; soglia opzionale (codice 3 = rifiutato) |
| `wsp_apply_edit` | applica in C una edit `sparse` di `lm_understand-logs` (`edit.npz` + manifest) verificando gli sha256 |
| `wsp_math` | MATH-500 ridotto con protocollo di campionamento degli autori + **misura di loop** (8-grammi ripetuti) |

## Risultati misurati (e limiti)

- Qwen3-8B: documento 399/399 tensori, 16.381.470.720 byte; round-trip tensor-identico; RSS ≈ 7 MB nello store/writer.
- Materializzato senza edit = originale (decode greedy identico, KL 0). Derivati rifiutati dal gate a 0,05 senza guarigione; il blocco scelto per
  similarità batte quello a ritroso (PPL 30,8 vs 91,6 per −4 layer). **Importanza dei neuroni sui soli pesi = baseline debole** (−5% neuroni: PPL ×4).
- Esp. 8 del paper (Qwen3.5-0.8B): struttura dell'edit confermata (sha256 e 64 colonne × 2 tensori); a tetto 3.072 token l'edit riduce le ripetizioni
  (media 0,155 vs 0,230; 26/30 appaiati) ma **non** cambia accuratezza né quota di risposte tagliate. Il guadagno di accuratezza non è riprodotto.
- Limiti noti: circuiti QK senza RoPE/qk-norm; KL troncata (stima per difetto); un solo testo/seme per i confronti PPL.

## Politica d'uso

Le primitive di edit sono **generali**. Il progetto non include ricette né dataset per rimuovere protezioni di sicurezza di un modello.
Ogni edit resta nel journal e nella trascrizione; la sessione agente espone solo verbi su dati (nessuna rete, shell o file arbitrari).

## Non fatto

W5 completo (attn_out/mlp_out, probabilità di attenzione, `RANK1`/`NULLSPACE`), `VOCAB.TRIM`, `EXPERT.PRUNE`, GGUF/MLX come sorgente/uscita del
materializer, guarigione LoRA dei derivati, superficie BASIC e P/Invoke, registrazione degli strumenti per `AI AGENT`.
