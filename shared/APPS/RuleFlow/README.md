# RuleFlow (GMBasic port) — sistemi discreti con grafo causale

**RuleFlow** modella ed evolve sistemi discreti complessi (Sequential Substitution
Systems, automa cellulare, riscritture di stringhe) tenendo traccia **non** del solo stato
finale, ma di **ogni evento** che lo ha prodotto: quali celle sono state distrutte, quali
create, e da quale evento discendono. Da lì si estrae un **DAG causale** con le sue metriche
(nodi, archi, densità, cammino massimo).

Questo è il **port GMBasic** dell'**ossatura headless**, scritto in BASIC e verificato sui
quattro backend del monorepo.

## Upstream (sorgente del port)

| Cosa | Riferimento |
|---|---|
| Repository | <https://github.com/RuleFlow-OSS/RuleFlow> |
| Sito | <https://www.ruleflow.org/> |
| Licenza | **MIT** (obbligo di attribuzione: vedi sotto) |
| Natura | framework Python di ricerca + IDE terminale (*RuleFlow Studio*, Textual) |
| Core | `Cell`/`DeltaCell`/`Event`/`Flow`/`RuleSet`, topologie vettoriali, DSL **FlowLang** |
| Analisi | `EventCausalityGraph` → NetworkX `MultiDiGraph` + metriche |

Attribuzione (MIT): RuleFlow © RuleFlow-OSS (Southern Adventist University research team),
<https://github.com/RuleFlow-OSS/RuleFlow>. Il codice originale è MIT; questo port riprende
**l'architettura e il comportamento osservabile**, non il codice Python.

## Cosa fa questo port (M1 — core headless)

```
script .flow ──► parser FlowLang (M1) ──► universo di CELLE (valore + id + origine)
                                             │
                         per ogni tick: applica le regole
                                             │
                    una applicazione = un EVENTO con il suo DELTA
                    (celle distrutte / create, ognuna con la sua origine)
                                             │
                     archi causali: origine(cella toccata) ──► evento
                                             │
                    metriche del DAG: nodi, archi, densità, cammino massimo
```

- **Celle**: universo come sequenza di simboli; ogni cella ha **id univoco** e **origine**
  (l'evento che l'ha creata; `0` = generazione zero).
- **Eventi**: una applicazione di regola registra tick, regola, posizione e il delta
  (le celle toccate/prima e dopo).
