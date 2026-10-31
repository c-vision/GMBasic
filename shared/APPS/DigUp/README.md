# DigUp (GMBasic port) — ricerca semantica nei file

**DigUp** trova i file del computer **da ciò che dicono o mostrano**, non dal nome: si descrive
quello che si cerca e l'app ranka i passaggi più vicini. Questo è il **port GMBasic** della sua
**ossatura headless**, scritto in BASIC e compilabile sui backend del monorepo.

## Upstream (sorgente del port)

| Cosa | Riferimento |
|---|---|
| Repository | <https://github.com/ARahim3/DigUp> |
| Licenza | **MIT** (obbligo di attribuzione: vedi sotto) |
| Modello dell'originale | **EmbeddingGemma 2** (Google DeepMind, 768 dim, multimodale) via llama.cpp |
| Architettura | cartelle → reader → embedding → **un indice** (vettori + testo) → query → ranking |

Attribuzione (MIT): DigUp © ARahim3 (Abdur Rahim), <https://github.com/ARahim3/DigUp>. Codice
originale MIT; questo port riprende **l'architettura e il comportamento osservabile**, non il
codice Swift/Tauri.

## Cosa fa questo port (M1 — core headless)

```
cartelle configurate ─► lettura dei file di testo ─► chunk ─► embedding ─► SQLite
                                                                       │  (testo + vettore)
query in parole ─────► embedding della query ─► coseno + boost parole esatte ─► top-k
```

- **Reader**: file di testo/documenti/codice per estensione (`[index] extensions`).
- **Chunking**: finestre di `[index] chunk` caratteri.
- **Embedding**: `AI.EMBED` della capability `%EMBEDDING`, con il **modello configurato nei
  settings della app** (`.digup.toml`, `[llm]`) — mai hardcoded (vedi ADR nel vault).
- **Indice**: un file SQLite (`bin/digup.sqlite`) con `path`, `seq`, `text`, `vec` (CSV).
- **Ranking**: delegato a `AI.SEARCH(..., hybrid=1)` — similarità di embedding **+** parole esatte
  (il boost sui numeri di fattura / codici d'errore lo fa il motore, non l'app); il `sourceId`
  (`path#seq`) lega ogni hit al file e al passaggio, che l'app risolve nella tabella `chunks`.

### Modi

| Modo | Come si attiva | Cosa fa |
|---|---|---|
| `status` (default) | — | banner, config, presenza indice, numero chunk |
| `index` | `#DEF DIGUP_MODE_INDEX` in testa al `.bas` | indicizza le `roots` (ricorsivo) |
| `search` | `#DEF DIGUP_MODE_SEARCH` | cerca `[search] query` e stampa il top-k |
| `check` | `#DEF DIGUP_MODE_CHECK` | provider di embedding attivo, dims, indice |

> BASIC non ha argv: il modo si sceglie con `#IFDEF` (i runner di `tools/runners/` non passano
> `-D`) o con `[settings] mode` nel `.digup.toml`.

## Configurazione (`.digup.toml`, accanto alla app o in una cartella superiore)

```toml
[settings]
mode = "status"

[index]
roots = "fixtures"                     # cartelle da indicizzare (relative alla app, csv)
db = "bin/digup.sqlite"                # indice (output, gitignored)
extensions = ".txt,.md,.csv,.bas,.json,.toml"
chunk = 300
reindex = "true"

[search]
top = 5
query = "zebra nella savana"

[llm]
model = "fixture-embedding"            # alias/provider %EMBEDDING (default deterministico)
capability = "embedding"
dir = ""                               # se valorizzato: AI REGISTER + attivazione (modello locale)
dims = 8                               # fixture=8; modelli reali 384/768/1024
```

- `[llm] dir` valorizzato → la app registra il modello locale (`AI REGISTER "digup-embed",
  %EMBEDDING, dir`) e lo attiva: si usa un modello supportato da LLMEnv, **non** per forza quello
  dell'originale (EmbeddingGemma 2 non è richiesto).
- `[llm] model` vuoto → resta il provider già attivo.
- Alias non registrato → **errore esplicito**, nessun fallback silenzioso.

## Come eseguire

Dalla **root del monorepo** (`GMBasicMonoRepo/`):

