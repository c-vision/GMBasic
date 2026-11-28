# Palette dei comandi — guida utente

La **palette dei comandi** è un overlay che si apre con `Ctrl+K` (o dal
programma): si scrivono un paio di lettere e si sceglie un'azione. È lo stesso
schema degli editor moderni, e non crea controlli: non entra nei registri, non
prende il fuoco e non genera eventi `ON CLICK`.

L'esecuzione dell'azione scelta è l'unico punto che tocca il programma, e passa
dal **GPM**: il tasto accoda, il **frame** esegue — mai inline nel percorso del
tasto, mai su un thread esterno.

## 1. Sintassi

Gli statement usano la stessa forma dei comandi di controllo (`CONTROL <SUB> …`):
prima parola `PALETTE`, poi il verbo.

```basic
PALETTE ADD "file.save", "Salva il documento", "Ctrl+S", "File"
PALETTE RUN CALL OnCommand
```

| Statement | Significato |
|---|---|
| `PALETTE ADD "id", "titolo"[, "scorciatoia"[, "categoria"]]` | Registra (o aggiorna) un'azione. L'`id` è ciò che arriva al runner |
| `PALETTE CLEAR` | Svuota il registro |
| `PALETTE OPEN` / `PALETTE CLOSE` | Apre (query e selezione azzerate) / chiude l'overlay |
| `PALETTE TYPE "testo"` | Scrive nella query di ricerca |
| `PALETTE KEY <espressione>` | Invia un tasto alla palette (vedi §3) |
| `PALETTE RUN CALL <sub>` | Collega la SUB/FUNCTION che esegue le azioni |

La SUB del runner riceve **l'id** dell'azione scelta:

```basic
SUB OnCommand(id AS STRING)
    SELECT CASE id
        CASE "file.save" : CALL Salva()
        CASE "view.diff" : CALL Mostra_diff()
    END SELECT
END SUB
```

## 2. Il filtro

La ricerca è **deterministica** e verificabile headless. L'ordine dei risultati:

1. titolo che **inizia** con la query;
2. titolo che **contiene** la query;
3. **categoria** o **scorciatoia** che contiene la query.

A pari merito vale l'ordine di registrazione. La query è normalizzata
(minuscole, spazi singoli), quindi `"SA"`, `"sa"` e `" sa"` sono la stessa
cosa. Query vuota = tutte le azioni, nell'ordine di registrazione.

Registrare due volte lo stesso `id` **aggiorna** la voce (non duplica).

## 3. Navigazione e tasti

Quando l'overlay è aperto è **modale**: ogni tasto va a lui, nessun controllo
sotto reagisce.

| Tasto | Codice | Azione |
|---|---|---|
| `ESC` | `27` | chiude |
| `Enter` | `13` | esegue la voce selezionata |
| `Backspace` | `8` / `127` | cancella un carattere della query |
| Freccia su / giù | `0x1A01` / `0x1A02` | sposta la selezione (con giro circolare) |
| Pagina su / giù | `0x1A04` / `0x1A05` | sposta di 12 voci |

La **selezione è relativa all'elenco filtrato**, non a quello completo: filtrare
e poi premere `Enter` esegue ciò che si vede.

## 4. Esecuzione: GPM e frame

- **Con un frame attivo** (TUI a celle o percorso pixel, dentro un programma che
  cicla: `GPM` o `CONTROL … FRAME`): `Enter` accoda l'azione; il frame chiama
  `gmb_palette_pump()` e l'azione parte come **task del GPM**. La palette si
  chiude prima che l'azione inizi.
- **Programma lineare** (nessuno scheduler, nessun frame che pompi): l'azione
  viene eseguita subito, dal programma stesso — non da un thread esterno.

L'overlay è acceso **solo con una finestra reale**: in esecuzione headless
(driver video dummy, gate di test) non si accende, così i test restano
deterministici e nessuna immagine golden contiene la palette.

## 5. Disegno

L'overlay non chiede nulla al programma: il frame lo dipinge **dopo** i
controlli e **dopo** l'inspector.

- TUI a celle: riquadro centrato con titolo `COMANDI (Ctrl+K)`, riga di ricerca
  in evidenza, l'elenco a destra della selezione.
- Percorso pixel: pannello arrotondato con la riga di ricerca evidenziata,
  testo dal tema del controllo corrente (è la stessa forma del resto della UI).

`Ctrl+K` è intercettato prima dei controlli: anche con il fuoco in una TEXTBOX
il tasto apre la palette (è la convenzione degli editor).

## 6. Disponibilità e verifiche

| Gamba | `PALETTE …` |
|---|---|
| IL | sì (parità per costruzione: overlay del frame, esecuzione via GPM) |
| PCodeVM | sì |
| VM/C | sì |
| C99Native | sì |

- Modulo: `GMBasic/native/gmb_runtime_c/include/gmb_ui_palette.h`,
  `src/gmb_ui_palette.c` (registro, filtro, overlay, accodamento).
- Emitter C99: `src/gmb_c99_emit_controls.c` (statement `PALETTE …`) e
  `emit_palette_runners` in `src/gmb_c99_emit.c` (bridge `id$ → SUB`).
- Parser C: `gmb_p_parse_palette` (`gmb_parser_stmt_controls.c`), riconosciuto
  come **soft keyword** (`PALETTE <SUB>`, stessa forma di `CONTROL <SUB>`).
- Selfcheck deterministico: target CMake `gmb_palette_selfcheck` (40 controlli).
- MiniTest: `shared/minitests/controls/ui_palette.bas`.
- Sample completo: `shared/samples/Controls/CommandPalette.bas`.

## 7. Riferimenti

- Inspector della UI (l'altro overlay): `Guide/utente/`.
- Animazione dei controlli: `Animazione-Guida-Utente.md`.
- Ruoli e layout: `STYLE-Ruoli-e-Layout-Guida-Utente.md`.