- **Causa**: un evento è collegato alle **origini delle celle che tocca** (per la
  cancellazione: le celle distrutte; per l'inserimento: le celle di contesto).
  Poiché le origini sono sempre eventi **precedenti**, il grafo è un DAG e l'ordine degli
  id degli eventi è un ordine topologico.
- **Metriche** (parità con `EventCausalityGraph` upstream):

| metrica | definizione in questo port |
|---|---|
| `nodi` | numero di eventi **+ 1** (nodo radice = generazione zero) |
| `archi` | archi causali distinti `(origine → evento)` |
| `densità` | `archi / (nodi * (nodi - 1))` |
| `cammino max` | lunghezza del cammino massimo nel DAG (DP in ordine di evento) |

### Modi

| Modo | Come si attiva | Cosa fa |
|---|---|---|
| `status` (default) | — | banner, config, presenza del flow, limiti |
| `run` | `#DEF RULEFLOW_MODE_RUN` in testa al `.bas` | evolvi, stampa regole, contatori, spazio finale, traccia eventi |
| `graph` | `#DEF RULEFLOW_MODE_GRAPH` | evolvi e stampa le metriche del grafo causale |
| `check` | `#DEF RULEFLOW_MODE_CHECK` | diagnostica: regole, evoluzione, DAG, metriche, spazio (`RULEFLOW-CHECK-OK`) |

> BASIC non ha argv: il modo si sceglie con `#IFDEF` (i runner di `tools/runners/` non
> passano `-D`) o con `[settings] mode` nel `.ruleflow.toml`.

## FlowLang supportato (M1)

| operatore | significato | esempio |
|---|---|---|
| `pat -> dst` | **substitution**: il selettore è rimpiazzato dal target | `ABA -> AAB;` |
| `pat --> dst` | **overwrite**: sovrascrive le celle del selettore (stessa lunghezza) | `B --> Z;` |
| `pat > dst` | **insertion**: inserisce `dst` **prima** del selettore | `Y > W;` |
| `pat ><` | **deletion**: rimuove il selettore | `W ><;` |

Direttive: `@init("TEST0")` (universo iniziale), `@evolve(n)` (tick; **vince** su
`[flow] steps`). Commenti: `#`. Case-insensitive (i simboli sono normalizzati a maiuscolo).

**Semantica M1 (deterministica):** in ogni tick, ogni regola si applica **una volta**, al
primo match da sinistra (dopo un'applicazione la ricerca riparte oltre il target).
Nessuna casualità, nessun branch: due run danno lo stesso risultato.

**NON supportato in M1 — errore esplicito, mai ignorato:**

- flag FlowLang `-pl`, `-bl`, `-cmp`, `-crp`, `-p_rule`, `-p_space`, `-life`, `-sr`
  (parallelismo, branch multiway, protocolli di conflitto);
- direttive `@regress`, `@merge`, `@compress`, `@macro`;
- bootstrap Python/Wolfram (`.pflow`/`.wpflow`);
- reader `.npy`/binario per `@init`.

Un flow che usa queste forme **fallisce** con un messaggio che indica la forma e il TODO.

## Configurazione (`.ruleflow.toml`, accanto alla app o in una cartella superiore)

```toml
[settings]
mode = "status"          # status | run | graph | check

[flow]
file = "flows/sss.flow"  # script FlowLang-like, relativo alla cartella della app
steps = 12               # tick se il flow NON dichiara @evolve(n)

[run]
verbose = "true"         # "nessun regola applicabile" a fine evoluzione
```

## Fixtures (input versionati)

| flow | cosa mostra |
|---|---|
| `flows/sss.flow` | il **Sequential Substitution System del README upstream** (`ABA -> AAB; A -> ABA;`, 12 tick → 24 eventi, 27 celle) |
| `flows/lsystem.flow` | L-system di Lindenmayer in semantica sequenziale (cresce di 1 cella/tick) |
| `flows/ops.flow` | **tutti e quattro** gli operatori + il churn inserimento/cancellazione |

## Come eseguire

Dalla **root del monorepo** (`GMBasicMonoRepo/`):

```sh
# status (default)
tools/sh/harness.sh pcode shared/APPS/RuleFlow/src/RuleFlow.bas
tools/sh/harness.sh il    shared/APPS/RuleFlow/src/RuleFlow.bas

# run / graph / check: aggiungere in testa al .bas  #DEF RULEFLOW_MODE_RUN (o _GRAPH/_CHECK)

# gamba nativa (C99Native), percorso relativo a GMBasic/
bash tools/sh/gmb_c99native_run.sh "$PWD/shared/APPS/RuleFlow/src/RuleFlow.bas"
```

Nessun output su disco: la app è pura computazione in memoria (`bin/` viene solo creata,
come da convenzione del compilatore).

## Verifica registrata (2026-11-01, 4 gambe)

Flow `flows/sss.flow` (2 regole, `@evolve(12)`):

| passo | esito |
|---|---|
| `status` | config letta, flow presente, limiti |
| `run` | 24 eventi / 24 applicazioni / 24 archi; spazio finale `ABAAB…AB` (27 celle) |
| `graph` | nodi **25**, archi **24**, densità **0.0400**, cammino max **24** |
| `check` | `check-rules/evolve/dag/metric/space` OK + `RULEFLOW-CHECK-OK` |

Output **identico** su IL, PCode, VM/C e C99Native (verificato con `diff` sulle righe
`flow/eventi/nodi/archi/densità/cammino/spazio finale`).
