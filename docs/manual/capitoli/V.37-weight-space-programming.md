# V.37 — Weight-Space Programming: leggere e modificare i pesi

## Cosa ottieni da questo capitolo

Aprire un modello **senza eseguirlo**, vederne l'intera struttura come un albero indirizzabile, analizzare i circuiti di
attenzione, modificare i pesi su una copia protetta, derivare un modello diverso da uno esistente e riscrivere il risultato con
una prova di cosa è cambiato. Tutto con il namespace `WSP`, che usa la libreria nativa `gmbwsp`.

Il capitolo dice anche cosa **non** è dimostrato: modificare i pesi cambia un modello, non lo migliora per definizione. Ogni
derivato va misurato.

## 1. Il modello come albero

```basic
IMPORT WSP

m = WSP.LOADMODEL("~/ai/Models/Qwen3-8B")
PRINT WSP.MODELTYPE(m), WSP.DIM(m, "layers"), WSP.TENSORCOUNT(m), WSP.CHECKDOC(m)
```

`WSP.LOADMODEL` apre una directory di pesi `safetensors` (anche divisi in più file) e costruisce il **documento**: l'elenco completo
dei tensori organizzati in embedding, layer (norme, attenzione, MLP), norma finale e testa di uscita. `WSP.CHECKDOC` verifica
l'invariante che rende il documento affidabile: **ogni byte del modello appartiene a una foglia, una sola volta**. Zero violazioni
significa che non c'è nulla di non sezionato.

Le famiglie coperte sono Qwen3 e Llama a pesi densi. Per le altre `WSP.HASDOC(m)` vale 0: si possono leggere e modificare i tensori
con indirizzi espliciti (sotto), ma non usare l'analisi per testa né la derivazione.

### Indirizzi

| Forma | Esempio | Cosa indica |
|---|---|---|
| foglia | `layers.5.attn.o` | un tensore del documento |
| partizione | `layers.5.attn.o.heads.7` | la slice di una testa |
| neurone | `layers.5.mlp.down.neurons.100` | una colonna/riga dell'MLP |
| globale | `embed`, `final_norm`, `lm_head` | tensori fuori dai layer |
| esplicito | `@1:896:1024:model.layers.5.self_attn.o_proj.weight` | asse, intervallo, nome del tensore (vale per ogni famiglia) |

`WSP.LEAFADDR(m, i)` elenca gli indirizzi; `WSP.READSLICE(m, addr, v(), cap)` legge i valori correnti come `DOUBLE`. Una slice di
un tensore enorme si legge **senza caricare il tensore intero**.

## 2. Analisi senza eseguire il modello

```basic
DIM s(1) AS DOUBLE
r = WSP.COPYSCORE(m, 5, 7, "OV", s)      ' testa 7 del layer 5
PRINT s(0), s(1)
PRINT WSP.COMPOSE(m, 4, 2, 5, 7, "k")
```

| Funzione | Risultato |
|---|---|
| `WSP.NORMS(m, layer, "q"/"k"/"v"/"o", out(), cap)` | norma di ogni testa: la **baseline ovvia** con cui confrontare tutto il resto |
| `WSP.COPYSCORE(m, L, H, "OV"/"QK", out())` | `out(0) = Σλ/Σ|λ|`, `out(1) = frazione di autovalori positivi`: le **due** definizioni di *copying score* |
| `WSP.SPECTRUM(...)` | gli autovalori del circuito della testa |
| `WSP.COMPOSE(m, l1, h1, l2, h2, "q"/"k"/"v")` | quanto lo spazio scritto da una testa è letto da un'altra (Projection Kernel, da 0 a `head_dim`) |
| `WSP.MLPIMPORTANCE`, `WSP.KVIMPORTANCE` | importanza di neuroni e gruppi kv calcolata **solo dai pesi** |

Due avvertenze che valgono per ogni uso:

- Il circuito QK è quello "nudo": **RoPE e le norme su q/k non sono incluse**. Per i modelli che le usano (Qwen3) lo spettro QK è
  un'approssimazione.
- L'importanza calcolata dai soli pesi è una baseline **debole**: su Qwen3-8B togliere il 5% dei neuroni scelti così ha quadruplicato
  la perplessità. Serve come punto di partenza da confrontare con misure vere, non come criterio di potatura.

## 3. Modificare i pesi senza rovinare l'originale

Ogni modifica avviene su un **overlay**: una copia dei soli tensori toccati. La base resta identica fino a quando non scrivi.