```sh
# status (default)
tools/sh/harness.sh pcode shared/APPS/DigUp/src/DigUp.bas
tools/sh/harness.sh il    shared/APPS/DigUp/src/DigUp.bas

# index / search: aggiungere in testa al .bas  #DEF DIGUP_MODE_INDEX  (o _SEARCH)
tools/sh/harness.sh pcode shared/APPS/DigUp/src/DigUp.bas

# eseguibile nativo (release)
dotnet run --project GMBasicSharp/REPL -- -n shared/APPS/DigUp/src/DigUp.bas

# C99Native (frontend C + emitter → cc → esecuzione; percorso relativo a GMBasic/)
bash tools/sh/gmb_c99native_run.sh ../shared/APPS/DigUp/src/DigUp.bas
```

L'indice e ogni output stanno in **`bin/`** (default del compilatore, gitignored): la app **non**
usa `work/` (riservata ai sample, vedi `AGENTS.md` §11.1).

## Fixtures (input versionati)

`fixtures/` contiene tre testi brevi usati dal demo e dalla verifica: `zebra.txt` (un branco di
zebre), `lease.md` (contratto: clausola animali), `podcast.txt` (puntata sul sonno). Servono a
mostrare il ranking senza dipendere da modelli o file dell'utente.

## Verifica registrata (2026-11-01, 4 gambe + fixture embedding)

> [!note] **Stato per gamba**: `status`, `index` (3 file / 3 chunk) e `search` (top-1
> `fixtures/zebra.txt` 0.333) verificati su **IL, PCode, VM/C e C99Native**. Due limiti del backend
> IL sono aggirati nel codice e tracciati nel vault: lettura righe con `IO.READFILE`+`STRINGS.SPLIT`
> (invece di `IO.READLINES`+`UBOUND`) e condizione `IF isDir <> 0` (invece di `IF isDir`), più il
> path del DB passato come parametro a `OpenIndex`. Dettaglio:
> `Bugs/(open) - App-DigUp-Trappole-Tipizzazione-JSON-Array` e
> `Bugs/(open) - IL-IF-su-Variabile-BIT-e-SQL-OPEN-con-Globale`.

Il search usa **AI.SEARCH** (ibrida: embedding + parole esatte) sulla collezione AI RAG; i
punteggi sono quelli del motore (con il provider fixture: 0.33 / 0.75 / 1.0 nelle tre prove).


| passo | esito |
|---|---|
| `status` | config letta da `.digup.toml`, indice assente → `chunks : 0` |
| `index` | `files indexed : 3`, `chunks : 3` |
| `search` «zebra nella savana» | 1° `fixtures/zebra.txt#0` (0.333) |
| `search` «clausola animali domestici locazione» | 1° `fixtures/lease.md#0` (0.75) |
| `search` «sonno REM sogni» | 1° `fixtures/podcast.txt#0` (1.0) |

> Con il provider **fixture** il vettore è deterministico ma **non semantico**: il ranking regge
> grazie al boost sulle parole esatte. Per la ricerca «per significato» serve un modello di
> embedding reale in `[llm] dir` (supportato da LLMEnv) — vedi `TODO.md` §M2.

## Note di implementazione (per chi riprende)

- **`STRINGS.SPLIT` non è usata**: con `IMPORT AI` restituisce elementi che sono **array**
  (`System.String[]`) invece di stringhe → il parsing dei CSV/estensioni è fatto a mano
  (`SplitParts`, `MID$`/`TRIM`) su un buffer globale.
- `IO.DIR` restituisce una **LIST** (pattern `FOR EACH`); la ricorsione usa `WalkDir`.
- Il testo di un chunk è salvato come stringa con apici raddoppiati (`SqlQuote`); niente
  parametri `?` multipli (non usati altrove nel repo).
- Con l'embedding fixture la dimensione è 8 (`[llm] dims`); con un modello reale va allineata.

## Licenza di questo port

Codice del port: licenza del progetto GMBasic. **Attribuzione obbligatoria** all'originale DigUp
(MIT) come sopra. Il modello di embedding usato dipende dalla configurazione dell'utente e mantiene
la propria licenza (LLMEnv/LLMEngine per l'inferenza).
