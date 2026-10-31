# TODO — RuleFlow (GMBasic port)

Stato: **M1** — core headless funzionante: parser FlowLang (sottoinsieme), universo di celle
con id/origine, eventi con delta, **grafo causale** e metriche (nodi, archi, densità,
cammino massimo). Verificato su IL/PCode/VM-C/C99Native.
Upstream: <https://github.com/RuleFlow-OSS/RuleFlow> (MIT).

## M2 — fedeltà al motore FlowLang

- [ ] **Parallelismo per regola** (`-pl[n]`): oggi una regola si applica **una volta per
      tick**. Con `-pl[inf]` (default upstream del README) si applicano tutte le occorrenze
      non sovrapposte. Richiede: ricerca di tutti i match, gestione dei conflitti spaziali,
      eventi multipli nello stesso tick.
- [ ] **Branch multiway** (`-bl[n]`): quando più regole/matches competono, generare universi
      alternativi e conservare la storia dei rami (`parent_delta`), con merge/esclusione.
- [ ] **Protocolli di conflitto** (`-cmp`, `-crp`): marcatura/skip/branch sugli span sovrapposti.
- [ ] **Probabilità stocastiche** (`-p_rule`, `-p_space`) con PRNG deterministico (seed nei
      settings, così i run restano riproducibili).
- [ ] **`@regress(n)`**: richiede la storia completa delle mutazioni (oggi si conservano solo
      gli eventi, non lo stato precedente). Con M2 il grafo diventa reversibile.
- [ ] **`@merge` / `@compress` / `@macro`**: composizione atomica di regole e import di flow
      esterni.
- [ ] **Vincoli per istruzione** (`-life[n]`, `-sr[...]`, `-g[...]`): contatori di vita e
      range spaziali per regola.

## M3 — bootstrap e analisi

- [ ] **Bootstrap `.pflow` (Python) / `.wpflow` (Wolfram)**: nel mondo BASIC il ruolo del
      bootstrap è di un *generatore di regole*; candidati: macro `#DEF`, `INCLUDE`, oppure un
      piccolo linguaggio di template. Decidere l'equivalente (mai eseguire Python/Wolfram).
- [ ] **Import/export del grafo**: GEXF (Gephi), GraphML, GML, Graph6/Sparse6 — il formato
      atteso dal mondo NetworkX. Il DAG è già in memoria: serve il writer.
- [ ] **Topologie** (upstream `core/topologies`): oggi l'universo è 1D (stringa). Aggiungere
      2D/ND e topologie toroidali; `@init` da file `.npy`/binario.
- [ ] **Ricerca di pattern prestazionale** (Boyer-Moore-Horspool/KMP/Rabin-Karp come
      upstream): oggi `INSTR`; utile per universi grandi.
- [ ] **Metriche aggiuntive**: degree assortativity, flow hierarchy, densità per generazione,
      distribuzione delle lunghezze dei cammini.
- [ ] **Enumerazione di sistemi** (Studio "System Enumeration"): valutare interi spazi di
      regole con proprietà (identità, shortening, sbilanciamento) — con budget di celle/eventi.

## M4 — esperienza utente

- [ ] **TUI** (il monorepo ha `MENU`/`TBROWSE`): workspace, step/regress, hover sulle celle
      (quanta/generazione/id), grafo con sparkline. Oggi: 4 modi CLI.
- [ ] **Snapshot/restore** di una sessione (universo + eventi) per riprendere un esperimento.
- [ ] **Varianti di workspace** (upstream "Workspace Variants"): copie temporanee del flow.

## Vincoli (non negoziabili)

- **Nessuno stub**: ogni capacità o funziona o restituisce un **errore esplicito** documentato
  (oggi: flag, direttive e bootstrap non-M1 falliscono con messaggio).
- **Determinismo**: nessuna casualità non seminata; due run = stesso risultato.
- **Parità 4 gambe** della superficie BASIC usata (AGENTS §4.1): `check` identico su
  IL/PCode/VM-C/C99Native.
- **Licenza**: attribuzione MIT di RuleFlow nel `README.md`; nessun codice Python copiato.
