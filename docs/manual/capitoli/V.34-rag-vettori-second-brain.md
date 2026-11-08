# V.34 — RAG, vettori e Second Brain

## Cosa ottieni da questo capitolo

Dare a un modello i **tuoi** documenti: spezzarli, calcolarne gli embedding, cercare i pezzi
rilevanti e metterli nel prompt. Il capitolo spiega la catena e, soprattutto, come si misura se
funziona.

## 1. Che cos'è un embedding

Un embedding è un vettore di numeri che rappresenta il **significato** di un testo. Due frasi con
lo stesso senso hanno vettori vicini anche senza parole in comune. "Vicino" si misura con una
distanza (coseno o euclidea).

Senza un provider di embedding non c'è RAG. Eseguito
(`shared/samples/VectorDB/VectordbSq8.bas`, backend C99Native):

```
VectordbSq8: provider embedding non attivo, demo non eseguibile qui.
```

Il sample lo dice invece di restituire risultati finti — ed è il comportamento corretto: una demo
che "funziona" senza il modello produce numeri arbitrari.

## 2. La catena

```
documenti → chunk → embedding → indice vettoriale → query → top-k → prompt → risposta
```

Ogni passo ha un modo di rompersi:

| Passo | Cosa va storto | Sintomo |
|---|---|---|
| chunk | pezzi troppo grandi o troppo piccoli | il modello non trova il dettaglio, o perde il contesto |
| embedding | provider sbagliato o assente | risultati casuali |
| indice | metriche diverse per indicizzare e cercare | nessun risultato sensato |
| top-k | `k` troppo piccolo o troppo grande | risposta incompleta o rumore |
| prompt | i pezzi non hanno provenienza | il modello inventa |

## 3. Indicizzare

```basic
AI VECTOR INDEX TEXT "db", "docs", "id", "testo"
```

Il comando prende l'indice (`db`), la cartella o la sorgente (`docs`), il campo identificatore
(`id`) e il campo di testo (`testo`). Il dettaglio è in `VECTORDB-Guide`.

Il chunking è la decisione che pesa più di tutte: un documento spezzato a metà frase perde il
soggetto; un chunk da diecimila parole non entra nel prompt. La pratica che funziona è spezzare
per struttura (titoli, paragrafi) e non per numero fisso di caratteri.

## 4. Misurare: il RAG si valuta, non si spera

`RagEvalMetrics.bas`, `RagEvalAdvanced.bas` e `Rag-Eval-Guida-Utente` coprono la valutazione:
recall dei documenti recuperati, precisione, correttezza della risposta rispetto a una verità di
riferimento.

Senza una misura, "il RAG funziona" è un'impressione. Le metriche che servono:

- **recall@k** — il documento giusto è fra i `k` recuperati?
- **precisione** — quanti dei recuperati erano utili?
- **fedeltà** — la risposta cita solo ciò che è nei documenti?

## 5. Second Brain

Un "secondo cervello" è un archivio personale interrogabile: note, documenti, conversazioni. Il
pattern (`Second-Brain-Guide`) è RAG più una struttura di collegamenti (`AI-SemanticGraph-Guida-Utente`)
che permette di navigare le relazioni fra concetti, non solo la somiglianza.

La differenza pratica: la ricerca vettoriale trova "pezzi simili"; il grafo semantico trova
"concetti collegati" — e per una domanda che richiede di mettere insieme due documenti diversi è
il secondo che serve.

## 6. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| risposte inventate | il modello non ha i documenti nel prompt | verifica il recupero prima del modello |
| recupero sempre gli stessi pezzi | indice con duplicati | deduplica prima di indicizzare |
| il documento giusto non esce mai | chunk troppo grandi | spezza per struttura |
| risultati cambiati senza toccare nulla | embedding aggiornati | fissa la versione del modello |
| lentezza | indice non in memoria o `k` enorme | riduci `k`, usa un indice approssimato |

## 7. Dove andare adesso

- **V.32** — il provider di embedding e il modello che risponde.
- **V.33** — l'agente che usa il RAG come strumento.
- **V.35** — un embedding calcolato in locale, senza dati che escono.

## 8. Nota di onestà

I sample di questo capitolo non producono output in questo ambiente: serve un provider di
embedding. La citazione sopra è l'**unico** output reale disponibile, e dice esattamente questo.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### AI-RAG-Guide

*Fonte: `AI-RAG-Guide`*


## 1. Concetti
| Termine | Significato |
|---|---|
| `db` | nome del database SQLite (`mydb`) o **percorso** esplicito; i db generati da `RAG BUILD` finiscono in `<root>/rag/` (configurabile, vedi [[Guide/utente/Paths-Config-User-Guide]]) |
| `coll` | collezione (tabella) dentro il db (es. `docs`) |
| `id` | chiave del documento (stringa) |
| `embedding` | vettore `float[dim]` del testo (es. 768) |

## 2. Comandi — `AI RAG`

### `AI RAG ENSURE COLLECTION db, coll [, dim]`
| parametro | obbligatorio | significato |
|---|---|---|
| `db` | sì | alias DB / path (`":memory:"` = DB in-memory del run) |
| `coll` | sì | nome della collezione |
| `dim` | no | dimensione embedding (es. `768`); omesso = `ActiveDimensions` del provider |

AI RAG ENSURE COLLECTION "mydb", "docs", 768
AI RAG ENSURE COLLECTION "mydb", "docs2"      ' dim dal provider attivo


### `VECTORDB.ENSURECOLLECTION(db, coll[, dims[, format]])` — con storage SQ8
| parametro | obbligatorio | significato |
|---|---|---|
| `db` | sì | alias DB / path (`":memory:"` = DB in-memory del run) |
| `coll` | sì | nome della collection |
| `dims` | no | dimensione embedding (0 = `ActiveDimensions` del provider) |
| `format` | no | `'fp32'` (default) o `'sq8'` — solo minuscolo esatto; altri valori = fp32 |

IMPORT VECTORDB
DIM idf = VECTORDB.ENSURECOLLECTION("mydb", "docs", 768)
DIM idq = VECTORDB.ENSURECOLLECTION("mydb", "docsq", 768, "sq8")


### Storage quantizzato SQ8 — quando usarlo

### `AI RAG INDEX TEXT db, coll, id, testo`

AI RAG INDEX TEXT "mydb", "docs", "doc1", "Il manuale spiega il namespace AI."
AI RAG INDEX TEXT "mydb", "docs", "doc2", "La GPM è l'unico esecutore del codice BASIC."

| parametro | obbligatorio | significato |
|---|---|---|
| `db` | sì | alias DB / path (`":memory:"` = DB in-memory del run) |
| `coll` | sì | nome della collection |
| `id` | sì | id sorgente (chiave di dedup: re-index dello **stesso testo** = `Skipped`) |
| `testo` | sì | contenuto da indicizzare |

### `AI RAG INDEX META db, coll, id, testo, metaJson`

AI RAG INDEX META "mydb", "docs", "doc2", "testo con autore", "{""autore"":""Gaetano""}"


### `AI RAG LOAD DOCUMENTS db, coll, path`

AI RAG LOAD DOCUMENTS "mydb", "docs", "/Users/gaetano/documents"


### `AI RAG REMOVE SOURCE db, coll, id`

AI RAG REMOVE SOURCE "mydb", "docs", "doc1"


### `AI RAG PURGE DEPRECATED db, coll`

AI RAG PURGE DEPRECATED "mydb", "docs"


## 3. Funzioni — `AI.*`
| Funzione | Ritorna | Esempio |
|---|---|---|
| `AI.EMBED(text, dim)` | vettore `float[dim]` (lista) | `v = AI.EMBED("ciao", 768)` |
| `AI.INDEXTEXT(db, coll, id, testo)` | `0`/`1`/`2`/`-1` (Indexed/Updated/Skipped/errore) | `n = AI.INDEXTEXT("mydb","docs","d1","testo")` |
| `AI.INDEXMETA(db, coll, id, testo, meta)` | `0`/`1`/`2`/`-1` (come sopra) | `n = AI.INDEXMETA("mydb","docs","d1","t","{}")` |
| `AI.REMOVESOURCE(db, coll, id)` | `1` rimosso / `0` assente | `r = AI.REMOVESOURCE("mydb","docs","d1")` |
| `AI.PURGEDEPRECATED(db, coll)` | numero eliminati (`-1` errore) | `p = AI.PURGEDEPRECATED("mydb","docs")` |
| `AI.COUNT(db, coll)` | numero | `n = AI.COUNT("mydb", "docs")` |
| `AI.SEARCH(db, coll, query, topK, hybrid, filter [, reranker [, minEvidence [, explain]]])` | stringa JSON | `r$ = AI.SEARCH("mydb", "docs", "namespace AI", 5, 1, "{""tenant"":""acme""}")` |
| `AI.RAGCONTEXT(db, coll, query, topK)` | stringa | `c$ = AI.RAGCONTEXT("mydb", "docs", "cos'è la GPM?", 5)` |
| `AI.ASK(db, coll, domanda, topK)` | stringa | `a$ = AI.ASK("mydb", "docs", "Spiega la GPM.", 5)` |
| `AI.RERANK(query, json, topK)` | stringa JSON | `r$ = AI.RERANK("query", "[{...}]", 5)` |
| `AI.LISTCOLLECTIONS(db)` | stringa JSON | `c$ = AI.LISTCOLLECTIONS("mydb")` |
| `VECTORDB.ENSURECOLLECTION(db, coll[, dims[, format]])` | id / `-1` errore | `id = VECTORDB.ENSURECOLLECTION("mydb","docs",768,"sq8")` |

IMPORT AI

SUB Main()
  AI RAG ENSURE COLLECTION "mydb", "docs", 768
  AI RAG INDEX TEXT "mydb", "docs", "gpm", "La GPM è l'unico esecutore del codice BASIC. Ogni handler è un task GPM."
  AI RAG INDEX TEXT "mydb", "docs", "vector", "AI RAG indicizza testi e cerca per similarità."

  PRINT "COUNT="; AI.COUNT("mydb", "docs")
  PRINT "SEARCH="; AI.SEARCH("mydb", "docs", "esecutore codice", 2, 1, "")
  PRINT "CONTEXT="; AI.RAGCONTEXT("mydb", "docs", "chi esegue il codice?", 2)
  PRINT "ASK="; AI.ASK("mydb", "docs", "Chi esegue il codice BASIC?", 2)
