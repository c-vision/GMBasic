# TODO — GMBasicTuiIde (IDE TUI in BASIC)

Stato: **v1.0** — IDE funzionante (multi-tab, I/O file, outline/completion/lint/goto/refs/make),
verificato su IL e in compilazione standalone (C99Native). Vedi `README.md`.

Progetto **autonomo del monorepo** dal 2026-10-31 (prima `shared/APPS/GMBasicIde`): serve anche da
**banco di misura del footprint** di un programma BASIC compilato (vedi README §"Perché è un
progetto autonomo").

## Funzionalità residue

- [ ] **Popup completion inline** con inserimento: aprire un ListBox popup coi candidati
      (`gmb_idef_complete`) e inserire la scelta nel CodeArea alla riga/colonna del cursore.
- [ ] **goto per interazione** sull'outline (oggi: F7 legge la voce selezionata). Serve leggere
      l'evento di selezione del ListBox (in TEXT `gmb_form_poll_event` è vuoto → usare i callback o
      un TBROWSE con `gmb_tui_control_selrow`).
- [ ] **Ricerca/find nel buffer** (in avanti/indietro), replace, con evidenziazione.
- [ ] **Rename/snippet** esposti come tasti (le wrapper `gmb_idef_rename`/`snippet` esistono già).
- [ ] **Colonne/caret nella StatusBar** (riga/colonna del cursore) — serve un getter
      `gmb_code_area_cursor_line`/`_col` per indice (oggi c'è solo su puntatore) o esporlo.
- [ ] **Dimensione schermo reale** (oggi `gW=100,gH=30` fissi) — usare la dimensione del terminale.
- [ ] **Verifica su display** reale (headless non copre GRAPHICS); provare il loop interattivo.
- [ ] **Guida utente** in vault (`Guide/utente/`).

## Debito tecnico / migliorie C

- [ ] `gmb_tui_control_text_ptr`/`set_text` usano un buffer statico condiviso: documentato, ma
      attenzione a chiamate annidate.
- [ ] `gmb_tui_control_set_text` per ListBox usa '\n'; le factory (`gmb_tui_add_*`) usano '|'
      (`tui_split`). Armonizzare il separatore (o wrappare) per ridurre l'asimmetria.
- [ ] Nessun setter per nome di `items`/`tabs` in TUI (solo il setter per indice aggiunto qui).

## Collegamenti

- Piano: vault `Todo/(open) - IDE-TUI-BASIC-e-standalone-e-VSCode-LSP.md`.
- Logica C + wrapper: `native/gmb_runtime_c/{include/gmb_ide.h,src/gmb_ide.c}`.
- Controlli TUI: `native/gmb_runtime_c/{include/gmb_tui_app.h,src/gmb_tui_controls_add.c}`.
