# RMLX-10 — decode compiled: piano di esecuzione (dal 2026-09-03)

Obiettivo: chiudere il residuo ~1.5-3x del decode vs rapidMLX (mlx.compile + graph-reuse) sui
modelli MoE grandi (Mixtral, 35B-A3B, Coder, A10B). Regola suprema: copiare/adattare 1:1 dalla
reference (rapidMLX `vllm_mlx/`, mlx_lm cache.py); nessuna ipotesi custom.

## Fatto accertato (pre-requisito)

`mixtral_attention_block` aggiorna la KV con `mlx_concatenate_axis(&k_full, kv, 2)` **a ogni step**:
le shape del passo crescono → il decode-step intero NON e' compilabile (mlx.compile richiede shape
statiche). mlx_lm eager (reference) fa lo stesso concat e infatti non compila il decode. rapid usa
cache a buffer fisso.

Misura odierna (engine vs mlx eager, Mixtral): decode ctx corto engine ~5.3 vs mlx ~7 (1.3x);
ctx 6658: engine (ctx4096 ~2.9-4.0) PIU' veloce di mlx eager (~1 tok/s). Il gap grosso e' verso
rapid (compile+paged), non verso mlx eager.

### Analisi di fattibilità (2026-09-03, dal codice)

- `mixtral_attention_block` (riga ~3787): cache `[1,n_kv,L,hd]`, ogni step `k_full=concat(k_cache,k_rope,axis=2)`.
- `gmb_mlx_gqa_sdpa` (`gmb_mlx_ops.h:113`) con window<=0 delega a `mlx_fast_scaled_dot_product_attention(...,"causal")`
  che deriva la lunghezza chiavi dalla SHAPE di k (offset=kL-qL, fast.cpp). Cache padded a shape statica
  => la modalita' causal guarderebbe i token futuri (zeri). PER SDPA su cache statica servono: shape statica
  padded `[1,n_kv,max_ctx,hd]` + MASCHERA ADDITIVA mode="" con 0 per j<=pos e -inf oltre (la mask e' dato,
  non shape) — stesso meccanismo del branch window (gmb_mlx_ops.h:67-105).
- Scrittura posizionale con le sole op mlx = where(rowsel, k_new, old) => copia O(max_ctx) per step per layer
  (no in-place in mlx). A max_ctx>=4096 e' ~ concat; NON basta per rapid-parity: serve un **kernel Metal
  custom di scatter in un buffer persistente** + SDPA che legge buffer+offset (sottosistema KV dedicato,
  non un patch). Per la sola Fase B (compile) la maschera additiva + where-update bastano come primo step
  funzionante; il kernel in-place e' l'evoluzione successiva.
- Runner: cache per-layer create nel loop del decode (vedere k_cache/v_cache init e `has_cache`); dimensione
  max_ctx = max_len effettiva (non 128k: allocare solo la max vista, con trim rolling kv_max_tokens).

## Fase A — KV cache a buffer fisso (prerequisito, value da solo)


1. In ogni attention block (mixtral, qwen3_moe, qwen3_5, gpt_oss, glm4_moe) sostituire il concat
   crescente con buffer preallocato max_ctx: `k_cache = mlx_zeros([n_layers? n_kv_h, max_ctx, hd])`
   allocato a load/`new_cache`, e per-step scrittura posizionale a shape statica.
   Reference layout: `mlx_lm/models/cache.py` `KVCache` (update_and_fetch) e mixtral.py uso cache.
