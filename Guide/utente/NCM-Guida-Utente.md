# NCM — Guida utente (generazione di testo "dal grossolano al fine")

> **Stato**: namespace `NCM`, 9 funzioni (`LOADMODEL`, `GENERATE`, `FREEMODEL`, `LASTERROR`, `LOADCONDITIONAL`, `GENERATEFROM`, `ROUGE1`, `ROUGE2`, `ROUGEL`), disponibili e identiche
> sulle **quattro gambe** (C99Native, IL, PCodeVM, VM/C). Implementazione: FFI su `libgmb_torch` (host libtorch C++).
> Mini test: `shared/minitests/ncm/` (`ncm_errors`, `ncm_generate`, `ncm_lifecycle`, `ncm_conditional`, `ncm_rouge`). Sample completo:
> `shared/samples/Ncm/NcmWorkbench.bas` (modello minuscolo, esegue tutto) e `shared/samples/AiLLM/NcmGenerate.bas`
> (modello reale LM1B, ~1 GB di pesi), `shared/samples/AiLLM/NcmSummarize.bas` (riassunto condizionato + ROUGE, modello minuscolo). Manuale: capitolo V.31d. Piano di sviluppo (condizionamento, addestramento): vault
> `Piani/(open) - Piano-NCM-Modelli-Guidabili-Condizionamento-e-Addestramento.md`.

`NCM` (Next-Concept Modeling, arXiv 2610.14162, Stanford / Together AI) genera testo con un'idea diversa dai modelli
che producono **un token alla volta**: il testo è descritto da **codici discreti su più livelli** (da pochi codici
molto grossolani a tanti codici di dettaglio) e **ogni livello viene deciso in un solo passo** del transformer.
Alla fine un decoder riporta i codici ai token.

Due regole valgono per tutta la guida:

> [!IMPORTANT] **`IMPORT NCM` è obbligatoria.** `NCM` non è un namespace implicito: senza l'IMPORT nessuna funzione
> qualificata è disponibile.

> [!IMPORTANT] **Sono FUNZIONI, con le parentesi.** `NCM.LOADMODEL(...)`, `NCM.GENERATE(...)`, `NCM.LASTERROR()`.
> `NCM.FREEMODEL` è una SUB: si chiama con le parentesi come le procedure del namespace (`NCM.FREEMODEL(h)`).

## 1. Cosa serve

| Elemento | Dettaglio |
|---|---|
| Checkpoint del **generatore** | `generator.pt` (formato `torch.save`, pesi in virgola mobile a 32 bit) |
| Checkpoint del **tokenizer** | `tokenizer.pt` (VQ-VAE multiscala: embedding, codebook, decoder) |
| **Vocabolario** del testo | `vocab.txt`, una voce per riga, l'id è il numero di riga (per i modelli HazyResearch: `bert-base-uncased`) |
| Libreria | `libgmb_torch` aggiornata con le funzioni `gmb_ml_ncm_*` e libtorch raggiungibile |

I checkpoint dei modelli reali sono pubblicati dagli autori (`hazyresearch/ncm-checkpoints`, licenza MIT): LM1B small pesa
390 MB (generatore) + 598 MB (tokenizer). Per provare senza scaricare nulla c'è il **modello minuscolo**
`shared/samples/Fixtures/ncm_tiny/` (pesi casuali, 4 livelli, vocabolario da 128 voci): il testo non ha senso ma tutto il
meccanismo è vero. Lo genera `GMBasic/native/gmb_torch/tools/ncm_make_tiny.cpp` (seme fisso, nessun Python).

I checkpoint reali vanno in `shared/samples/Fixtures/ncm/` (cartella ignorata da git).

## 2. Sintassi in una schermata

```basic
IMPORT NCM

DIM h AS LONG
h = NCM.LOADMODEL("../Fixtures/ncm_tiny/generator.pt", "../Fixtures/ncm_tiny/tokenizer.pt", "../Fixtures/ncm_tiny/vocab.txt", 0)
IF h = 0 THEN
    PRINT "errore: "; NCM.LASTERROR()
ELSE
    PRINT NCM.GENERATE(h, 4, 42)      ' 4 testi, uno per riga, seed 42 (riproducibile)
    NCM.FREEMODEL(h)
END IF
```

## 3. Riferimento delle funzioni

### 3.1 `NCM.LOADMODEL(generatorPt, tokenizerPt, vocabTxt, device)` → LONG

