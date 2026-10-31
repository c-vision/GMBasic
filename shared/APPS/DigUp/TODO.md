# TODO — DigUp (GMBasic port)

Stato: **M1** — core headless funzionante su testo/documenti/codice con indice SQLite e ranking
(coseno + parole esatte). Vedi `README.md`. Upstream: <https://github.com/ARahim3/DigUp> (MIT).

## M2 — Reader mancanti (per tipo di file)

- [ ] **PDF** (pagina per pagina, con il numero di pagina nel risultato). LLMEnv non ha PDFium;
      `gmb_doc_c`/`ConversionLib` coprono parte dei documenti: verificare cosa è già estraibile e
      usare quello (mai duplicare un reader che esiste).
- [ ] **Office**: docx/odt/xlsx/ods/pptx/odp — estrazione testo via `gmb_doc_c` se disponibile.
- [ ] **Immagini / screenshot** (OCR del testo) via `gmb_doc_ocr` **dove tesseract è nel bundle**
      (oggi solo osx-arm64, vedi `Todo/(open) - Build-OCR-Tesseract-per-OS` nel vault).
- [ ] **Audio/video**: richiede l'embedding multimodale dell'originale (EmbeddingGemma 2).
      LLMEnv **non** lo espone oggi: **bloccato** finché non c'è un modello multimodale
      supportato. Nel frattempo: nessun finto supporto (errore esplicito se si tenta).

## M3 — Ricerca e qualità

- [ ] **Indice incrementale**: oggi `reindex = "true"` ricostruisce tutto; aggiungere
      `mtime`/`size` per file e reindicizzare solo i cambiati (la app è "una passata" come
      l'originale: dopo, solo i nuovi).
- [ ] **Prefisso `code:`** (indice separato per il codice, come l'originale) e filtro per
      estensione/cartella nei risultati.
- [ ] **Indice di parole esatte** (FTS/`LIKE` con ranking) invece del solo boost sulle parole:
      numeri di fattura e codici d'errore con pesi confrontabili fra loro.
- [ ] **Eval deterministico** (come `evals/` dell'originale): query+file atteso, metrica
      top-1/top-k, eseguibile con il provider fixture (nessun modello) e con un modello reale.
- [ ] **Output JSON** dei risultati (per script/CI) — es. `[search] format = "json"`.

## M4 — Esperienza utente (come l'originale)

- [ ] **Hotkey globale** e **tray**: nel monorepo esistono `TRAY` e `CONTROLS`; valutare una TUI
      (`MENU`/`TBROWSE`) prima della GUI pixel.
- [ ] **Apertura al punto**: aprire PDF alla pagina, audio/video al minuto, codice alla riga
      (dipende dai reader di M2).
- [ ] **Anteprima** grande e griglia immagini.

## M1.1 — sbloccare le altre gambe (fatto 2026-11-01)

- [x] **IL**: `status`/`index`/`search` verdi. Aggiramenti applicati (bug nel vault):
      `IO.READFILE` + `STRINGS.SPLIT(CHR$(10))` invece di `IO.READLINES`+`UBOUND`;
      `IF isDir <> 0` invece di `IF isDir` (BIT); path del DB a `OpenIndex(p)` non dal globale.
- [x] **VM/C** e **C99Native**: `index` (3 file / 3 chunk) e `search` (top-1 `zebra.txt` 0.333).
      Nota C99: l'array che riceve `STRINGS.SPLIT` deve essere dinamico (`DIM lines() AS STRING`).
- [ ] **Chiudere i due bug IL** alla radice (note `Bugs/(open) - IL-IF-su-Variabile-BIT-e-SQL-OPEN-
      con-Globale`): quando sono corretti, si possono togliere gli aggiramenti qui sopra.

## Vincoli (non negoziabili)

- **Il modello si configura nei settings** (`[llm] model`/`dir`, ADR nel vault): mai hardcoded.
- **Nessuno stub**: ogni capacità o funziona o restituisce un **errore esplicito** documentato.
- **Niente `work/`**: l'indice e gli output stanno in `bin/` (default del compilatore).
- **Parità 4 gambe** per la superficie BASIC che eventualmente si aggiungesse (AGENTS §4.1);
  l'app in sé gira su IL/PCode/VM/C/C99Native.
- **Licenza**: attribuzione MIT dell'originale nel `README.md`; nessun codice Swift/Tauri copiato.