2. La lettura attenzione usa `cache[:, :, :pos, :]` (slice a shape variabile SOLO sul lato
   attention) — posizione passata come scalare; il grafo decode rimane shape-statico per
   k/v nuovi (slice read puo' restare shape-dipendente fuori dal compile, o usare mask max_len).
3. Gate: parita' numerica vs eager prima e dopo (layer trajectory / decode tokens).
4. Bonus atteso anche senza compile: niente copia O(ctx) per step (concat) -> ctx lunghi migliorano.

## Fase B — closure compiled del decode

1. Plumb `mlx_closure_new_func_payload` (closure.h) + `mlx_compile` (compile.h) in
   `gmb_host_ai_mlx`. Funzione C `step_fn(outputs, inputs, payload)`: riceve array di input a
   shape statica (h del token, pesi, buffer k/v, pos scalare) e costruisce il grafo del passo
   (attenzione+ffn tutte le layer) restituendo i lazy output; `mlx_compile(closure, shapeless=false)`.
2. A load/compile: si traccia il grafo (prima call con quelle shape); a ogni token si richiama la
   closure compilata con i nuovi array (shape uguali) -> cache hit -> metal compilato/fuso.
   Compile per per-architettura; `mlx_compile_mode` gate via env (off default).
3. Verifica: parita' decode Mixtral vs eager (token identici a dato seed), poi tok/s a ctx8/64/4096.
4. Attenzione gestione memoria: il compile tiene i grafi tracciati vivi (cache) -> verificare
   peak GPU (la cache del compile NON e' il limite di memoria live gia' usato).
   Interazione con `mlx_set_cache_limit` (linea non committata ~7057) da decidere.

## Fase C — misure target (GPU pulita, dopo warmup termico)

- Mixtral decode ctx8/64/4096 vs mlx eager e vs rapid (stesso prompt) -> atteso: eager-parity
  a breve, avvicinarsi a rapid (~17 ctx4096).
- 35B-A3B, Coder, A10B decode @ctx4096.

## Caveat

- compile con `shapeless=false` richiede shape identiche OGNI step: batch=1 fisso, top_k fisso,
  buffer KV fisso. Cambi di lunghezza prompt (prefill vs decode) = 2 trace separate.
- Non toccare il path eager: tenere `GMB_NO_COMPILE=1` come fallback.
- No double-free: closure compilata possiede il grafo; gli array lazy intermedi restano gestiti
  dal refcount mlx-c (stesso schema del path attuale).

Commit collegati: sorted-gather 3c667c9b/59cb0edb; fix memoria 91fb3aba; doc MLX-MoE-Sorted-Gather.

---

## Baseline misurata 2026-09-08 (M5 Max, load 2,7→4,5; dettaglio nel vault LLMEnv)

Qui sopra il gap «1,5-3×» era **stimato su Mixtral**. Oggi è misurato modello per modello, stesso
giorno, stessa macchina, braccia adiacenti (decode `gen=128`, 3 rep, ordine alternato; rapid =
`/v1/completions` streaming M=128, 2 rep). **tok/s decode | prefill@4096**:

| modello (model_type, quant) | motore fp32 | motore bf16 | rapidMLX | esito |
|---|---|---|---|---|
| Qwen3.6-27B-4bit (`qwen3_5`, affine 4/64) | 25,3 | **31,5** | 28 \| 680 | bf16 **promosso default** (`74bd7a61e`): 1,245×, **1,13× vs rapid** |
| Qwen3.8-27B-8bit (`qwen3_5`, affine 8/64) | 13,9 | **16,7** | 15-16 \| 586 | bf16 **promosso default** (`a496b9854`): 1,20×, **1,07× vs rapid** |
| DeepSeek-R1-32B-8bit (`qwen2`, 8/64) | 10,3-11,9 | 10,5 (bf16 **inerte**) | 15-16 \| 326 | **0,67× vs rapid — gap aperto, e non è il dtype** |

Il gate di qualità che ha autorizzato le due promozioni (teacher forcing su testo reale, 600
posizioni, braccia nello stesso processo, ordine alternato): loss nats/token e top-1 contro il testo
umano sono **a parità o migliori in bf16** — 4-bit `0,6711/324` fp32 → `0,6677/326` bf16; 8-bit
`0,6877/325` → `0,6872/327`; nessun `P(argmax)>1`. Quest'ultima colonna, fino a ieri, era una lettura
**falsa**: `mlx_array_data_float32` in mlx-c è una reinterpret cast, non una conversione
(`2167d7458`) — la stessa causa del SIGSEGV del sampling con bf16 attivo.

### Ipotesi ESCLUSE con la misura per il residuo DeepSeek-32B (non a parole)

- **bf16**: 1,03× su `qwen2` (inerte) contro 1,20-1,245× su `qwen3_5` → il win è arch-dipendente.
- **`GMB_GU_FUSE`** (gate+up in una sola GEMV): 1,02× = rumore.
- **`GMB_GEMVQ4`** (kernel Metal custom della «proposta 614 GB/s»): **zero** sul 4-bit (31,3-31,5
  identico, IDS identici) — lì `qmv_fast` di mlx è già a ~505 GB/s.
- **collo di KV/concat**: la decodifica è **piatta da ctx 64 a 4096** (9,0 → 7,7 tok/s), e
  `GMB_PHASE` ripartisce il passo in attn 27 ms + ffn 80 ms (il FFN è il 75%).
- **costo fisso per `generate`**: già falsificato (G7, `f≈0,18 s`).

Residuo probabile: **bandwidth-efficiency della GEMV 8-bit del FFN su quelle shape** (~360 GB/s
contro i ~490-505 GB/s raggiunti dagli stessi 8-bit su `qwen3_5`). Prossimo passo onesto:
micro-harness che misuri gate/up (M=27648, K=5120) e down (M=5120, K=27648) **separatamente**, prima
di scrivere qualunque kernel Q8 in `native/gmb_metal_gemv/` (che oggi contiene SOLO
`gmb_metal_gemv_q4.h`). Fase A/B di questo documento (FIXKV/COMPILE) non c'entra con questo residuo:
sono gateate su `mixtral` e il difetto qui è dentro il FFN denso.

Strumenti e log di questa sessione in `/tmp/gmb-l8-gate/` (`ab8.sh`, `ab9.sh`, `ab11.sh`,
`gate-stats.py`, `gate-*.log`); `test_mlx_bench` è gitignorato, quindi le modalità `score` e
`sampletext` esistono solo nel working tree.

## 2026-09-08 notte — L19 CHIUSo: il residuo DeepSeek era il dtype, come COPPIA (x, scale)

Il paragrafo «Residuo probabile» qui sopra è **superato dalla misura**: il micro-harness separato
che suggerivo è stato fatto (`/tmp/gmb-l19/gemv3.py`, shape reali lette dai checkpoint, 16 op per
eval, canary di banda ad ogni misura, 2 round ordine alternato) e **non serve alcun kernel Q8**.

**E la diagnosi «il candidato del kernel è `gate_proj`/`up_proj`, non `down_proj`» puntava la GEMV
sbagliata**: gate+up e down leggono lo stesso numero di byte (27648×5120 in entrambi i versi), e a
coppia `(x, scale)` corretta le due time **collimano** — `f16: gate 0,325 ms` vs `down 0,324 ms`
(rapporto 0,997), mentre con `x` fp32 la coppia era 0,393/0,415. L'asimmetria inseguita era in
buona parte un effetto del dtype sbagliato, non una proprietà della shape: un kernel scritto su
`gate+up` avrebbe ottimizzato il sintomo. Nessun permesso su CMake era necessario.

**Riconciliazione con la misura in-engine: PARZIALE, registrata onesta.** La sessione separata ha
rifatto `GMB_FFN_PHASE` con `bf16=f16` (stesso branch `l19-ffn-gemv-diag` rebasato su
`f5722b617`): rapporto **2,45**, non ~2,0 → si muove **nella direzione predetta** (2,60 → 2,45) ma
un residuo di asimmetria sopravvive dentro il forward reale, dove il micro-harness isolato non
vede sigmoid/multiply intermedi, lookup LoRA e allocazioni. Quanto vale quel residuo lo dice il
tetto, non la polemica: con entrambi i motori a **508-515 GB/s su ~520 misurati**, il premio
massimo ancora disponibile sul decode DeepSeek è **~7%** (teorico 546), e la stessa cifra è il
soffitto di qualunque kernel Q8 nuovo. Non è «settimane di lavoro garantito»: è ≤7%, e
l'esperimento più vicino (GEMVQ4 su Q4) l'ha già dimostrato nullo.

`mlx_quantized_matmul` (8-bit affine/64, B=1) prende il kernel veloce **solo se il dtype di `x`
coincide con il dtype delle `scales` del checkpoint**. `t/step` medio per op:

| shape | x=fp32 | x=f16 | x=bf16 |
|---|---|---|---|
| DeepSeek gate `[27648×5120]` (scale **F16**) | 0,393 ms | **0,325** | 0,414 |
| DeepSeek down `[5120×27648]` | 0,415 | **0,324** | 0,405 |
| DeepSeek lm_head `[152064×5120]` | 1,924 | **1,534** | 2,164 |
| Qwen3.8 gate `[17408×5120]` (scale **BF16**) | 0,351 | 0,364 | **0,309** |

bf16-su-scale-f16 è il mix **peggiore** (sotto fp32): è l'unica lettura che riconcilia il «+25%
per-op bf16» (Qwen3.8, coppia giusta) con il «1,03× model-level bf16 su qwen2» (coppia sbagliata)
già in tabella. Survey dei dtype delle scale (soli header safetensors, tutti i checkpoint locali):
**F16** → DeepSeek-R1-32B-8bit, Llama-3.2-1B-4bit, Mixtral-8x22B-4bit, Ternary-Bonsai-2bit;
**BF16** → GLM-4.5-Air, Qwen3-Coder-30B-8bit, Qwen3.5-27B-6bit, Qwen3.6-27B-4bit,
Qwen3.6-35B-A3B, Qwen3.8-27B-8bit, Qwen3.8-Flash-Next, gemma-4-31b-4bit, gpt-oss-20b;
**U8 (mxfp4)** → Coder-Next, Qwen3.5-122B-A10B, gpt-oss-120b.

**Codice** (`LLMEnv/LLMEngine/mlx/gmb_host_ai_mlx.c`): i ~20 siti «bit a 16 bit = bf16» passano
da un unico accessore (`g_act16`, `gmb_mlx_act_dt()`, `gmb_mlx_is_act16()`, `gmb_mlx_act16_name()`);
`gmb_mlx_bf16_mode_parse` è il solo punto che scompone una mode nello stato completo (due bit +
flavor) e `gmb_ai_mlx_model_set_bf16_mode` accetta `"f16"`/`"fp16"`/`"bf16"` — **nessuna nuova
symbol esportata, nessuna modifica di ABI/C#/CMake** (quindi niente problema-L16 delle 23 copie).
Il corto circuito di no-op confronta anche il flavor; setter booleani legacy e default
architetturale pinnano bf16 (`g_act16` è un globale di file); guardie `GMB_KVMETAL` corrette a
«non qualunque 16 bit» (il kernel Metal assume fp32).

**A/B model-level su DeepSeek-R1-32B-8bit** (`decode 128`, 3 rep, ordine alternato, load macchina
12,6 → 26,1: l'assoluto non è usabile, solo il rapporto): mediana `off 9,3` | `bf16 9,5` = **1,02×**
| `f16 11,8` = **1,27×**, con f16 primo in tutte e tre le rep anche nel campione più carico e **IDS
greedy dei 128 token identici fra le tre braccia** (nessun overflow fp16). Nel loop `score` (lm_head
per posizione, quindi GEMV più grossa) f16 vale **1,52×** su off.
Baseline nota in finestra quieta: `off 11,5` tok/s → `11,5 × 1,27 ≈ 14,6` contro i `14,32` di
rapidMLX misurati lo stesso giorno.

**Confermato, non più estrapolato — PARITÀ misurata nello stesso window.** La macchina ha 128 GB
(non serviva la «finestra quieta»): si alternano i due motori **a turno, mai simultanei** — due
workload bandwidth-bound in parallelo si dividono la banda e il rapporto collassa a 1,0 per legge
fisica, non per merito. Canone rapid corretto = **tempo inter-token della stessa chiamata
streaming** (`(n-1)/(t_ultimo - t_primo)`), canone motore = il cronometro solo-decode di
`test_mlx_bench decode 128`; `nchunk == completion_tokens == 128` verificato a ogni misura.

| finestra | motore `f16` (3 rep) | rapidMLX (3 rep) | ancora `off` |
|---|---|---|---|
| A (load 2,8–7,6) | 13,30 / 13,12 / 13,18 | 11,34\* / 13,39 / 13,34 | 10,59 |
| B (load 3,7–5,5) | **14,60** / 13,00 / 12,80 | **14,80** / 14,78 / 11,85\* | 9,00 |

\* = artefatto da scartare, non numero: in A il rep1 rapid è la prima richiesta su server appena
alzato (TTFT 10,76 s); in B il rep3 rapid ha `win` 10,72 s contro 8,58 (picco di carico esterno non
visto dalla media a 1 minuto). **La lettura corretta è la coppia adiacente migliore: 14,60 contro
14,80 = 0,986×**, e in A 13,18 contro 13,365 = **0,986×**. Il rapporto è lo stesso in entrambe le
finestre. In termini di banda: `14,60 × 34,81 GB/token = 508 GB/s` contro `14,80 × 34,81 = 515
GB/s`, canary di roofline ~520 → **entrambi i motori sono al 97-99% del tetto della macchina**; il
gap DeepSeek passa da **0,80× a 0,98×** (era 0,67× prima della correzione del canone).

**Canone old, ritrattato:** `completion_tokens / (t_nonstream − TTFT_di_un'altra_chiamata)` — la
formula usata fino ad ora — **gonfia il numero** quando la seconda chiamata trova già tutto caldo:
nella finestra di mezzo ha prodotto `19,30 tok/s = 672 GB/s`, fisicamente impossibile sopra il
tetto. Ogni numero rapid precedente ottenuto così va considerato un **limite superiore**, non una
misura. Strumenti: `/tmp/gmb-l19/ab-vs-rapid-f16.py` (log `ab-vs-rapid-f16.log` = canone vecchio,
`ab-vs-rapid-f16b.log` = canone inter-token).

**Gate di qualità CHIUSO** (`/tmp/gmb-l19/gate-ds-f16.log`, teacher forcing 600 posizioni × 3 rep
× 3 braccia = 1800 passi per braccio, load macchina 16 → 30 durante la corsa):

| braccio | loss/token (nats) | top-1 | `score` tok/s (3 rep) | mediana |
|---|---|---|---|---|
| `off` (fp32) | **0,5862** | 936/1800 (52,0%) | 7,56 / 8,02 / 7,81 | 7,81 |
| `bf16` | **0,5860** | 936/1800 (52,0%) | 7,32 / 8,35 / 8,55 | 8,35 |
| `f16` | **0,5865** | 933/1800 (51,8%) | 11,49 / 11,59 / 11,69 | 11,59 |

Lettura onesta: `f16` paga **+0,0003 nats/token** (+0,05%, sotto la terza cifra significativa) e
**3 posizioni su 1800** di top-1 (0,17%) — e sono sempre le *stesse* 3, perché il greedy è
deterministico e il delta è identico in tutte e tre le rep. Contro **1,486×** di decode nel loop
`score`, dove la GEMV dominante è la `lm_head` (che nel micro-harness valeva 1,25×). Non è
«gratis»: è un baratto misurato, e il prezzo sta dentro il rumore di qualunque uso reale.
Secondo segnale, non cercato: la spread di `f16` è **11,49–11,69** (±0,9%) mentre `off` vaga
**7,56–8,55** (±6,5%) nella stessa finestra condivisa — consumando banda minore, `f16` è anche meno
sensibile al rumore della macchina.

**Conseguenza APPLICATA (commit `d8ca31990`, stesso file): il flavor si sceglie dal checkpoint.**
`gmb_mlx_scales_dtype(&m->weights, ...)` campiona il dtype delle `.scales` della prima proiezione
quantizzata del layer 0 (multishard, `s.ctx != NULL` = tripla davvero quantizzata), e
`gmb_mlx_auto_pair_act_dtype(m, dtype_explicit)` la applica come **default** subito dopo
`load_all_shards` — il punto più presto in cui il dtype esiste, perché vive nei safetensors e non
nel config. Regole di precedenza, tutte verificate a macchina semi-silentata:

| modello | cosa stampa | decode |
|---|---|---|
| DeepSeek-R1-32B-8bit, **nessun env** | `[bf16:auto pair scales=F16] mode=f16 dt=f16 embed=1 matmul=1` | **15,2 tok/s** (era 11,5 fp32) |
| DeepSeek, `GMB_BF16_DENSE=off` | `[bf16:env GMB_BF16_DENSE] fp32 esplicito`, nessuna riga auto | 11,9 → rapporto **1,28×** (il noto 1,27) |
| Qwen3.6-27B-4bit (promossa bf16) | `[bf16:default] mode=both dt=bf16`, nessuna riga auto | 31,3 = il numero promosso, **invariato** |
| Llama-3.2-1B-4bit | nessuna riga bf16 | invariato (esclusa dal task #5) |

**Scope deliberato (non "tutti i modelli con scale F16"):** la promotion automatica è limitata a
`model_type=qwen2` + `quant_mode=affine` + `quant_bits=8` + scale **F16**, cioè esattamente la
configurazione con gate di qualità verde e parità vs rapid misurata. La guardia
`if (m->cfg.bfloat16_input || g_bf16_all) return;` mette **prima** di ogni altra cosa le famiglie
già promosse: `mixtral`/`qwen3_moe` hanno scale F16 e stanno bene in bf16 (+40-50%, GEMM batched),
quindi accoppiarle per analogia sarebbe esattamente l'errore documentato in §Limiti.
Il kill-switch resta triplo: `GMB_BF16_DENSE=off`, `mlxComputeDtype="off"` nel config, e
`gmb_ai_mlx_model_set_bf16_mode(handle,"off")` a caldo (l'esplicito vince sempre, e l'auto non
parte se l'ospite ha già chiesto qualcosa).

**Limiti da non dimenticare:** (a) la regola della coppia è misurata su **GEMV densa B=1** —
`mixtral` è promosso bf16 (+40-50%, misurato) pur avendo scale F16 perché i MoE fanno GEMM batched:
nessuna promozione va estesa per analogia, ogni famiglia richiede il suo gate; (b) trovato e **chiuso
nello stesso commit**: `g_bf16_all`/`g_act16` non erano azzerati in `gmb_ai_mlx_load_model`, quindi un
secondo modello nello stesso processo ereditava i bit del primo — innocuo finché era solo un default
per-architettura, **non** innocuo ora che l'accoppiamento automatico legge quello stato per decidere
(`if (m->cfg.bfloat16_input || g_bf16_all) return;`). Azzerati prima del default per-modello.
Strumenti: `/tmp/gmb-l19/{gemv3.py,ab-flavor-ds.sh,gate-ds-f16.log}`.

## 2026-09-08 notte fonda — il confronto vs rapid SU UNA SECONDA FAMIGLIA, rifatto col canone corretto

Serve perché il numero che stava in questo documento (`Qwen3.6-27B-4bit`: motore **31,0** vs rapid
**28**, «1,11× avanti») era costruito sul canone **ritrattato** §sopra: quel canone gonfia rapid,
quindi «28» era un limite superiore sbagliato e il vantaggio dichiarato **non era misurato**. Nuove
requisizioni con lo strumento `/tmp/gmb-l19/ab-vs-rapid-family.py` (canone inter-token, validazione
`nchunk == usage.completion_tokens`, 3 rep alternate, MAI simultanee), braccio motore = **nessun
env** = il default promosso a load-time che è quello che usano realmente gli utenti:

| rep | ordine | load | engine (bf16 default) | rapid | rapporto |
|---|---|---|---|---|---|
| 1 | eng → rapid | 13,2 / 13,0 | 17,60 | 16,97 (`ct=128 nchunk=128`) | 1,04× |
| 2 | rapid → eng | 12,4 / 14,1 | 19,60 | 14,88 (`ct=128 nchunk=128`) | 1,32× |
| 3 | eng → rapid | 17,0 / 16,5 | 20,60 | 16,44 (`ct=128 nchunk=128`) | 1,25× |

**Mediane 19,60 vs 16,44 = 1,19×**; rapporto per rep 1,04–1,32, mediana dei rapporti **1,25×**.
`nchunk == ct` in tutte e tre: il canone tiene. La direzione (il motore NON è dietro su questa
famiglia) è **confermata**; il valore preciso no, e i valori assoluti sono **¼ sotto** la finestra
quieta (31,3 → ~19,6) perché la macchina era condivisa (load 13→25 durante la corsa, server rapid
di un'altra AI attivo): su questa macchina conta solo il rapporto, mai il tok/s.

**Asimmetria da non dimenticare in questa misura:** il braccio motore usa la modalità `decode` del
bench, che prefilla **64 token sintetici**, mentre rapid ha ricevuto il prompt reale da 1800 caratteri
(~450 token). Il decode **non è piatto** su quel range (documento §Ipotesi escluse: 9,0 → 7,7 tok/s
da ctx 64 a 4096, ≈−14%), quindi il rapporto qui sopra è **drogato al massimo di ~1,14×** a favore
del motore. Detto in cifre: anche togliendo tutto il bias possibile, resterebbe ~1,10× — il che
riporta il numero nell'ordine di grandezza del «1,11×» precedente, ma **da misura con canone sano**,
non da canone che gonfiava il confronto. L'ancora `GMB_BF16_DENSE=off` (14,10) **non è
confrontabile**: corsa per ultima a load 21,3, senza una sorella bf16 adiacente a quel load.

## 2026-09-08 22:33 — L21: la coppia (x, scale) vale ANCHE sul PREFILL, e la promozione era già il default spedito

Il «deficit maggiore ora noto è il prefill» scritto sopra presupponeva che il dtype a 16 bit
cambiasse solo la GEMV B=1. **Misura che lo smentisce** (`/tmp/gmb-l19/prefill-dtype.sh`, log
`prefill-dtype.log`): prompt **reale** da ~9000 token (`docs/comparison/RMLX10-decode-compiled-plan.md`
+ i due corpus, tokenizzati dal `tokenizer.json` del modello), braccia **nello stesso processo** in
ordine alternato (`score`, `nScored=8` perché i passi di scoring costano un `lm_head` extra e qui non
c'entrano), e il numero utile è di nuovo il **rapporto**.

DeepSeek-R1-32B-8bit, `prompt=9321` token, tempo di prefill (s, 3 rep):

| braccio | rep0 | rep1 | rep2 | mediana | tok/s | vs fp32 |
|---|---|---|---|---|---|---|
| `off` (fp32) | 46,20 | 45,24 | 44,55 | 45,24 | 206 | 1,00× |
| `bf16` | 41,42 | 48,58 | 47,76 | 47,76 | 195 | **0,95×** (peggio) |
| `f16` | 25,97 | 27,11 | 31,54 | **27,11** | **344** | **1,67×** |

Qwen3.6-27B-4bit, `prompt=9019` token (stessa famiglia di scale BF16, quindi il flavor giusto è bf16):

| braccio | rep0 | rep1 | rep2 | mediana | tok/s | vs fp32 |
|---|---|---|---|---|---|---|
| `off` (fp32) | 26,25 | 26,79 | 26,02 | 26,25 | 344 | 1,00× |
| `both` (bf16) | 18,48 | 18,85 | 18,95 | **18,85** | **479** | **1,39×** |

**Cosa segue, e perché conta più della somma dei numeri:** (1) la regola della coppia **non è una
proprietà del decode**: sul DeepSeek il bf16 è il flavor *sbagliato* anche a M grande (0,95×, dentro
il rumore ma definitamente non avanti) mentre f16 vale **+67%** — più del +28% di decode, quindi la
promozione di `d8ca31990` ha spostato il **segmento che era rimasto indicato come il deficit
maggiore**, senza una riga di codice nuova; (2) il `+39%` su Qwen3.6 è lo **stesso default** già
spedito (ramo `qwen3_5`), che finora era stato validato solo sul decode: il prefill promosso non è
una richiesta utente, è ciò che gira; (3) `top1controTesto=3` sulle 8 posizioni scored è **identico
in tutte e 9 le corse** (non è il gate di qualità — quello resta la sessione da 1800 posizioni — ma
è il segno che il Δ prefill non sta muovendo l'argmax su testo reale).

**Confronto vs rapidMLX, finito lo stesso minuto** (`/tmp/gmb-l19/prefill-vs-rapid.py`, log
`prefill-vs-rapid-{ds,qwen}.log`, canone dichiarato per parte: `ntok/prefill_s` il motore,
`prompt_tokens/ttft` rapid — il quale include un passo di decodifica + tokenizzazione + coda HTTP e
quindi **sottovaluta** rapid; `--disable-prefix-cache`, 3 rep alternate, MAI simultanee, prompt reale
uguale da ~26 KB → ~9000 token per parte):

| famiglia | N | engine (3 rep) | mediana | rapid (3 rep) | mediana | rapporto |
|---|---|---|---|---|---|---|
| Qwen3.6-27B-4bit (`both`, **default promosso**) | 9019/9027 | 419,5 · 431,1 · 434,9 | **431,1** | 432,2 · 424,9 · 440,7 | **432,2** | **0,99×** |
| DeepSeek-R1-32B-8bit (`f16`, **default promosso**) | 9321/9330 | 304,0 · 155,1 · 296,4 | 296,4 | 298,8 · 329,6 | 314,2 | 0,94× (mediane) / **1,03×** (rep appaiate 1 e 3) |

**Verdetto: il prefill NON è un deficit.** Su Qwen3.6 la misura è pulita (load 4-9, spread ±2% su
entrambi i lati) e dice **parità**: 0,971 / 1,015 / 0,987 rep per rep. Sul DeepSeek la mediana engine
è **drogata al ribasso** da un picco di load 17,3 caduto solo sulla gamba engine della rep2 (60,09 s
contro 30,66/31,45): escluse le due misure non adiacenti, le rep accoppiate 1 e 3 danno **1,017× e
1,002×**, cioè di nuovo parità. Quindi il «deficit maggiore ora noto è il prefill (1,2-1,4× dietro)»
scritto sopra — e la riga `Qwen3.6-27B: prefill 185 vs rapid 425 = rapid 2,3×` nella tabella
cumulativa di `GMBasic-vs-RapidMLX-Comparison.md` §13.3 — sono **falsificati da questa misura**: quel
185 era precedente alla promozione bf16 (e comunque quel ramo oggi gira a **431-479** tok/s).

**Ciò che resta aperto, dopo L19-L21, non è più il dtype:** (a) le famiglie **MoE/mxfp4** (il gap
grande della §13.3: 122B-A10B decode 4,2×, gpt-oss-120b 4,1×), (b) il **decode a contesto molto
lungo** (N=16384: «2,4×» su DeepSeek, §19 — mai rifatto con la coppia corretta, ed è l'unico numero
di quella riga che la promozione automatica non tocca di per sé), (c) llama/bf16-correttezza (task
#5). Il denso a contesto medio è **appaiato o avanti su entrambi i segmenti**.
> **Aggiornato da L22 (stessa serata):** il punto (b) è stato rifatto lato engine, e il risultato
> dice che **non era il dtype** — vedi L22 qui sotto. Il (a) resta il grosso, il (b) ha cambiato
> bersaglio (KV, non GEMV).

## L22 — Decode a N=16384 sul denso accoppiato: il degrado O(pos) NON è di dtype (2026-09-08, 23:15-23:20)

**Gamba engine fatta, gamba rapid NO e detta così.** DeepSeek-R1-32B-8bit, 3 rep alternate
4096↔16384, processo fresco a ogni run, `auto pair scales=F16 → mode=f16` **verificato a stderr a
ogni load** (non inferito). Load di finestra **8,2→11,6 in salita**, e alla fine della gamba è
schizzato a **18,6** con `test_ilrun` di un'altra AI al 103% di CPU + Rider + opencode: la ratio
vs rapid in questa finestra **non è misurabile** e la rapid leg non è stata lanciata (farebbe
una ratio fra due gambe prese a carico diverso — esattamente l'errore già commesso e ritrattato in
§20/L20). Il `vs rapid` a 16384 resta quindi aperto.

**Ma la curva interna regge senza rapid, ed è il numero che serviva** — il fattore di degrado
decode 4096→16384 è un rapporto **intra-motore: stesso processo, stessa finestra**, quindi la
contesa lo tocca molto meno di quanto tocchi le assolute:

| condizione | decode @4096 | decode @16384 | **fattore di degrado** |
|---|---|---|---|
| GMBasic **f16 accoppiato** (questa sessione, load 8-11) | 9,4 / 9,7 / 9,6 → **9,6** | 5,9 / 5,4 / 4,7 → **5,4** | **0,5625** |
| GMBasic **fp32, pre-pairing** (§19, load 3-6) | 9,6 | 5,5 | **0,573** |
| **rapidMLX** (§19, load 3-6) | 14,9 | 13,3 | **0,893** |

**Decomposizione del «2,4×» — è questa la scoperta, non il numero assoluto.** Il gap §19 era
1,56× a 4096 e 2,42× a 16384: il **gap aggiuntivo** del contesto lungo è 2,42/1,56 = **1,55×**, e
il rapporto dei due fattori di degrado è 0,893/0,5625 = **1,59×**. Sono la stessa cosa entro il
rumore: **tutto** il penalizzo extra a contesto lungo è il tasso di degrado, non un handicap
costante. Ne segue una previsione falsificabile e importante: siccome il dtype accoppiato è un
**moltiplicatore** che colpisce i due punti quasi egualmente (0,5625 vs 0,573: la promozione non
ha mosso il fattore), **nessuna leva di dtype può chiudere il buco a 16K**. Il bersaglio è il
percorso KV/attention (concat del cache, `qwen2` attention), e la riga §21 «DeepSeek @16384 →
mai rifatto dopo la coppia» va letta così: rifatto, e la coppia **non c'entra**.

**Assolute non confrontabili con §20, e va scritto**: 9,6 tok/s a 4096 contro i 14,60 di §20 è
**-34%**, ma §20 girava a load 4-6 e questa a 8-11 — è la sensibilità alla contesa già misurata
(motore in-process penalizzato più del server), non una regressione introdotta da `d8ca31990`.
Nessuna delle assolute di questa sessione va messa in una tabella di confronto.

*Strumenti: `/tmp/gmb-l22/gmb-deepseek.log` (6 run completi, dylib con auto-pair, verificato
presente col marcatore `auto pair scales=F16` — la copia in `test_mlx_bench/bin/Release/` era
**stale** e l'ho ricaricata prima di misurare: senza quel controllo questa sessione avrebbe
misurato il fp32 credendo di misurare il f16).*