Carica un modello NCM completo (generatore + tokenizer + vocabolario) e ne ritorna l'**handle**.

| Parametro | Tipo | Significato |
|---|---|---|
| `generatorPt` | STRING | Percorso del checkpoint del generatore. Un percorso che inizia con `~/` è espanso nella cartella dell'utente. I percorsi relativi sono relativi alla cartella corrente del programma. |
| `tokenizerPt` | STRING | Percorso del checkpoint del tokenizer VQ-VAE (stesse regole). |
| `vocabTxt` | STRING | Percorso del vocabolario (una voce per riga). Serve a trasformare gli id dei token in testo. |
| `device` | LONG | Dove si calcola: `0` = CPU, `1` = CUDA, `3` = METAL (GPU Apple, via MPS). Se il dispositivo non è disponibile la chiamata fallisce (ritorna 0). |

**Ritorno**: un intero **positivo** (l'handle) se il caricamento riesce, **0** se fallisce. Su fallimento `NCM.LASTERROR()`
riporta il motivo. Non esistono handle negativi.

Il caricamento legge i pesi in memoria (CPU o GPU) e ricava dalla forma dei tensori le dimensioni del modello
(livelli, teste di attenzione, codebook). Si possono tenere **più modelli vivi insieme**: ogni chiamata dà un handle diverso.

**Errori tipici** (tutti → handle 0): file inesistente, file che non è un `torch.save` (ad esempio il vocabolario passato
come generatore), checkpoint con un numero di livelli che il programma non riconosce, dispositivo non disponibile.

### 3.2 `NCM.GENERATE(handle, n, seed)` → STRING

Genera `n` testi indipendenti con il modello `handle`. Il ritorno è **una stringa** con i testi **separati da newline**
(`CHR$(10)`), uno per riga, **con un newline anche dopo l'ultimo**: per contare i testi si usa `SPLIT(t, CHR$(10))` e si
ignora l'ultimo elemento vuoto.

| Parametro | Tipo | Significato |
|---|---|---|
| `handle` | LONG | Handle ottenuto da `NCM.LOADMODEL`. Un handle 0, sconosciuto o già liberato è un errore. |
| `n` | LONG | Numero di testi da generare, **positivo**. Il lotto viene generato insieme (più è grande, meno costa a campione). `n <= 0` è un errore. |
| `seed` | LONG | `>= 0`: fissa il generatore casuale, la chiamata è **riproducibile** (stesso modello, stesso `n`, stesso seed → stesso testo, sulla stessa macchina). `< 0`: ogni chiamata è diversa. |

**Ritorno**: la stringa con i testi; **stringa vuota** su errore (con il motivo in `NCM.LASTERROR()`).
La lunghezza non è limitata. Gli eventuali newline interni a un testo sono sostituiti da spazi, così una riga è sempre un testo.

**Cosa produce**: il generatore sceglie, livello per livello, i codici discreti del testo (un solo passo del transformer per
livello), il decoder li trasforma nei token di una finestra a lunghezza fissa (per LM1B: 128 token) e il vocabolario li
trasforma in testo. Il testo è in minuscolo se il vocabolario lo è; i pezzi di parola `##x` vengono riattaccati alla parola
precedente e la punteggiatura è ripulita come fa il tokenizer BERT. Il modello LM1B genera frasi di tipo notizia
(corpus LM1B) **senza condizionamento**: non si può guidare con un testo di partenza.

### 3.3 `NCM.FREEMODEL(handle)`

Libera il modello (memoria CPU/GPU) e rende **non valido** l'handle. Nessun valore di ritorno.

| Parametro | Tipo | Significato |
|---|---|---|
| `handle` | LONG | L'handle da liberare. Liberare due volte lo stesso handle, o l'handle `0`, **non ha effetto** e non dà errore. |

Dopo `FREEMODEL`, `NCM.GENERATE(handle, ...)` ritorna `""` e `NCM.LASTERROR()` lo dichiara. Gli altri modelli restano
intatti.

### 3.4 `NCM.LASTERROR()` → STRING

Ritorna il messaggio dell'**ultimo errore** del motore torch (`""` se non ce ne sono stati). Va letto subito dopo una
chiamata che ha fallito (`LOADMODEL` che ritorna 0, `GENERATE` che ritorna `""`): il messaggio non viene cancellato da
una chiamata riuscita successiva, quindi non è un indicatore di successo — il successo si vede dal valore di ritorno.

### 3.5 `NCM.LOADCONDITIONAL(generatorPt, tokenizerPt, configYaml, vocabJson, mergesTxt, device)` → LONG

Carica un modello **condizionato dal testo** (checkpoint XSum del paper: riassunto di un documento). Il documento è il
**prefisso** che il generatore vede; i parametri che non stanno nei pesi (RoPE, lunghezza del prefisso, cascata del
tokenizer, penalità di generazione) si leggono dal `config.yaml` del checkpoint.

| Parametro | Tipo | Significato |
|---|---|---|
| `generatorPt`, `tokenizerPt` | STRING | `generator.pt` e `tokenizer.pt` del checkpoint (cartella `xsum` di `hazyresearch/ncm-checkpoints`). |
| `configYaml` | STRING | `config.yaml` dello stesso checkpoint. |
| `vocabJson`, `mergesTxt` | STRING | Il BPE di GPT-2 (`vocab.json`, `merges.txt` di `openai-community/gpt2`). |
| `device` | LONG | 0 = CPU, 1 = CUDA, 3 = METAL (GPU Apple); se il dispositivo non c'è ritorna 0. |

Ritorna un handle (> 0) oppure `0` (motivo in `NCM.LASTERROR()`): per esempio un percorso inesistente. L'handle si libera
con `NCM.FREEMODEL`. Un modello condizionato **non** serve a `GENERATE` (che non ha prompt): darebbe `""` con l'errore
"prefisso di 1 token, il modello ne richiede N".

### 3.6 `NCM.GENERATEFROM(handle, prompt, seed, cfgScale)` → STRING

Genera **un** testo condizionato dal `prompt`, tagliato al primo fine-testo.

| Parametro | Tipo | Significato |
|---|---|---|
| `handle` | LONG | Modello caricato con `LOADCONDITIONAL`. |
| `prompt` | STRING | Il documento (UTF-8). Contano solo i primi P token BPE (512 nel checkpoint XSum); il resto è ignorato, e un documento più corto è completato con padding mascherato. |
| `seed` | LONG | Seme del campionamento, `>= 0` per riprodurlo. Il config XSum è greedy (`top_k = 1`): il seed non cambia il risultato. |
| `cfgScale` | DOUBLE | Guidance senza classificatore: `0` = spenta, `> 0` = guidata (costo doppio), valore **negativo** = quello del config (0 in XSum). |

Ritorna `""` su errore (handle non valido, modello non condizionato): il motivo è in `NCM.LASTERROR()`. L'uscita è sempre
UTF-8 valida (i byte BPE incompleti diventano U+FFFD).

**Quanto è buono?** Sui primi 1000 documenti del test set XSum il checkpoint pubblicato dà ROUGE-1/2/L = **34,60 / 12,82 / 27,77**
(paper: 35,0 / 12,8 / 28,0), circa 0,12 s a riassunto su Apple M5 Max. La guidance (`cfgScale` 1) non migliora in modo
misurabile e costa 2,4 volte; a 3 peggiora (R-1 31,8): il valore di default del paper, 0, resta il migliore.

```basic
h = NCM.LOADCONDITIONAL("xsum/generator.pt", "xsum/tokenizer.pt", "xsum/config.yaml", "gpt2/vocab.json", "gpt2/merges.txt", 3)
summary = NCM.GENERATEFROM(h, document, 0, -1)
PRINT summary; "  ROUGE-L="; FORMAT(NCM.ROUGEL(reference, summary), "0.000")
```

### 3.7 `NCM.ROUGE1(reference, hypothesis)`, `NCM.ROUGE2(...)`, `NCM.ROUGEL(...)` → DOUBLE

F-measure ROUGE (0..1) fra un testo di riferimento e uno generato; **non richiedono nessun modello**. Stessa
tokenizzazione di `rouge_score` con lo stemmer: minuscolo, solo `[a-z0-9]`, stemming di Porter per i token oltre 3 lettere;
ROUGE-1/2 = sovrapposizione di unigrammi/bigrammi con conteggi, ROUGE-L = sottosequenza comune più lunga. Il testo vuoto vale 0. Lo
stemmer è il Porter classico: su poche parole irregolari si discosta da quello di NLTK (scarti di pochi centesimi di punto sul
punteggio medio). Esempio: `NCM.ROUGE1("the cat sat", "the cat sat on the mat")` = 0,667.

## 4. Riproducibilità e parità

- **Stesso seed, stesso testo** sulla stessa macchina e con lo stesso dispositivo. Fra macchine o fra CPU e GPU i testi
  possono differire (aritmetica in virgola mobile diversa).
- Le **quattro gambe danno lo stesso testo** perché passano tutte dalla stessa libreria nativa: i test di parità
  (`tools/sh/parity4`) lo verificano su `ncm_generate`, `ncm_lifecycle`, `ncm_errors`, `ncm_conditional`, `ncm_rouge` e sui sample `NcmWorkbench` e `NcmSummarize`.
- **Fedeltà al riferimento** (modello LM1B small): con temperatura 0 i codici generati coincidono con quelli del
  codice PyTorch degli autori (0 differenze su 2564); la perplexity generativa misurata con GPT-2 large (protocollo
  `genppl_flm` del paper) è **93,26** contro **93,05** dichiarata.
- **Velocità** (Apple M5 Max, MPS, LM1B small): circa 28 ms a campione da 128 token con lotti di 64 o più; su CPU circa
  0,24 s a campione. Non c'è un confronto con un modello autoregressivo equivalente: non si può dire che NCM sia più
  veloce di un LLM tradizionale.

## 5. Limiti attuali

- **Condizionamento solo in inferenza**, con i checkpoint pubblicati (XSum). Un prompt = un testo per chiamata. Mancano
  MBR, WMT14 (BLEU) e Sudoku. L'addestramento e il fine-tuning del condizionamento sono nel piano (vault, Piani).
- Un solo tipo di modello: generatore + tokenizer VQ-VAE multiscala nel formato HazyResearch, vocabolario BERT.
- Finestra fissa (128 token nel modello reale): niente testi più lunghi in una chiamata.
- Serve la libreria `libgmb_torch` con libtorch: se manca, `LOADMODEL` ritorna 0 con l'errore della libreria.
- Nessun addestramento in BASIC.
- ROUGE: lo stemmer è il Porter classico (non quello di NLTK), vedi 3.7.

## 6. Diagnostica

| Sintomo | Causa probabile |
|---|---|
| `[FFI] simbolo 'gmb_ml_ncm_load' non trovato in 'gmb_torch'` | `libgmb_torch.dylib` vecchia, senza le funzioni NCM: va sostituita con la build aggiornata (in tutte le cartelle di ricerca delle librerie: `~/.gmbasic/libs/osx-arm64`, `.../ml/osx-arm64`). |
| `LOADMODEL` ritorna 0, `LASTERROR` parla di `open file failed` | percorso sbagliato (relativo alla cartella corrente del programma). |
| `LASTERROR`: `numero di livelli del checkpoint ... diverso dai livelli noti` | checkpoint con una struttura non LM1B e senza `inference.levels`. |
| `GENERATE` ritorna `""` | handle non valido o `n <= 0`: guarda `NCM.LASTERROR()`. |
| `GENERATEFROM` ritorna `""` | handle non valido, oppure modello non condizionato (caricato con `LOADMODEL`): guarda `NCM.LASTERROR()`. |
| `LOADCONDITIONAL` ritorna 0 con `config non leggibile` | manca `config.yaml` (o il percorso è sbagliato): senza di lui i parametri di RoPE, prefisso e cascata sono quelli di LM1B. |

## 7. Riferimenti

- Mini test: `shared/minitests/ncm/ncm_errors.bas`, `ncm_generate.bas`, `ncm_lifecycle.bas`, `ncm_conditional.bas`, `ncm_rouge.bas`.
- Sample: `shared/samples/Ncm/NcmWorkbench.bas` (completo, usa le 4 funzioni), `shared/samples/AiLLM/NcmGenerate.bas`, `shared/samples/AiLLM/NcmSummarize.bas` (condizionato + ROUGE).
- Include: `shared/include/ncm.inc`. Codice: `GMBasic/native/gmb_torch/src/gmb_ncm.hpp`, `gmb_pt_load.hpp`, `gmb_gpt2.hpp` (BPE), `gmb_rouge.hpp`, `gmb_host_ml.cpp`. Fixture condizionata: `shared/samples/Fixtures/ncm_tiny_cond/` (`ncm_make_tiny <dir> cond`); valutazione XSum: `GMBasic/native/gmb_torch/tests/ncm_xsum_eval.cpp`.