```basic
r = WSP.CHECKPOINT(m, "prima")
r = WSP.SCALESLICE(m, "layers.5.attn.o.heads.7", 0.5)
PRINT WSP.LASTREPORT(m)           ' toccati=524288/524288 rel_norm=0.5 max_abs=...
r = WSP.ROLLBACK(m, "prima")
```

| Funzione | Cosa fa |
|---|---|
| `SCALESLICE`, `ZEROSLICE` | moltiplica o azzera una slice |
| `SETVALUES`, `ADDDELTA` | sostituisce o somma valori (`n` elementi) |
| `COPYSLICE(dst, src)` | copia una slice su un'altra di forma uguale |
| `PROJECTOUT(addr, dirs(), ndirs, len, asse, flags)` | toglie dalla slice le componenti lungo le direzioni date; `flags`: 1 = conserva le norme, 2 = direzione "innocua" da non toccare |
| `UNDOEDIT`, `REDOEDIT`, `CHECKPOINT`, `ROLLBACK` | navigano il journal |
| `DIFFCOUNT`, `SUMMARY` | cosa differisce dalla base e di quanto |

**Invarianti.** Una modifica con valori non finiti, di dimensione sbagliata, su un indirizzo che non esiste o su un tipo non
modificabile (pesi quantizzati) viene **rifiutata** e nulla cambia. Ogni modifica riporta quanti pesi ha toccato, la variazione
relativa di norma e il massimo scarto.

Il valore di `asse` in `PROJECTOUT` dice quali vettori proiettare: `0` se i vettori sono le **colonne** (le matrici che scrivono nel
residuo: `o_proj`, `down_proj`), `1` se sono le **righe** (embedding e le matrici che leggono).

### Scrivere il risultato

```basic
r = WSP.MATERIALIZE(m, "~/out/model-edit")
PRINT WSP.VERIFYMODEL("~/out/model-edit", 3)       ' 0 = ok
```

`MATERIALIZE` clona i file non toccati e riscrive **solo il file** che contiene un tensore modificato; accanto ai pesi scrive
`wsp_summary.txt` con l'elenco di cosa è cambiato: nessuna scrittura silenziosa. `VERIFYMODEL` riapre il risultato e controlla
documento (bit 1) e finitezza dei valori (bit 2).

Il journal è anche un **programma**: `WSP.SAVERECIPE(m, dir)` lo serializza e `WSP.REPLAYRECIPE(m2, dir)` lo riesegue su un modello
nuovo ottenendo gli stessi byte. `WSP.EXPORTPATCH` / `WSP.APPLYPATCH` producono e applicano una **patch sparsa**: solo le slice
davvero cambiate.

## 4. Derivare un modello diverso

```basic
DIM none(0) AS LONG
DIM lm(34) AS LONG
' ... lm(j) = layer sorgente del layer di uscita j: tutti tranne il penultimo ...
r = WSP.DERIVE(m, "~/out/meno-un-layer", lm, 35, none, 0, none, 0)
PRINT WSP.LASTREPORT(m)
```

`WSP.DERIVE(m, outdir, layer_map(), nl, kv_keep(), kvm, neuron_keep(), neuk)` combina tre operazioni, ognuna disattivata da un
conteggio a zero:

- **layer**: la mappa dice quale layer sorgente alimenta ogni layer di uscita; omettere un indice lo toglie, ripeterlo lo duplica,
  cambiarne l'ordine lo riordina;
- **gruppi kv**: si tolgono interi gruppi di attenzione (una testa k/v con le sue teste q), così il rapporto resta valido; lo stesso
  numero in ogni layer, scelta libera per layer;
- **neuroni MLP**: si tolgono neuroni dell'intermedia, lo stesso numero in ogni layer.

Il piano viene **validato prima di scrivere** (indici fuori intervallo, duplicati, conteggi nulli, pesi quantizzati) e il risultato
viene **rivalidato dopo**. Rinumera i tensori, riscrive la configurazione e il file di architettura, copia tokenizer e accessori.

**Cosa aspettarsi.** Un derivato è un modello nuovo e quasi sempre peggiore dell'originale finché non lo si "cura" con un breve
riaddestramento. Misure su Qwen3-8B (una sola sequenza di prova, quindi indicative):

| Derivato | Perplessità (originale 15,4) |
|---|---|
| −4 layer, blocco scelto per similarità tra rappresentazioni | 30,8 |
| −4 layer, quelli subito prima dell'ultimo | 91,6 |
| −5% neuroni (importanza dai pesi) | 61,3 |
| −25% neuroni e −2 gruppi kv su 8 | oltre 25.000 (inutilizzabile) |