END SUB
Main()


### ON INSERT EXECUTE AGENT (automatico, opzionale — C2, 2026-09-07)

// host C# — richiede che l'agente esista già (AgentServer.Instance.List())
Basic.Agents.AgentInsertTrigger.Register(db, coll, agentName, "Elabora il documento inserito: {sourceId}");


## 4. Errori e limiti
| Situazione | Cosa succede |
|---|---|
| Nessun provider embedding attivo | `?No embedding provider configured. Use PROVIDER.REGISTER` |
| `db`/`coll` inesistenti su `SEARCH` | JSON vuoto `[]` |
| `hybrid=1` senza FTS5 | fallback a solo vettoriale |

### Tokenizzazione dei modelli ONNX (BERT WordPiece)
| Limite | Valore | Effetto |
|---|---|---|
| Finestra embedding | **128 token** | il testo oltre la finestra viene troncato (i marcatori `[CLS]`/`[SEP]` restano; per l'indicizzazione il `TextChunker` riduce il problema a monte) |
| Finestra reranker | **256 token** (coppia query+documento) | su overflow viene tagliato il CONTENUTO del documento (poi della query), mai i marcatori |
| `vocab.txt` | obbligatorio accanto al modello `.onnx` | senza: errore esplicito `BERT vocab.txt not found` |
| Parole fuori vocabolario / >100 caratteri | `[UNK]` | contribuiscono poco alla semantica: nei corpus con molti termini rari preferire un modello con vocabolario adeguato |

## 5. File del progetto
| File | Ruolo |
|---|---|
| `GMBasicSharp/common/AI/Rag/AiVectorRuntime.cs` | `Embed`, `EnsureCollection`, `SearchJson`, `RagContext`, `Ask` |
| `GMBasicSharp/common/Databases/VectorDB/*` | `SqlVectorIndexer`, `SqlVectorSearch` (FTS5+RRF) |
| `GMBasicSharp/common/Parser/Parser.Stmt.Ai.cs` | `AI RAG ENSURE/INDEX/REMOVE/PURGE/LOAD` |
| `GMBasicSharp/common/StdLib/StdLib.AI.cs` | `AI.EMBED/SEARCH/RAGCONTEXT/ASK/COUNT` |

## 6. Riferimenti

### AI-SemanticGraph-Guida-Utente

*Fonte: `AI-SemanticGraph-Guida-Utente`*


## 1. Cosa sono
| Funzione | Domanda a cui risponde |
|---|---|
| `LLM.SYMBOL(nome$)` | *Cos'è* `nome$`? (kind + nome della definizione) |
| `LLM.CALLMAP(nome$)` | *Chi la chiama e chi chiama lei*? (callers / callees, 1 livello) |
| `LLM.IMPACT(nome$[, profondità])` | *Chi è impattato* (transitivo all'indietro) da un cambiamento di `nome$`? |

## 2. Sintassi e parametri

ris$ = LLM.SYMBOL(nome$)
ris$ = LLM.CALLMAP(nome$)
ris$ = LLM.IMPACT(nome$ [, profondità])
ris$ = LLM.INSPECT(nome$)          ' hover: kind+label + callmap immediato


### Valori di ritorno (stringa, canonici)
| Chiamata | Ritorno (esempio) |
|---|---|
| `LLM.SYMBOL("Foo")` | `procedure Foo` (kind in minuscolo + nome); `""` se il nome non esiste |
| `LLM.CALLMAP("Foo")` | `callers=[Main] callees=[Bar]` (liste di **nomi**, separate da `,`) |
| `LLM.IMPACT("Foo")` | `Main@1` / `UnitScale@1,PointScore@2,…` — `<nome>@<livello>`, **senza** il simbolo seme |
| `LLM.INSPECT("Foo")` | `<kind> <label> callers=[…] callees=[…]` (hover: definizione + callmap immediato); `""` se non esiste |

## 3. Semantica

## 4. Esempi

### 4.1 Definizione

IMPORT LLM
SUB Foo()
END SUB
SUB Main()
  PRINT LLM.SYMBOL("Foo")      ' -> procedure Foo
  PRINT LLM.SYMBOL("NonEsiste")' -> (stringa vuota)
END SUB


### 4.2 Mappa delle chiamate

IMPORT LLM
SUB Bar()
END SUB
SUB Foo()
  Bar()
END SUB
SUB Main()
  Foo()
  PRINT LLM.CALLMAP("Foo")     ' -> callers=[Main] callees=[Bar]
END SUB


### 4.3 Impatto (transitivo) e con limite di profondità

IMPORT LLM
SUB Top()
END SUB
SUB Mid()
  Top()
END SUB
SUB L0()
  Mid()
END SUB
SUB Main()
  L0()
  PRINT LLM.IMPACT("Top")      ' -> Mid@1,L0@2,Main@3
  PRINT LLM.IMPACT("Top", 1)   ' -> Mid@1   (solo 1 livello)
END SUB


### 4.4 Programma completo

## 5. Disponibilità e verifica sui backend
| Backend | Comando | Esito |
|---|---|---|
| IL | tutta la superficie `LLM.*` | ✅ |
| PCodeVM | idem | ✅ |
| VM/C | idem | ✅ |
| C99Native | idem | ✅ |

## 6. Polimorfismo

## 7. Note implementative (per chi mantiene)

## 8. Riferimenti

## 9. Estensione SEMAPRAX — `LLM.REVISION()` e `LLM.CONTEXT(name$[,depth])`
| Funzione | Ritorno |
|---|---|
| `LLM.REVISION()` | `rev=<hash>` — hash **FNV-1a** deterministico sul call-graph (coppie `caller->callee`, solo simboli utente). **Identico** tra i due compilatori (C# e C). Serve per i **patch revision-bound** (rifiutare un patch se la revisione è cambiata). |
| `LLM.CONTEXT(name$ [, depth])` | `context <name>: [<vicini>]` — il simbolo + i vicini (callers/callees) fino a `depth` livelli (default 1), nomi ordinati. |

PRINT LLM.REVISION()          ' -> rev=dfb23ef0339682c0
PRINT LLM.CONTEXT("B", 2)     ' -> context B: [A,B,C,Main]


### `LLM.INSPECT(name$)` — hover del simbolo

### `LLM.PATCH(target$)` — receipt del semantic patch

### `LLM.CHECKREV(rev$)` — gate revision-bound

### `LLM.FINDSYMBOL(prefix$)` e `LLM.REPOMAP()` — PARSEC (repo/symbol/call map)

### `LLM PATCH "target$", "rev$" … END PATCH` — write-side revision-bound

IMPORT LLM
SUB Foo()
  PRINT "ORIGINALE"
END SUB
SUB Main()
  LLM PATCH "Foo", "rev=92ef79932addfddb"   ' <-- rev reale di questo programma
    PRINT "PATCHATO"
  END PATCH
  Foo()
END SUB


### `LLM.VERIFY()` e `LLM.COMMIT()` — receipt del lifecycle (read-only)

IMPORT LLM
SUB Greet()
  PRINT "orig"
END SUB
SUB Main()
  LLM PATCH "Greet", "rev=baa4ff0afd9323a6"
    PRINT "patched"
  END PATCH
  PRINT LLM.VERIFY()     ' -> verify: ok
  PRINT LLM.COMMIT()     ' -> commit: applied
  Greet()                ' -> patched
END SUB


## 7. Esposizione agli agenti — `AI.AGENTRUNCODE` (§4.10)
| Parametro | Obbligatorio | Descrizione |
|---|---|---|
| `<name>` | sì | Nome dell'agente (deve esistere: `AI AGENT CREATE`). |
| `<prompt>` | sì | La domanda; il modello deciderà quali tool `code.*` invocare. |
| `<rounds>` | no | Numero massimo di round del tool-loop (default 5). |
| Tool | Ritorno |
|---|---|
| `code.symbol` | `"<kind> <label>"` (es. `procedure Foo`) |
| `code.callmap` | `callers=[…] callees=[…]` |
| `code.impact` | `<nome>@<depth>,…` (transitivo all'indietro) |
| `code.context` | `context <nome>: [vicini…]` |
| `code.findsymbol` | `find <q>: [nomi…]` |
| `code.fileoutline` | `<kind> <nome>;…` (simboli del file) |
| `code.repomap` | `repomap: [caller->callee;…]` |

### Rag-Eval-Guida-Utente

*Fonte: `Rag-Eval-Guida-Utente`*


## 1. A cosa servono

## 2. Sintassi canonica (funzioni qualificate, §1-§2 AGENTS)

' R1 — retrieval: retrieved$ e ideal$ = CSV di id (trim, vuoti scartati)
p@k = VECTORDB.PRECISIONAT(retrieved$, ideal$, k)
r@k = VECTORDB.RECALLAT(retrieved$, ideal$, k)
mrr = VECTORDB.MRR(retrieved$, ideal$)
ndcg@k = VECTORDB.NDCGAT(retrieved$, ideal$, k)

' R2 — generazione: score 0..1
fai = VECTORDB.FAITHFULNESS(contesto$, risposta$)
hal = VECTORDB.HALLUCINATION(contesto$, risposta$)
rel = VECTORDB.RELEVANCE(domanda$, risposta$)
com = VECTORDB.COMPLETENESS(domanda$, risposta$)


## 3. Semantica precisa (v1 deterministic)
| Funzione | Formula | Note |
|---|---|---|
| FAITHFULNESS | token(risposta) ∩ token(contesto) / \|token(risposta)\| | risposta vuota → 0 |
| HALLUCINATION | 1 − FAITHFULNESS | quota token risposta assenti dal contesto |
| RELEVANCE | token(domanda) ∩ token(risposta) / \|token(domanda)\| | domanda vuota → 0 |
| COMPLETENESS | come RELEVANCE ma solo token di contenuto (≥3 char, non stopwords) | ricade su RELEVANCE se non ci sono token di contenuto |

VECTORDB.PRECISIONAT("a,b,c,d", "b,d", 2)  ' → 0.5
VECTORDB.RECALLAT("a,b,c,d", "b,d", 2)     ' → 0.5
VECTORDB.MRR("a,b,c,d", "b,d")             ' → 0.5
VECTORDB.NDCGAT("a,b,c,d", "b,d", 4)       ' → 0.6509
VECTORDB.FAITHFULNESS("il motore mlx gestisce la memoria", "motore mlx memoria rapido")  ' → 0.75
VECTORDB.HALLUCINATION(<stessi arg>)       ' → 0.25
VECTORDB.RELEVANCE("che cosa gestisce il motore", "il motore gestisce la memoria")       ' → 0.6
VECTORDB.COMPLETENESS(<stessi arg>)        ' → 0.5


## 4. Esempio completo

## 5. Limiti onesti (v1)

## 6. Verifica parità (R1-R4 del blocco)
| Gamba | Verifica | Esito |
|---|---|---|
| R1 MiniTest | 8 per-funzione in `shared/minitests/vectordb/` (precisionat, recallat, mrr, ndcgat, faithfulness, hallucination, relevance, completeness) + `rageval_metrics.bas` aggregato, tutti con EXPECT | census `--ns VECTORDB` → 0 mancanti |
| R2 Sample | `shared/samples/VectorDB/RagEvalMetrics.bas` (usa tutti gli 8 simboli, deterministico) | `tools/sh/parity4` PASS 4/4 |
| R3 Guida | questa guida (tutti gli 8 simboli documentati con valori handbook) | in `Guide/_Index.md` |
| R4 Parità | parity IL/PCode/VMC PASS · C99Native 9/10 file PASS (10 con purge) · gate C99Native PASS 8/8 file + aggregato · unit `EvalHarnessTests.RagEval_Metrics_Handbook` 3/3 · `gmb_rageval_selfcheck` 12/12 | PASS |

## 7. R3 — GenTest: generazione testset sintetici

### Sintassi

result$ = VECTORDB.GENTEST("mycoll", 10, "simple,multi-hop,adversarial")


### Parametri
| Parametro | Tipo | Descrizione |
|---|---|---|
| `collection` | string | Nome della collection vettoriale |
| `n` | numeric | Numero di casi da generare (default 5) |
| `types` | string | Tipi di domanda, separati da virgola: `simple` (1 chunk), `multi-hop` (fatti correlati), `adversarial` (distrattore lessicale) |

### Comportamento

### Esempio

IMPORT VECTORDB
VECTORDB.ENSURECOLLECTION(":memory:", "docs", 8)
VECTORDB.INDEXTEXT(":memory:", "docs", "d1", "Sample text to generate questions from.")
DIM result = VECTORDB.GENTEST("docs", 5, "simple,multi-hop")
PRINT result    ' "Golden set saved: shared/samples/Fixtures/rag-eval/gentest_*.json (5 cases, sha256=...)"


### Backend
| Backend | Supporto |
|---|---|
| IL (C#) | ✅ |
| PCodeVM (C#) | ✅ |
| VM/C | ✅ (tramite builtin dispatch 3541) |
| C99Native (C) | ✅ (FD_TBL → `gmb_vectordb_gentest()`) |

### Riferimenti

## 8. R4-R8 — Diagnostica, comparazione e monitoraggio

### 8.1 VECTORDB.DIAGNOSE(eval$) — diagnosi root-cause

DIM cause$ = VECTORDB.DIAGNOSE('{"retrieved":["d2","d3"],"ideal":["d1"],"faithfulness":0.9,"hallucination":0.1,"completeness":0.7,"context_len":400}')
' → {"cause":"retrieval_miss","evidence":"...","recommendation":"..."}


### 8.2 VECTORDB.DIAGNOSEREPORT() — report aggregato

DIM report$ = VECTORDB.DIAGNOSEREPORT()
' → {"total":2,"causes":{"retrieval_miss":1,"generation_incomplete":1}}


### 8.3 VECTORDB.CALIBRATE(cases$) — calibrazione giudice

DIM cases$ = '[{"umano":1,"giudice":1},{"umano":0,"giudice":0},...]'
DIM cal$ = VECTORDB.CALIBRATE(cases$)
' → {"kappa":1.0,"wilson_ci_low":0.8,"wilson_ci_high":1.0,"threshold":0.5,...}


### 8.4 VECTORDB.EVALGATE(baseline$, current$) — gate CI

DIM base$ = '{"p_at":0.9,"recall":0.9,"faithfulness":0.9}'
DIM curr$ = '{"p_at":0.3,"recall":0.2,"faithfulness":0.8}'
DIM gate$ = VECTORDB.EVALGATE(base$, curr$)
' → {"verdict":"fail","details":{...}}


### 8.5 VECTORDB.EVALDIFF(baseline$, current$) — differenze per metrica

DIM diff$ = VECTORDB.EVALDIFF(base$, curr$)
' → {"p_at":{"before":0.9,"after":0.3,"delta":-0.6},...}


### 8.6 VECTORDB.BASELINE(sample$) — cattura baseline

DIM bl$ = VECTORDB.BASELINE('{"p_at":0.8,"recall":0.7,"latency_ms":150}')


### 8.7 VECTORDB.DRIFT(baseline$, window$) — Population Stability Index

DIM psi = VECTORDB.DRIFT('{"values":[1,2,3,4,5,6,7,8,9,10]}', '{"values":[8,9,10,11,12,13,14,15,16,17]}')
' PSI < 0.1 = no drift, 0.1-0.25 = warning, > 0.25 = critical


### 8.8 VECTORDB.ABTEST(variantA$, variantB$) — test A/B statistico

DIM ab$ = VECTORDB.ABTEST('{"n":100,"success":60}', '{"n":100,"success":40}')
' → {"z_score":2.89,"p_value":0.0039,"effect_size":0.2}


### 8.9 VECTORDB.COMPARE(configs$) — confronto configurazioni

DIM cfg$ = '[{"config":{"name":"base"},"results":{"p_at":0.5,"recall":0.4}},{"config":{"name":"tuned"},"results":{"p_at":0.9,"recall":0.8}}]'
DIM cmp$ = VECTORDB.COMPARE(cfg$)
' → {"winner":"tuned","metrics":{"p_at":{"base":0.5,"tuned":0.9,"winner":"tuned"},...}}


### Backend
| Backend | DIAGNOSE | CALIBRATE | EVALGATE/DIFF | DRIFT | ABTEST | COMPARE |
|---|---|---|---|---|---|---|
| IL (C#) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| PCodeVM (C#) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| VM/C | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| C99Native | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

### Riferimenti

### Second-Brain-Guide

*Fonte: `Second-Brain-Guide`*


## 0. Reference Card — tutti i comandi e le funzioni del Second Brain

### Vault (L1)
| Comando | Significato |
|---|---|
| `AI VAULT CREATE nome, cartella [, pattern]` | registra un vault (folder .md) |
| `AI VAULT INDEX [nome]` | indicizza (hash + folder) |
| `AI VAULT WATCH nome, AUTO\|MANUAL` | watchdog on-change (per-sessione) |
| `AI VAULT ADD FOLDER nome, path [, INDEX ON\|OFF]` | registra una sottocartella (subvault) |
| `AI VAULT REMOVE FOLDER nome, path` | rimuove la voce cartella |
| `AI VAULT SET FOLDER nome, path, INDEX ON\|OFF` | imposta `indexed` della cartella |
| Funzione | Ritorna |
|---|---|
| `AI.VAULTINFO(nome)` | JSON info (root/pattern/fileCount/indexVersion) |
| `AI.VAULTFOLDERS(nome)` | cartelle del vault |
| `AI.VAULTCOUNT(nome)` | numero di note indicizzate |
| `AI.VAULTLIST()` | elenco vault registrati |
| `AI.VAULTPOLICY(nome)` | `"AUTO"` / `"MANUAL"` |

### Note / Frontmatter (L2)
| Comando | Funzione equivalente |
|---|---|
| `AI NOTE CREATE path [, contenuto] [, tags]` | — |
| `AI NOTE UPDATE path, contenuto` | `AI.NOTEGET(vault, path)` |
| `AI NOTE APPEND path, testo` | `AI.NOTETITLE(vault, path)` |
| `AI NOTE DELETE path` | `AI.NOTETAGS(vault, path)` |
| `AI NOTE MOVE path, nuovo` | `AI.NOTEWORDCOUNT(vault, path)` |
| — | `AI.NOTEEXISTS(vault, path)` → 1/0 |
| — | `AI.NOTEFOLDER(vault, path)` |

### Linking (L3)
| Comando | Funzione |
|---|---|
| `AI NOTE LINK from TO to [AS alias]` | `AI.NOTELINK(vault, from, to [, alias])` |
| `AI NOTE UNLINK from TO to` | `AI.NOTEUNLINK(vault, from, to)` |
| `AI NOTE SET ALIAS from TO to AS alias` | `AI.NOTESETALIAS(vault, from, to, alias)` |
| — | `AI.LINKSOUT(vault, path)` / `AI.BACKLINKS(vault, path)` |
| — | `AI.LINKDANGLING(vault)` / `AI.LINKJSON(vault)` |

### RAG / ricerca (L4)
| Funzione | Significato |
|---|---|
| `AI.SEARCHMULTI(query [, topK] [, coll] [, brains] [, reranker] [, minEvidence] [, explain])` | ricerca su più brain, con provenance |
| `AI.RAGCONTEXTMULTI(query [, topK] [, reranker] [, minEvidence])` | contesto grounded |
| `AI.ASKMULTI(domanda [, topK] [, reranker] [, minEvidence])` | risposta LLM su più brain |
| `AIVECTOR.SEARCH(db, coll, query [, topK] [, hybrid] [, filter] [, reranker] [, minEvidence] [, explain])` | ricerca su una singola collezione |
| `AIVECTOR.RAGCONTEXT(db, coll, query [, topK] [, reranker] [, minEvidence])` | contesto grounded su una collezione |
| `AIVECTOR.ASK(db, coll, domanda [, topK] [, reranker] [, minEvidence])` | risposta LLM su una collezione |
| `AI.QAANSWER(domanda, brain [, window])` | QA grounded con citations |
| `AI.ENRICH(testo, mode)` | SUMMARIZATION/TAGGING/ENTITY_EXTRACTION |
| `AI.SUGGESTTAGS(testo)` | tag suggeriti |

### Knowledge Graph (L5)
| Comando | Significato |
|---|---|
| `BRAINS BUILD dir [, outputDir]` | costruisce il grafo arricchito (8 campi) |
| `BRAINS UPDATE dir [, outputDir]` | ricostruisce |
| `BRAINS FIND PATH from TO to` | path più corto (stampa) |
| `BRAINS RDD MEMORY\|JSON\|SQLITE` | backend del grafo |
| `GRAPH VIEW [-g dir] [-n nome]` | apre nel browser il viewer interattivo `graph.html` |
| `GRAPH WATCH <wikiDir> [-g dir] [-n nome]` | auto-rebuild del grafo a ogni modifica .md (watchdog) |
| `GRAPH UNWATCH <wikiDir>` | ferma il watchdog |
| `GRAPH EXPORT OBSIDIAN [-g dir] [-n nome] [--out dir]` | esporta un vault Obsidian navigabile |
| `GRAPH ENRICH SEMANTIC <wikiDir> [-g dir] [-n nome] [--max N]` | l'LLM (anche modello locale) deduce relazioni implicite tra le note |
| Funzione | Ritorna |
|---|---|
| `BRAINS.Adjacency(brain)` | ponte verso GRAPH/FLOW: nodi + archi (indice = riga/colonna) |
| `BRAINS.Neighbors(brain, nota)` | vicini diretti (out+in) |
| `BRAINS.Path(brain, da, a)` | cammino più corto |
| `BRAINS.Stats(brain)` | nodi/edge/dangling/PageRank top |
| `BRAINS.IsValid(brain)` | 1 se il brain ha almeno un nodo |
| `BRAINS.QueryMulti(query [, brains])` | ricerca multi-brain graph |
| `BRAINS.Dot(brain)` | export Graphviz |
| `BRAINS.ByTag(brain, tag)` | JSON per tag |
| `BRAINS.ByFolder(brain, folder)` | JSON per cartella |

> **La teoria dei grafi non è più in `BRAINS`** (2026-11-06): `BRAINS` espone solo l'accesso
> AI/dati del cervello (query, ispezione, export). BFS, DFS, Dijkstra, PageRank, Louvain,
> betweenness, clustering, diametro, eccentricità, MST, SCC, componenti connesse, topo-sort,
> sottografo, bipartiteness, has-cycle e max-flow vivono **una sola volta** nei pacchetti FFI
> `GRAPH`/`FLOW` (`IMPORT GRAPH`, `IMPORT FLOW`), su matrici esplicite — vedi IV.21 e
> `shared/samples/graph/GraphFFILab.bas`. La mappa `BRAINS.X → GRAPH.Y` è nel commento di testa
> di `GMBasicSharp/common/StdLib/StdLib.Brains.cs`.
>
> Il ponte è `BRAINS.Adjacency(brain)`: ritorna JSON con `n`, `nodes` (ordine = indice di
> riga/colonna) ed `edges` come coppie `[i,j]`; da lì si riempie la matrice n×n e si chiamano
> le funzioni `GRAPH.*`. È topologico (peso uniforme): i pesi di confidenza degli archi
> inferiti non sono nel payload.
| `AI.AGENTRUNGRAPH(nome, prompt [, rounds])` | un agente esplora il grafo con i tool `graph.*` |
| `AI.AGENTRUNRAG(nome, prompt [, rounds])` | un agente interroga i brain RAG con i tool `rag.*` (speculare a `AGENTRUNGRAPH`; senza provider/brain → errore esplicito) |

### Ingestion (L6)
| Comando | Funzione |
|---|---|
| `AI INGEST FILE file [, relPath]` | importa file (testo/PDF/OCR/audio) |
| `AI INGEST FOLDER cartella [, preserve]` | importa cartella |
| `AI INGEST WEB url [, relPath]` | scarica pagina come nota |
| `AI SYNC FOLDER src TO vault [, direction] [, conflict]` | sincronizza cartella |
| — | `AI.SYNCFOLDER(vault, src [, direction [, conflict]])` |
| — | `AI.INGESTWEB(vault, url [, relPath])` |

### Brain (L6)
| Comando | Funzione |
|---|---|
| `AI BRAIN USE a, b, c` | collega uno o più brain |
| `AI BRAIN DETACH nome` | scollega |
| `AI BRAIN INSTALL nome [, sorgente]` | installa dal manifest (download on demand) o da zip/URL |
| `AI BRAIN UNINSTALL nome` | rimuove |
| — | `AI.BRAINLIST()` / `AI.BRAININFO(nome)` / `AI.BRAINUSED()` |

### Memoria (§3)
| Comando | Funzione |
|---|---|
| `AI MEMORY NOTE testo` | registra un fatto |
| — | `AI.MEMORYRANK(tema [, topK])` |
| — | `AI.MEMORYCONSOLIDATE()` |

### Provenance (§2)
| Funzione | Significato |
|---|---|
| `AI.SIGNNOTE(contenuto, chiave)` | firma HMAC-SHA256 (hex) |
| `AI.VERIFYNOTE(contenuto, chiave, firma)` | verifica (1/0) |
| `AI.SHAREENCRYPT(contenuto, chiave)` | cifra AES-256-GCM (base64) |
| `AI.SHAREDECRYPT(cifrato, chiave)` | decifra |

### Brain Vault (§2)
| Funzione | Significato |
|---|---|
| `AI.BRAINVAULTBUILD(outputDir)` | genera documentazione StdLib + RAG |
| `AI.BRAINVAULTQUERY(query [, topK])` | retrieval semantico |
| `AI.BRAINVAULTWRITE(richiesta [, topK])` | genera codice con contesto |

### REPL / CLI
| REPL | CLI |
|---|---|
| `BRAIN USE/DETACH/LIST/INFO/INSTALL/EXPORT/SIGN` | `gmb brain use\|detach\|list\|info\|install\|export\|sign` |
| `BRAINS BUILD <dir>` · `RAG BUILD <dir>` | `gmb graph build <dir>` · `gmb rag build <dir>` |

## 1. Concetti in 60 secondi
| Termine | Significato |
|---|---|
| **vault** | una cartella di file `.md` (con frontmatter YAML `---`) — la tua libreria di note |
| **brain** | un vault registrato e indicizzato: può essere disponibile come **rag** (ricerca semantica) e/o come **graph** (relazioni) |
| **RAG** | ricerca per significato: il testo viene "vettorizzato" e cerchi i concetti, non le parole |
| **graph** | rete di note collegate da wikilink `[[nota]]`, con **relazioni pesate** (stessa cartella, stessi tag, menzioni…) |
| **wikilink** | `[[nome-nota]]` dentro una nota: crea un collegamento navigabile |
| **backlink** | note che puntano alla nota che stai guardando |
| **provenance** | firma di integrità: dimostra che una nota non è stata alterata |

1. AI VAULT CREATE    → registri una cartella come vault
2. AI NOTE CREATE     → scrivi note (frontmatter + wikilink)
3. AI INGEST FILE     → importi documenti (testo, PDF, OCR, audio)
4. BRAINS BUILD        → costruisci il grafo delle relazioni
5. AI BRAIN USE       → colleghi il brain (o più brain)
6. AI.QAANSWER / BRAINS.QueryMulti → fai domande e interroghi le relazioni


## 2. Vault: la tua libreria di note

### `AI VAULT CREATE nome, cartella [, pattern]`

AI VAULT CREATE "wiki", "/Users/gaetano/Documents/AiAirlineWiki"


### `AI VAULT INDEX [nome]`

AI VAULT INDEX "wiki"


### `AI VAULT WATCH nome, AUTO|MANUAL` — watchdog opzionale

AI VAULT WATCH "wiki", AUTO
PRINT AI.VAULTPOLICY("wiki")   ' AUTO
AI VAULT WATCH "wiki", MANUAL


### Subvault e props per cartella
| Comando | Cosa fa |
|---|---|
| `AI VAULT ADD FOLDER name, path [, INDEX ON\|OFF]` | registra una sottocartella come subvault (con guard anti-escape) |
| `AI VAULT REMOVE FOLDER name, path` | rimuove la voce della cartella (non cancella i file) |
| `AI VAULT SET FOLDER name, path, INDEX ON\|OFF` | imposta la prop `indexed` della cartella |

AI VAULT ADD FOLDER "wiki", "research"
AI VAULT SET FOLDER "wiki", "research", INDEX ON


### Funzioni di lettura
| Funzione | Ritorna |
|---|---|
| `AI.VAULTINFO(nome)` | JSON con percorso, numero note, ultimo indice |
| `AI.VAULTFOLDERS(nome)` | cartelle del vault (e quanti file contengono) |
| `AI.VAULTCOUNT(nome)` | numero di note |
| `AI.VAULTLIST()` | elenco dei vault registrati |

PRINT AI.VAULTINFO("wiki")
PRINT AI.VAULTCOUNT("wiki")


## 3. Note: scrivere e leggere

---
title: Strategia di trading
tags: [trading, strategy]
date: 2026-08-01
author: gaetano
---
Questa nota spiega la strategia. Vedi [[momentum]] per il dettaglio.


### Comandi `AI NOTE`
| Comando | Cosa fa |
|---|---|
| `AI NOTE CREATE path [, contenuto] [, tags]` | crea (o sovrascrive) la nota nel vault **attivo** |
| `AI NOTE UPDATE path, contenuto` | sostituisce il **body** preservando il frontmatter esistente (title/tags) |
| `AI NOTE APPEND path, testo` | accoda testo in fondo al body (frontmatter preservato) |
| `AI NOTE DELETE path` | elimina la nota |
| `AI NOTE MOVE path, nuovoPath` | sposta/rinomina |

AI NOTE CREATE "trading/momentum.md", "Il momentum segue la forza del trend.", "trading"
AI NOTE APPEND "trading/momentum.md", "Vedi anche [[trend-following]]."


### Funzioni `AI.NOTE*`
| Funzione | Ritorna |
|---|---|
| `AI.NOTEGET(vault, path)` | contenuto (body) della nota |
| `AI.NOTETITLE(vault, path)` | titolo: **primo H1 (`# …`) del body**, altrimenti il nome file senza estensione (il campo `title` del frontmatter NON è usato) |
| `AI.NOTETAGS(vault, path)` | tag del frontmatter, uniti con `,` (es. `alpha,beta`) |
| `AI.NOTEWORDCOUNT(vault, path)` | numero di parole (solo il body) |
| `AI.NOTEEXISTS(vault, path)` | 1 se la nota esiste |
| `AI.NOTEFOLDER(vault, path)` | cartella della nota |

## 4. Linking: wikilink e backlink
| Funzione | Ritorna |
|---|---|
| `AI.LINKSOUT(vault, path)` | note a cui punta la nota (wikilink uscenti) |
| `AI.BACKLINKS(vault, path)` | note che puntano alla nota (incoming) |
| `AI.LINKDANGLING(vault)` | link che non risolvono a nessuna nota (link rotti) |
| `AI.LINKJSON(vault)` | l'intera rete di link in JSON |

PRINT AI.BACKLINKS("wiki", "trading/momentum.md")
PRINT AI.LINKDANGLING("wiki")   ' link da sistemare


### Link manuali (senza modificare il file)
| Comando/Funzione | Cosa fa |
|---|---|
| `AI NOTE LINK from TO to [AS alias]` | crea un link manuale a→b (con alias opzionale) |
| `AI.NOTELINK(vault, from, to [, alias])` | idem con vault esplicito |
| `AI NOTE UNLINK from TO to` / `AI.NOTEUNLINK(...)` | rimuove il link manuale |
| `AI NOTE SET ALIAS from TO to AS alias` | imposta l'alias del link |

AI NOTE LINK "alpha.md" TO "beta.md" AS "Approfondimento"
PRINT AI.LINKSOUT("wiki", "alpha.md")   ' include il link manuale


## 5. Brain: collegare i cervelli

### Comandi `AI BRAIN`
| Comando | Cosa fa |
|---|---|
| `AI BRAIN USE a, b, c` | collega uno o più brain (per interrogarli insieme) |
| `AI BRAIN DETACH nome` | scollega un brain |
| `AI BRAIN INSTALL nome [, sorgente]` | installa un brain: senza sorgente lo prende dal **manifest** (`shared/settings/brains-manifest.json`, download on demand con resume+sha256); con sorgente da zip/URL/directory |
| `AI BRAIN UNINSTALL nome` | rimuove il brain registrato |

### Dove sta un cervello (risoluzione e cache, 2026-10-31)

### CLI: `gmb brain` (login, install, list, remove)
| comando | cosa fa |
|---|---|
| `gmb brain login --github [--token T \| --token-file F]` | valida il token (`GET /user`) e lo salva in `~/.gmbasic/github.token` con permessi **0600** |
| `gmb brain logout` | rimuove il token |
| `gmb brain install <nome[@versione]>` | installa dal **manifest** dei cervelli (download on demand, sha256 verificato) e lo **collega** |
| `gmb brain install <nome> --source <url\|zip\|dir>` | installa da una sorgente esplicita |
| `gmb brain list` | cervelli trovati con versioni, modalità (`rag`/`graph`) e percorso risolto |
| `gmb brain remove <nome[@versione]>` | rimuove **solo dalla cache** |

### Asset PRIVATI (`source: "github-asset"`)

{ "version": "1.0.0", "source": "github-asset",
  "repo": "c-vision/GMBasicBrains", "tag": "brains-v1",
  "asset": "brainvault-1.0.0.gmbrain.zip",
  "sha256": "e4bf…", "size": 573998 }


### Dove vivono i cervelli (e perché `shared/rag/` è vuota)

~/.gmbasic/brains/<nome>/<versione>/rag/<nome>.db


### Un cervello è RAG **e/o** GRAPH (un solo pacchetto)
| forma | file nel pacchetto | a cosa serve |
|---|---|---|
| `rag` | `rag/<nome>.db` | ricerca semantica (`AI.SEARCHMULTI`, `AI.RAGCONTEXT…`) |
| `graph` | `graphs/<nome>.json` | knowledge graph (`BRAINS.QueryMulti`, path/community/pagerank) |

~/.gmbasic/brains/<nome>/<versione>/
├── brain.manifest.json          (files + sha256 di OGNI forma)
├── rag/<nome>.db                (se modes contiene "rag")
└── graphs/<nome>.json           (se modes contiene "graph")


### I cervelli pubblicati (`brains-v2`, repo privato)
| cervello | versione | download | contenuto |
|---|---|---|---|
| `brainvault` | 1.0.0 | 561 KB | RAG del vault del progetto (1,2 MB) |
| `airline` | **1.1.0** | 30 MB | RAG 108 MB **+ graph 83 MB** |
| `trading` | **1.1.0** | **583 MB** | RAG **1,4 GB** **+ graph 40 MB** |
| `finance` | 1.0.0 | 3,4 MB | **graph-only** (40 MB) |

gmb brain login --github        # una volta: token con accesso a c-vision/GMBasicBrains
gmb brain install trading       # 579 MB, resume+sha256, ~1m30s su banda normale
gmb brain list                  # cosa è installato e dove


### Fixture di mercato on demand (`gmb fixtures`, F4)

gmb fixtures list                 # dichiarate nel manifest + quali sono presenti
gmb fixtures pull darkpool        # scarica+estrae in shared/samples/Fixtures/darkpool/
gmb fixtures pull --all           # tutte (markets 8 MB, option_chains 39 MB, rates 752 KB, darkpool 72 KB)


### Programma DISTRIBUITO: i cervelli viaggiano con l'eseguibile (F3)
| file accanto all'eseguibile | cosa contiene |
|---|---|
| `settings/brains-manifest.json` | snapshot del manifest dei cervelli (metadati, pochi KB) — il runtime lo legge **prima** di quello del checkout |
| `brains.txt` / `<exe>.brains.txt` | i cervelli **dichiarati dal programma**: `"brains": ["brainvault", "trading@1.0.0"]` nel suo `app.settings.json` (una voce per riga: `nome<TAB>versione`) |

gmbbrains <eseguibile | cartella> [--only <nome>] [--token-file <file>]


### Scope dei cervelli: multi-brain, per-agente e per-task (2026-09-16)

### Funzioni `AI.BRAIN*`
| Funzione | Ritorna |
|---|---|
| `AI.BRAINLIST()` | tutti i brain registrati |
| `AI.BRAININFO(nome)` | JSON: modalità (rag/graph/both), disponibilità, checksum |
| `AI.BRAINUSED()` | brain attualmente collegati |

AI BRAIN USE "wiki", "finanza"
PRINT AI.BRAINUSED()          ' wiki,finanza
PRINT AI.BRAINLIST()


## 6. Ricerca multi-brain e domande
| Funzione | Ritorna |
|---|---|
| `AI.SEARCHMULTI(query [, topK] [, collection] [, brains] [, reranker] [, minEvidence] [, explain])` | risultati JSON da tutti i brain, con campo `brain` |
| `AI.RAGCONTEXTMULTI(query [, topK] [, reranker] [, minEvidence])` | contesto grounded (testo formattato con attribuzioni) |
| `AI.ASKMULTI(domanda [, topK] [, reranker] [, minEvidence])` | risposta LLM basata sui chunk recuperati da tutti i brain |

AI BRAIN USE "wiki", "finanza"
PRINT AI.SEARCHMULTI("cos'e il momentum", 5)
PRINT AI.RAGCONTEXTMULTI("cos'e il momentum", 5)
PRINT AI.ASKMULTI("spiega il momentum con le fonti", 5)


### Ottimizzazione dei risultati: reranker, evidenza, explain (P1-2b)
| Parametro | Tipo | Default | Effetto |
|---|---|---|---|
| `reranker` | numero (`%RERANK_*`) | `%RERANK_KEYWORD` | modalità di re-ranking del risultato |
| `minEvidence` | numero | disattivata (NaN) | soglia di evidenza: sotto soglia il chunk/brain viene escluso |
| `explain` | `TRUE`/`FALSE` | `FALSE` | include informazioni di spiegazione (solo `SEARCHMULTI`/`AIVECTOR.SEARCH`) |

IMPORT AI
PRINT AI.SEARCHMULTI("momentum", 5, "wiki", NULL, %RERANK_KEYWORD, 0.3, TRUE)
PRINT AI.SEARCHMULTI("momentum", 5, "wiki", NULL, %RERANK_OFF)
PRINT AI.RAGCONTEXTMULTI("momentum", 5, %RERANK_KEYWORD, 0.3)
PRINT AIVECTOR.SEARCH("wiki", "collezione", "momentum", 5, TRUE, NULL, %RERANK_OFF)


## 7. Knowledge Graph: le relazioni
| Tipo relazione | Cosa significa | Peso |
|---|---|---|
| `LINK` | wikilink risolto `[[a]]→[[b]]` | 1.0 |
| `TAG_SHARE` | tag in comune (similarità Jaccard) | Jaccard |
| `FOLDER_CHILD` | note nella stessa cartella | 0.8 |
| `CONTENT_MENTION` | una nota cita il titolo di un'altra | 0.6 |
| `TEMPORAL_NEAR` | date a pochi giorni di distanza | decay |

### Comandi legacy `GRAPH`
| Comando | Cosa fa |
|---|---|
| `BRAINS BUILD cartella [, outputDir]` | costruisce il grafo arricchito (8 campi + relazioni pesate) |
| `BRAINS UPDATE cartella [, outputDir]` | ricostruisce |
| `BRAINS FIND PATH "a" TO "b"` | cammino più corto tra due note |
| `BRAINS RDD MEMORY\|JSON\|SQLITE` | backend del grafo (in memoria, su file JSON o in un database SQLite `.graph.db`) |

BRAINS RDD MEMORY
BRAINS BUILD "../Fixtures/graph-vault"
BRAINS FIND PATH "index.md" TO "trading/momentum.md"


### Funzioni `BRAINS.*`
| Funzione | Ritorna |
|---|---|
| `BRAINS.Neighbors(brain, nota)` | vicini diretti di una nota |
| `BRAINS.Path(brain, da, a)` | cammino più corto (BFS) |
| `BRAINS.Stats(brain)` | nodi/relazioni/dangling + top PageRank |
| `BRAINS.QueryMulti(query [, brains])` | ricerca su più brain graph uniti |
| `BRAINS.Dot(brain)` | export Graphviz (dot) |
| `BRAINS.ByTag(brain, tag)` | JSON dei nodi con quel tag + i loro edge |
| `BRAINS.ByFolder(brain, folder)` | JSON dei nodi nella cartella (o sotto) + edge |
| `BRAINS.IsValid(brain)` | 1 se il grafo ha almeno un nodo |

> **Gli algoritmi non sono in `BRAINS`** (dal 2026-11-06). PageRank, componenti connesse/SCC,
> topo-sort, bipartiteness, betweenness, clustering, diametro, eccentricità, MST, DFS/BFS,
> Dijkstra, Louvain, max-flow e sottografo vivono una sola volta nel pacchetto FFI
> `GRAPH`/`FLOW` e lavorano su matrici esplicite: materializza il grafo del cervello in
> (nodi, matrice) e usa `GRAPH.*` — vedi `shared/samples/graph/GraphFFILab.bas`.

BRAINS BUILD "../Fixtures/graph-vault"
PRINT BRAINS.Neighbors("graph-vault", "index.md")
PRINT BRAINS.Path("graph-vault", "index.md", "trading/momentum.md")
PRINT BRAINS.QueryMulti("momentum", "graph-vault,strategies")
PRINT BRAINS.Dot("graph-vault")
PRINT BRAINS.ByTag("graph-vault", "strategy")
PRINT BRAINS.ByFolder("graph-vault", "strategies")


### 7bis. Visualizzare il grafo — viewer interattivo (2026-09-06)

gmb graph build /percorso/wiki          # genera <nome>.{json,graphml,mermaid,stats.json,report.md,html}
gmb graph build /percorso/wiki -H       # -H/--no-html salta il viewer (wiki molto grandi: es. 28k nodi = html enorme)
gmb graph view                          # apre graph.html nel browser (rigenera se il json è più nuovo)


### 7bis-bis. Watchdog auto-rebuild (2026-09-06)

gmb graph watch /percorso/wiki [-g dir] [-n nome]     # resta attivo finché Ctrl+C
gmb graph unwatch /percorso/wiki                       # ferma (dalla CLI il watch è bloccante)


### 7ter. Semantic extraction LLM — GRAPH ENRICH SEMANTIC (2026-09-06)

gmb graph enrich-semantic /percorso/wiki [-g dir] [-n nome] [--max N] [--batch N]
# dal REPL:  GRAPH ENRICH SEMANTIC /percorso/wiki


### 7quater. Esportare come vault Obsidian (2026-09-06)

gmb graph export-obsidian --graphs graphs --name momentum --out /tmp/mom_vault
# oppure, dal REPL:  GRAPH EXPORT OBSIDIAN -g graphs -n momentum --out /tmp/mom_vault


### 7quinquies. Algoritmi avanzati e agenti che esplorano il grafo (2026-09-07)

result$ = AI.AGENTRUNGRAPH("consulente", "Cerca nel grafo come si collegano momentum e risk parity e riassumi")
' il loop agente chiama graph.query → graph.path → graph.neighbors → ... e risponde


## 8. Ingestion: importare documenti
| Comando | Cosa fa |
|---|---|
| `AI INGEST FILE sorgente [, pathRelativo]` | copia un file nel vault attivo (testo), oppure lo **converte** (PDF/docx → testo, immagini → OCR, audio → trascrizione) |
| `AI INGEST FOLDER cartella [, preservaStruttura]` | importa un'intera cartella (ricorsiva se `preservaStruttura` = 1) |

AI INGEST FILE "contratto.pdf", "il contratto shared/ (vedi Struttura-Monorepo)"
AI INGEST FOLDER "/tmp/dossier", 1


### Sincronizzazione cartella e Web
| Comando/Funzione | Cosa fa |
|---|---|
| `AI SYNC FOLDER src TO vault [, direction] [, conflict]` | sincronizza una cartella col vault (`direction` = `bidirectional`\|`push_only`; `conflict` = `newest_wins`). Ritorna `importati/aggiornati/conflitti` |
| `AI.SYNCFOLDER(vault, src [, direction [, conflict]])` | idem con vault esplicito |
| `AI INGEST WEB url [, relPath]` | scarica una pagina (HTTP GET), ne estrae il testo e la salva come nota `.md` |
| `AI.INGESTWEB(vault, url [, relPath])` | idem con vault esplicito |

AI SYNC FOLDER "/shared/research" TO "wiki"
AI INGEST WEB "https://example.com", "web/example.md"


## 9. Memoria: fatti che restano
| Comando/Funzione | Cosa fa |
|---|---|
| `AI MEMORY NOTE testo` | registra un fatto in memoria |
| `AI.MEMORYRANK(tema [, topK])` | fatti più rilevanti per il tema, ordinati per punteggio |
| `AI.MEMORYCONSOLIDATE()` | consolida (deduplica fatti simili) |

AI MEMORY NOTE "l'utente preferisce SQLite per i dati"
AI MEMORY NOTE "il server di test e' su :8899"
PRINT AI.MEMORYRANK("SQLite", 5)
AI.MEMORYCONSOLIDATE()


## 10. Enrichment: chiedere all'LLM
| Funzione | Cosa fa |
|---|---|
| `AI.ENRICH(testo, "SUMMARIZATION"\|"TAGGING"\|"ENTITY_EXTRACTION")` | sintetizza, suggerisce tag, estrae entità |
| `AI.SUGGESTTAGS(testo)` | scorciatoia per i tag |
| `AI.QAANSWER(domanda, brain [, finestra])` | risposta basata sui chunk del brain, con **citazioni** (JSON) |

PRINT AI.SUGGESTTAGS("la strategia usa RSI e momentum sui futures")
PRINT AI.QAANSWER("quale e' la capitale della Francia?", "kb", 3)


## 11. Provenance: integrità delle note
| Funzione | Cosa fa |
|---|---|
| `AI.SIGNNOTE(contenuto, chiave)` | firma hex (64 caratteri) del contenuto |
| `AI.VERIFYNOTE(contenuto, chiave, firma)` | 1 se la firma è autentica, 0 se alterata |

S$ = AI.SIGNNOTE("nota di test per provenance", "chiave-segreta-42")
PRINT AI.VERIFYNOTE("nota di test per provenance", "chiave-segreta-42", S$)   ' 1
PRINT AI.VERIFYNOTE("nota alterata", "chiave-segreta-42", S$)                 ' 0


## 11bis. Brain Vault: il cervello delle funzioni GMBasic
| Funzione | Cosa fa |
|---|---|
| `AI.BRAINVAULTBUILD(outputDir)` | genera `brain/<Ns>.md` dalla StdLib e li indicizza in RAG (ritorna numero di pagine) |
| `AI.BRAINVAULTQUERY(query [, topK])` | retrieval semantico: i namespace più rilevanti con le loro firme (JSON) |
| `AI.BRAINVAULTWRITE(richiesta [, topK])` | genera codice BASIC usando il brain vault come contesto |

PRINT AI.BRAINVAULTBUILD("work")
PRINT AI.BRAINVAULTQUERY("ricerca vettoriale su collezioni sqlite", 3)
PRINT AI.BRAINVAULTWRITE("scrivi una funzione che somma due numeri", 3)


## 12. Da REPL e CLI

BRAIN USE nome            ' collega un brain
BRAIN LIST / INFO nome    ' elenco / dettagli
BRAIN INSTALL nome, file.zip
BRAIN EXPORT nome         ' esporta come zip
BRAIN SIGN nome           ' firma e verifica il brain (probe minimi graph/vectordb)
BRAINS BUILD <cartella>    ' costruisci il grafo (scrive .json/.graphml/.mermaid/.stats.json/.report.md/.html)
GRAPH VIEW                ' apri il viewer interattivo del grafo nel browser
GRAPH WATCH <cartella>    ' auto-rebuild del grafo a ogni modifica .md (UNWATCH per fermare)
GRAPH EXPORT OBSIDIAN     ' esporta il grafo come vault Obsidian navigabile
GRAPH ENRICH SEMANTIC <cartella>   ' l'LLM deduce relazioni implicite tra le note
RAG BUILD <cartella>      ' costruisci l'indice semantico


gmb brain use nome
gmb brain list
gmb brain info nome
gmb brain install nome, fonte
gmb brain export nome
gmb brain sign nome
gmb rag build <cartella>
gmb graph build <cartella>
gmb graph view [-g dir] [-n nome]          # apre graph.html nel browser
gmb graph watch <cartella> [-g dir] [-n nome]   # auto-rebuild a ogni modifica .md
gmb graph export-obsidian [-g dir] [-n nome] [--out dir]
gmb graph enrich-semantic <cartella> [-g dir] [-n nome] [--max N]   # l'LLM deduce relazioni implicite
# opzioni build: -L/--louvain (community Louvain anziché label propagation), -H/--no-html (salta il viewer)


## 13. Ricetta completa: dal zero a una domanda

' 1. Vault: registra e indicizza la tua cartella di note
AI VAULT CREATE "Appunti", "/Users/gaetano/Appunti"
AI VAULT INDEX "Appunti"

' 2. Scrivi una nota con wikilink
AI NOTE CREATE "strategia.md", "Usiamo momentum e mean reversion. Vedi [[rischio]]."

' 3. Costruisci il grafo (le relazioni tra le note). Il nome del brain è il basename
'    della cartella: qui "Appunti".
BRAINS RDD MEMORY
BRAINS BUILD "/Users/gaetano/Appunti"

' 4. Collega il brain
AI BRAIN USE "Appunti"

' 5. Interroga


## 14. Limiti e buone pratiche

### VECTORDB-Guide

*Fonte: `VECTORDB-Guide`*


## 1. Concetti

## 2. Gestione collection

### `VECTORDB.ENSURE COLLECTION db$, coll$ [, dim%]`

IMPORT VECTORDB
VECTORDB ENSURE COLLECTION "AdminDb", "knowledge"
VECTORDB ENSURE COLLECTION "AdminDb", "knowledge", 8
' Legacy:
VECTORDB ENSURE COLLECTION "AdminDb", "knowledge"


### `VECTORDB.LISTCOLLECTIONS(db$) → STRING` (JSON array)

PRINT VECTORDB.LISTCOLLECTIONS("AdminDb") ' ["knowledge","documents"]


### `VECTORDB.COUNT(db$, coll$) → LONG`

PRINT VECTORDB.COUNT("AdminDb", "knowledge")


## 3. Indicizzazione

### `VECTORDB.INDEX TEXT db$, coll$, id$, text$`

VECTORDB INDEX TEXT "AdminDb", "knowledge", "k1", "Vettori e motori di search semantico"


### `VECTORDB.INDEX META db$, coll$, id$, text$, metaJson$`

VECTORDB INDEX META "AdminDb", "documents", "d2", "Report annuale 2026", "{ ""type"": ""report"", ""year"": 2026 }"


## 4. Ricerca

### `VECTORDB.SEARCH db$, coll$, query$, topK% [INTO arr$()]`

' Ritorno JSON
PRINT VECTORDB.SEARCH("AdminDb", "knowledge", "database vettoriali", 3)

' Ritorno array
DIM res$() AS STRING
VECTORDB SEARCH "AdminDb", "knowledge", "database vettoriali", 3 INTO res$()
FOR i% = 0 TO UBOUND(res$())
  PRINT res$(i%)
NEXT

' Funzione qualificata
DIM r$ = VECTORDB.SEARCH("AdminDb", "knowledge", "retrieval", 2)


### `VECTORDB.RAGCONTEXT db$, coll$, query$, topK% → STRING`

PRINT VECTORDB.RAGCONTEXT("AdminDb", "knowledge", "database", 3)


## 5. Gestione documenti
| Comando | Descrizione |
|---|---|
| `VECTORDB.REMOVE SOURCE db$, coll$, id$` | Rimuove documento per id |
| `VECTORDB.PURGE DEPRECATED db$, coll$` | Rimuove deprecati |
| `VECTORDB.DELETE TEXT db$, coll$, id$` | Alias di REMOVE |

VECTORDB REMOVE SOURCE "AdminDb", "documents", "d1"
VECTORDB PURGE DEPRECATED "AdminDb", "logs"


## 6. Confronto con AI RAG
|  | `VECTORDB` (autonomo) | `AI RAG` (integrato AI) |
|---|---|---|
| **DB** | `VECTORDB.ENSURE COLLECTION "AdminDb", "knowledge"` | `AI RAG ENSURE COLLECTION ":memory:", "products", 8` |
| **Index** | `VECTORDB.INDEX TEXT "AdminDb", "knowledge", "k1", "testo"` | `AI RAG INDEX TEXT ":memory:", "products", "prod-1", "Notebook i7"` |
| **Search** | `VECTORDB.SEARCH("AdminDb", "knowledge", "query", 3)` | `AI RAG SEARCH ":memory:", "products", "query", 3` |
| **Provider** | embedding interno fisso | provider configurabile (`AI REGISTER %EMB`) |

## 7. Esempi completi

### Esempio 1 — CRUD autonomo (da `VectorDbAdmin.bas`)

IMPORT VECTORDB
VECTORDB ENSURE COLLECTION "AdminDb", "knowledge"
VECTORDB ENSURE COLLECTION "AdminDb", "documents"

VECTORDB INDEX TEXT "AdminDb", "knowledge", "k1", "Vettori e motori di search semantico"
VECTORDB INDEX TEXT "AdminDb", "knowledge", "k2", "Retrieval Augmented Generation con LLM"
PRINT VECTORDB.COUNT("AdminDb", "knowledge") ' 2

DIM res$ = VECTORDB.SEARCH("AdminDb", "knowledge", "database vettoriali", 3)
PRINT res$

VECTORDB REMOVE SOURCE "AdminDb", "knowledge", "k1"
PRINT VECTORDB.COUNT("AdminDb", "knowledge") ' 1


### Esempio 2 — Con metadati e RAG

IMPORT VECTORDB
VECTORDB ENSURE COLLECTION "AdminDb", "docs"
VECTORDB INDEX META "AdminDb", "docs", "d2", "Report annuale 2026", "{ ""year"": 2026 }"

DIM ctx$ = VECTORDB.RAGCONTEXT("AdminDb", "docs", "report annuale", 2)
PRINT ctx$ ' contesto concatenato per LLM


### Esempio 3 — AI RAG vs VECTORDB

IMPORT VECTORDB
IMPORT AI

' VECTORDB autonomo
VECTORDB ENSURE COLLECTION "AdminDb", "knowledge"
VECTORDB INDEX TEXT "AdminDb", "knowledge", "k1", "testo"

' AI RAG con provider
AI RAG ENSURE COLLECTION ":memory:", "products", 8
AI RAG INDEX TEXT ":memory:", "products", "prod-1", "Notebook i7"
PRINT AI RAG SEARCH ":memory:", "products", "notebook", 3


## 8. Backend e note

## Riferimenti

<!-- END SURFACE -->



<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **GREP.COUNT(x1, x2)** — `PRINT "txt  = "; GREP.COUNT("needle", fx + "/text.txt")` (shared/samples/Grep/GrepFormats.bas:10)
- **GREP.FILES(x1, x2)** — `f = GREP.FILES("needle", fx + "/outer.zip")` (shared/samples/Grep/GrepFormats.bas:18)
- **GREP.MATCHES(x1, x2)** — `hit = GREP.MATCHES("needle_inner", fx + "/outer.zip")` (shared/samples/Grep/GrepFormats.bas:22)
- **KDTREE.CREATE(x1, x2)** — `h = KDTREE.CREATE(flat, 2)` (shared/samples/Math/KdTreeAdvanced.bas:23)
- **KDTREE.QUERY(x1, x2, x3)** — `DIM nn() = KDTREE.QUERY(h, q1, 1)` (shared/samples/Math/KdTreeAdvanced.bas:27)
- **KDTREE.QUERYBALL(x1, x2, x3)** — `DIM bs() = KDTREE.QUERYBALL(h, q1, 0.5)` (shared/samples/Math/KdTreeAdvanced.bas:39)
- **KDTREE.QUERYDIST(x1, x2, x3)** — `DIM kd() = KDTREE.QUERYDIST(h, q1, 3)` (shared/samples/Math/KdTreeAdvanced.bas:35)
- **MEMORYMAP.MEMORYMAP(x1)** — `OPEN filePath FOR BINARY AS #1 LEN = 1 ' Lunghezza minima, MEMORYMAP.MEMORYMAP può estenderlo` (shared/samples/Files/FilesMemoryMap.bas:77)
- **MEMORYMAP.MMAPPEEK(x1, x2)** — `PRINT "--- 4. Test MEMORYMAP.MMAPPEEK (Lettura singoli byte) ---"` (shared/samples/Files/FilesMemoryMap.bas:101)
- **MEMORYMAP.MMAPREADBYTES(x1, x2, x3)** — `PRINT "--- 6. Test MEMORYMAP.MMAPREADBYTES (Lettura blocco di byte) ---"` (shared/samples/Files/FilesMemoryMap.bas:128)
- **SPATIAL.CDIST(x1, x2, x3)** — `cdm = SPATIAL.CDIST(X, Y, "euclidean")` (shared/samples/Math/SciPyP1Parity.bas:45)
- **SPATIAL.CONVEXHULL(x1)** — `DIM hull() = SPATIAL.CONVEXHULL(pts)` (shared/samples/Math/SciPyP1Parity.bas:66)
- **SPATIAL.DISTANCEMATRIX(x1, x2)** — `dm = SPATIAL.DISTANCEMATRIX(X, Y)` (shared/samples/Math/SciPyP1Parity.bas:47)
- **SPATIAL.PDIST(x1, x2)** — `DIM pd() = SPATIAL.PDIST(X, "euclidean")` (shared/samples/Math/SciPyP1Parity.bas:43)
- **TOOL.REGISTER(x1, x2)** — `PRINT TOOL.REGISTER("", "")` (shared/minitests/tool/gen_TOOL_Register.bas:5)
- **TOOL.SEARCH(x1)** — `tb = TOOL.SEARCH("browser")` (shared/samples/cua/CuaDiagnostics.bas:34)
- **VECTOR.ANGLE2D(x1, x2, x3, x4)** — `DIM calcolatoDeg = VECTOR.Angle2d(0, 0, x, y)` (shared/samples/Math/MathCombinatorics.bas:57)
- **VECTOR.CHEBYSHEV(x1, x2)** — `PRINT "chebyshev  ="; VECTOR.CHEBYSHEV(h1, h2)` (shared/samples/Math/VectorDistanceSciPyParity.bas:11)
- **VECTOR.COSINE(x1, x2)** — `PRINT "cosine     ="; VECTOR.COSINE(h1, h2)` (shared/samples/Math/VectorDistanceSciPyParity.bas:12)
- **VECTOR.CREATE(x1, x2, x3)** — `h1 = VECTOR.CREATE(1, 2, 3)` (shared/samples/Math/VectorDistanceSciPyParity.bas:8)
- **VECTOR.DIST2D(x1, x2, x3, x4)** — `PRINT "dist2d="; ROUND(VECTOR.DIST2D(0, 0, 3, 4), 6); "|dist3d="; ROUND(VECTOR.DIST3D(0, 0, 0, 0, 0,…` (shared/minitests/math/math_all.bas:46)
- **VECTOR.DIST3D(x1, x2, x3, x4, x5, x6)** — `PRINT "dist2d="; ROUND(VECTOR.DIST2D(0, 0, 3, 4), 6); "|dist3d="; ROUND(VECTOR.DIST3D(0, 0, 0, 0, 0,…` (shared/minitests/math/math_all.bas:46)
- **VECTOR.EUCLIDEA(x1)** — `DIM eu = VECTOR.EUCLIDEA(v)         : PRINT "euclidea   ok"` (shared/samples/Math/VectorMiniTest.bas:14)
- **VECTOR.EUCLIDEANDISTANCESQUARED(x1, x2)** — `PRINT "euclid: "; VECTOR.EuclideanDistanceSquared(x, y)` (shared/samples/Analysis/AnalysisInterpReal.bas:16)
- **VECTOR.MANHATTAN(x1, x2)** — `PRINT "manhattan  ="; VECTOR.MANHATTAN(h1, h2)` (shared/samples/Math/VectorDistanceSciPyParity.bas:10)
- **VECTOR.MINKOWSKI(x1, x2, x3)** — `PRINT "minkowski  ="; VECTOR.MINKOWSKI(h1, h2, 3)` (shared/samples/Math/VectorDistanceSciPyParity.bas:13)
- **VECTOR.POLAR_X(x1, x2)** — `DIM x = VECTOR.POLAR_X(raggio, angoloDeg)` (shared/samples/Math/MathCombinatorics.bas:49)
- **VECTOR.POLAR_Y(x1, x2)** — `DIM y = VECTOR.POLAR_Y(raggio, angoloDeg)` (shared/samples/Math/MathCombinatorics.bas:50)
- **VECTOR.PRODUCT(x1, x2)** — `DIM dp = VECTOR.PRODUCT(h1, h2)     : PRINT "dot        ok"` (shared/samples/Math/VectorMiniTest.bas:12)
- **VECTOR.VADD(x1, x2)** — `DIM s = VECTOR.VADD(a, b)` (shared/minitests/stats/vector_ops.bas:6)
- **VECTOR.VANGLE(x1, x2)** — `PRINT "ang="; MATH.ROUND(VECTOR.VANGLE(VECTOR.CREATE(1, 0, 0), VECTOR.CREATE(0, 1, 0)), 6)` (shared/minitests/stats/vector_ops.bas:18)
- **VECTOR.VCROSS(x1, x2)** — `DIM c = VECTOR.VCROSS(VECTOR.CREATE(1, 0, 0), VECTOR.CREATE(0, 1, 0))` (shared/minitests/stats/vector_ops.bas:16)
- **VECTOR.VDIST(x1, x2)** — `PRINT "dist="; MATH.ROUND(VECTOR.VDIST(VECTOR.CREATE(1, 1, 1), VECTOR.CREATE(1, 1, 4)), 6)` (shared/minitests/stats/vector_ops.bas:19)
- **VECTOR.VDIV(x1, x2)** — nessun uso nel corpus
- **VECTOR.VGET(x1, x2)** — `PRINT "add="; VECTOR.VGET(s, 0); VECTOR.VGET(s, 1); VECTOR.VGET(s, 2)` (shared/minitests/stats/vector_ops.bas:7)
- **VECTOR.VLERP(x1, x2, x3)** — `DIM l = VECTOR.VLERP(VECTOR.CREATE(0, 0, 0), VECTOR.CREATE(10, 10, 10), 0.25)` (shared/minitests/stats/vector_ops.bas:20)
- **VECTOR.VMAX(x1, x2)** — nessun uso nel corpus
- **VECTOR.VMIN(x1, x2)** — nessun uso nel corpus
- **VECTOR.VMUL(x1, x2)** — `DIM m = VECTOR.VMUL(a, b)` (shared/minitests/stats/vector_ops.bas:10)
- **VECTOR.VNORM(x1)** — `DIM n = VECTOR.VNORM(VECTOR.CREATE(3, 4, 0))` (shared/minitests/stats/vector_ops.bas:14)
- **VECTOR.VSCALE(x1, x2)** — `DIM q = VECTOR.VSCALE(a, 2.0)` (shared/minitests/stats/vector_ops.bas:12)
- **VECTOR.VSUB(x1, x2)** — `DIM d = VECTOR.VSUB(b, a)` (shared/minitests/stats/vector_ops.bas:8)
- **VECTORDB.ABTEST(x1, x2)** — `DIM ab$ = VECTORDB.ABTEST(va$, vb$)` (shared/samples/VectorDB/RagEvalAdvanced.bas:88)
- **VECTORDB.BASELINE(x1)** — `DIM bl$ = VECTORDB.BASELINE(sample$)` (shared/samples/VectorDB/RagEvalAdvanced.bas:63)
- **VECTORDB.CALIBRATE(x1)** — `DIM cal$ = VECTORDB.CALIBRATE(cases$)` (shared/samples/VectorDB/RagEvalAdvanced.bas:41)
- **VECTORDB.COMPARE(x1)** — `DIM cmp$ = VECTORDB.COMPARE(cfg$)` (shared/samples/VectorDB/RagEvalAdvanced.bas:94)
- **VECTORDB.COMPLETENESS(x1, x2)** — `PRINT "buona completeness= " + STR(MATH.ROUND(VECTORDB.COMPLETENESS("che cosa gestisce il motore", b…` (shared/samples/VectorDB/RagEvalMetrics.bas:42)
- **VECTORDB.COUNT(x1, x2)** — `PRINT "knowledge count: "; VECTORDB.COUNT("AdminDb", "knowledge")` (shared/samples/Databases/VectorDbAdmin.bas:41)
- **VECTORDB.DIAGNOSE(x1)** — `DIM diag1$ = VECTORDB.DIAGNOSE(eval1$)` (shared/samples/VectorDB/RagEvalAdvanced.bas:16)
- **VECTORDB.DIAGNOSEREPORT()** — `DIM report$ = VECTORDB.DIAGNOSEREPORT()` (shared/samples/VectorDB/RagEvalAdvanced.bas:33)
- **VECTORDB.DRIFT(x1, x2)** — `DIM psi = VECTORDB.DRIFT(b2$, w2$)` (shared/samples/VectorDB/RagEvalAdvanced.bas:74)
- **VECTORDB.EMBED(x1)** — `DIM e = VECTORDB.EMBED("memoria gerarchica", 8)` (shared/minitests/vectordb/vectordb_all.bas:333)
- **VECTORDB.ENSURECOLLECTION(x1, x2)** — `VECTORDB.ENSURECOLLECTION(":memory:", "gentest_docs", 8)` (shared/samples/VectorDB/RagEvalE2eGentest.bas:27)
- **VECTORDB.EVALDIFF(x1, x2)** — `DIM diff$ = VECTORDB.EVALDIFF(base$, curr$)` (shared/samples/VectorDB/RagEvalAdvanced.bas:56)
- **VECTORDB.EVALGATE(x1, x2)** — `DIM gate$ = VECTORDB.EVALGATE(base$, curr$)` (shared/samples/VectorDB/RagEvalAdvanced.bas:50)
- **VECTORDB.FAITHFULNESS(x1, x2)** — `PRINT "buona faithfulness= " + STR(MATH.ROUND(VECTORDB.FAITHFULNESS(ctx$, buona$), 4))` (shared/samples/VectorDB/RagEvalMetrics.bas:40)
- **VECTORDB.GENTEST(x1, x2, x3)** — `DIM result$ = VECTORDB.GENTEST("gentest_docs", 3, "simple")` (shared/samples/VectorDB/RagEvalE2eGentest.bas:44)
- **VECTORDB.HALLUCINATION(x1, x2)** — `IF VECTORDB.HALLUCINATION(ctx$, buona$) = 0 THEN` (shared/samples/VectorDB/RagEvalMetrics.bas:43)
- **VECTORDB.INDEXMETA(x1, x2, x3, x4)** — `VECTORDB.INDEXMETA("work/vsq8fp.db", "docs", "rete", "la rete neurale apprende dai dati", "{""tema""…` (shared/samples/VectorDB/VectordbSq8.bas:16)
- **VECTORDB.INDEXTEXT(x1, x2, x3, x4)** — `VECTORDB.INDEXTEXT(":memory:", "gentest_docs", "d1", "Rome is the capital city of Italy. It was foun…` (shared/samples/VectorDB/RagEvalE2eGentest.bas:29)
- **VECTORDB.LISTCOLLECTIONS(x1)** — `DIM colls$ = VECTORDB.LISTCOLLECTIONS("AdminDb")` (shared/samples/Databases/VectorDbAdmin.bas:26)
- **VECTORDB.LOADDOCUMENTS(x1, x2, x3)** — `PRINT "load>=-1="; (VECTORDB.LOADDOCUMENTS("work/vdb_all.db", "cdocs", "work/docs", "*.txt") >= -1)` (shared/minitests/vectordb/vectordb_all.bas:344)
- **VECTORDB.MRR(x1, x2)** — `PRINT "q1 mrr= " + STR(MATH.ROUND(VECTORDB.MRR(r1$, i1$), 4))` (shared/samples/VectorDB/RagEvalMetrics.bas:25)
- **VECTORDB.NDCGAT(x1, x2, x3)** — `PRINT "q1 ndcg@2= " + STR(MATH.ROUND(VECTORDB.NDCGAT(r1$, i1$, 2), 4))` (shared/samples/VectorDB/RagEvalMetrics.bas:26)
- **VECTORDB.PRECISIONAT(x1, x2, x3)** — `PRINT "q1 p@2= " + STR(MATH.ROUND(VECTORDB.PRECISIONAT(r1$, i1$, 2), 4))` (shared/samples/VectorDB/RagEvalMetrics.bas:23)
- **VECTORDB.PURGEDEPRECATED(x1, x2)** — `PRINT "=== VECTORDB.PURGEDEPRECATED ==="` (shared/samples/VectorDB/VectordbPurgeDeprecated.bas:10)
- **VECTORDB.RECALLAT(x1, x2, x3)** — `PRINT "q1 r@2= " + STR(MATH.ROUND(VECTORDB.RECALLAT(r1$, i1$, 2), 4))` (shared/samples/VectorDB/RagEvalMetrics.bas:24)
- **VECTORDB.RELEVANCE(x1, x2)** — `PRINT "buona relevance= " + STR(MATH.ROUND(VECTORDB.RELEVANCE("che cosa gestisce il motore", buona$)…` (shared/samples/VectorDB/RagEvalMetrics.bas:41)
- **VECTORDB.REMOVESOURCE(x1, x2, x3)** — `VECTORDB.REMOVESOURCE("work/vsq8fp.db", "docs", "rete")` (shared/samples/VectorDB/VectordbSq8.bas:35)
- **VECTORDB.SEARCH(x1, x2, x3, x4)** — `DIM ris2$ = VECTORDB.SEARCH("AdminDb", "knowledge", "retrieval e generazione", 2)` (shared/samples/Databases/VectorDbAdmin.bas:59)
<!-- END DOMAIN TABLE -->
