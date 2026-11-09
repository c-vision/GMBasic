# ClientBook — TODO

Stato: **v0.1** (CRUD completo, 4 modalità UI, verificato su C99Native).

## Fatto

- [x] Schema DBF (`TYPE TCliente`) creato al primo avvio in `<bin>/clienti.dbf`
- [x] Aggiungi / Modifica / Elimina / Esci con pulsanti
- [x] Form: Cognome, Nome, Data di nascita (DatePicker), Sesso (CheckBox),
      Paese di nascita (ComboBox 20 paesi)
- [x] Elenco clienti con selezione (carica il cliente nel form)
- [x] Selezione modalità `-c` / `-t` / `-s` / `-g` (default `-t`) via `COMMAND$`
- [x] Layout unico in celle (`USE CELLS`) per TUI e GUI
- [x] `--selftest` (CRUD headless su DBF temporaneo) e `--demo` (3 frame + screenshot)
- [x] Compilazione con `gmb build` → `bin/ClientBook` eseguibile (nessuno script di lancio)
- [x] Dati accanto all'eseguibile (`IO.BINPATH()`), indipendenti dalla CWD

## Da fare

- [ ] **Conferma prima di eliminare** (`CONFIRMATIONBOX`/`CONFIRM`): oggi `Elimina`
      agisce subito sul cliente selezionato.
- [ ] **Validazione**: data di nascita non futura e non antecedente al 1900; cognome
      o nome obbligatorio (oggi solo un controllo minimo su Aggiungi).
- [ ] **Ricerca/filtro** per cognome (campo di ricerca + filtro sulla lista).
- [ ] **Ordinamento** per cognome/nome (indice DBF o `ARRAY SORT` lato programma).
- [ ] **Paginazione** della lista quando i clienti superano le righe visibili.
- [ ] **Esportazione CSV** dell'archivio (`DBF COPY TO` / `EXPORT`).
- [ ] **Parità 4 gambe** (IL, PCodeVM, VM/C): verificare `COMMAND$`, `TEXTMODE`,
      `IO.BINPATH` e `DBF` anche sui runner; oggi la verifica è sulla leg C99Native.
- [ ] **`--fixture`**: archivio demo deterministico per i test (oggi il seed dipende
      dall'assenza del file).
- [ ] Screenshot anche per la modalità TUI (golden PNG in cell mode).