Il confronto dice una cosa precisa: **come si sceglie cosa togliere conta**, e l'importanza dai soli pesi non basta. La scelta per
similarità richiede di eseguire il modello e si fa con gli strumenti descritti nella guida tecnica (`gmbllm/docs/WSP.md`).

## 5. Verifica, soglie e provenienza

| Funzione | Cosa fa |
|---|---|
| `WSP.VERIFYMODEL(dir, flags)` | riapre un checkpoint e ne controlla struttura e finitezza |
| `WSP.MODELHASH(m)` | impronta del modello: stessa impronta = stessi pesi, tensore per tensore |
| `WSP.GATE(valore, soglia, le)` | 1 se il valore rispetta la soglia **dichiarata da te** (`le`≠0: `valore ≤ soglia`); un valore non numerico fallisce |
| `WSP.PROVENANCE(m, outdir, ricetta$, path$)` | scrive la catena base → operazioni → risultato con le impronte |

Le soglie non sono implicite: chi usa `WSP.GATE` le sceglie e le riporta. Un edit "di comportamento" va accompagnato da una
misura (divergenza KL, perplessità) e rifiutato oltre la soglia dichiarata; il calcolo della misura sta fuori da `WSP`, che ne
valuta soltanto il verdetto.

## 6. Una sessione pilotabile da un agente

`WSP.AGENTNEW(m, max_edit, max_forward)` apre una sessione che accetta **comandi testuali** su dati; `WSP.AGENTEXEC(m, cmd$)`
li esegue e restituisce il testo del risultato, `WSP.AGENTRC(m)` lo stato (0 ok, 1 rifiutato, −1 errore).

```basic
r = WSP.AGENTNEW(m, 3, 0)
r = WSP.AGENTTRANSCRIPT(m, "~/out/sessione.jsonl")
o$ = WSP.AGENTEXEC(m, "SPECTRUM 1 0 ov")
o$ = WSP.AGENTEXEC(m, "SCALE layers.0.attn.o.heads.1 0.5")
o$ = WSP.AGENTEXEC(m, "SHELL ls /")          ' RIFIUTATO e registrato
r = WSP.AGENTSTOP(m)
```

I verbi ammessi sono quelli di lettura e di modifica descritti sopra (`NORMS`, `SPECTRUM`, `COMPOSE`, `SCALE`, `ZERO`, `SET`,
`ADDDELTA`, `COPY`, `PROJECTOUT`, `CHECKPOINT`, `ROLLBACK`, `UNDO`, `REDO`, `JOURNAL`, `SUMMARY`, `SHA`, `DEFINE`). Qualunque altro
verbo — shell, rete, file arbitrari — è rifiutato. I budget limitano le modifiche e i forward. La trascrizione registra ogni comando
con l'impronta del risultato; `WSP.AGENTREPLAY(m, path$)` la riesegue su una sessione nuova e dà `0` se **ogni output coincide**,
altrimenti l'indice del primo comando che diverge. `WSP.AGENTRUN` scrive il riepilogo dell'audit (budget, verbi usati, nessuno
strumento esterno).

## 7. Uso responsabile

Le primitive di modifica sono **generali**: servono per ricerca, potatura, correzioni mirate. Il namespace non include ricette né
dati per rimuovere le protezioni di sicurezza di un modello. Ogni modifica resta nel journal e, se usi la sessione, nella trascrizione.

## 8. Limiti e stato

- Solo pesi `safetensors` in `F32`, `BF16`, `F16` per le modifiche; i pesi quantizzati si leggono ma non si modificano né si potano.
- Famiglie coperte dal documento: Qwen3 e Llama densi. Non sono coperti `VOCAB.TRIM`, la potatura di esperti (MoE), la guarigione dei
  derivati e le sonde che richiedono di eseguire il modello (`RANK1`, `NULLSPACE`, probabilità di attenzione).
- Il circuito QK non include RoPE e le norme su q/k.
- Un caso verificato del paper di riferimento (togliere una direzione di *loop* da due matrici di Qwen3.5-0.8B) ha riprodotto la
  **struttura** della modifica (130.188 pesi, 0,015%) e una **riduzione delle ripetizioni**; non ha riprodotto, nel protocollo ridotto
  usato, un aumento di accuratezza.

Sample completi: `shared/samples/Wsp/Section.bas`, `Edit.bas`, `Derive.bas`, `Agent.bas`. Guida tecnica con formati e strumenti da riga
di comando: `GMBasic/gmbllm/docs/WSP.md`.
