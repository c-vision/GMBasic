' FILE: Clipper/SchedaAnagrafica.bas
' TAG: interattivo — il READ attende l'input (in CI: GMB_TUI_SCRIPT oppure SENDKEY).
' SCENARIO DIDATTICO — scheda anagrafica in stile Clipper: @ SAY, @ GET, READ.
'
' Il modello Clipper separa tre momenti, ed è la ragione per cui queste tre istruzioni
' esistono ancora oggi:
'   @ r, c SAY <testo>   DISEGNA l'etichetta in una posizione fissa dello schermo;
'   @ r, c GET <var>     CREA un campo editabile sulla variabile (PICTURE, RANGE, TYPE…);
'   READ                 mette in EDITING tutti i campi creati e applica i valori alle
'                        variabili quando l'utente conferma (INVIO) o annulla (ESC).
'
' Il programma è una vera scheda interattiva: si compila il cliente, poi si mostra la
' riga normalizzata (nome in maiuscolo, età con controllo di validità).
'
' Esecuzione interattiva:      tools/sh/gmb_c99native_run.sh ../shared/samples/Clipper/SchedaAnagrafica.bas
' Esecuzione scriptata (CI):   GMB_TUI_SCRIPT=$'mario\r42\rMilano\r' …  (un valore per campo)
'
' Domini: Clipper (SAY/GET/READ) + CONSOLE + stringhe. Verificato sulle 4 gambe con
' input scriptato; l'output dipende SOLO dallo script, non dall'ambiente.
IMPORT CONSOLE

FUNCTION Ripeti(testo AS STRING, volte AS LONG) AS STRING
    DIM s AS STRING
    DIM i AS LONG
    s = ""
    FOR i = 1 TO volte
        s = s + testo
    NEXT i
    RETURN s
END FUNCTION

SUB Main()
    CLS

    ' ── 1. Cornice della scheda (@SAY) ───────────────────────────────────────
    @ 1, 3 SAY "╔" + Ripeti("═", 46) + "╗"
    @ 2, 3 SAY "║  ANAGRAFICA CLIENTI — nuovo inserimento   ║"
    @ 3, 3 SAY "╚" + Ripeti("═", 46) + "╝"

    ' ── 2. Campi editabili (@GET) ────────────────────────────────────────────
    ' PICTURE "@!" forza il maiuscolo mentre si digita; RANGE limita un numerico.
    DIM nome AS STRING
    DIM eta AS INTEGER
    DIM citta AS STRING
    nome = ""
    eta = 0
    citta = ""

    @ 5, 3 SAY "Nome......:"
    @ 5, 15 GET nome PICTURE "@!" SIZE 30

    @ 7, 3 SAY "Eta'......:"
    @ 7, 15 GET eta RANGE 18, 99 SIZE 4

    @ 9, 3 SAY "Citta'....:"
    @ 9, 15 GET citta SIZE 20

    ' ── 3. Editing dei campi (READ) ──────────────────────────────────────────
    @ 11, 3 SAY "(INVIO conferma il campo, ESC annulla la scheda)"
    READ

    ' ── 4. Riga normalizzata ─────────────────────────────────────────────────
    ' Quello che farebbe un gestionale dopo il READ: normalizzare e validare.
    CLS
    PRINT "== Scheda ricevuta =="
    PRINT "nome:  ["; nome; "]"
    PRINT "eta:   "; eta
    PRINT "citta: ["; citta; "]"

    DIM valido AS BIT
    valido = (LEN(TRIM$(nome)) > 0) AND (eta >= 18) AND (eta <= 99) AND (LEN(TRIM$(citta)) > 0)
    PRINT "scheda valida: "; valido

    IF valido THEN
        PRINT "riga per l'archivio: "; TRIM$(nome); ";"; eta; ";"; TRIM$(citta)
    ELSE
        PRINT "compilare tutti i campi (eta' fra 18 e 99)"
    END IF

    ' ── 5. Riepilogo a schermo ───────────────────────────────────────────────
    @ 1, 3 SAY "╔" + Ripeti("═", 46) + "╗"
    @ 2, 3 SAY "║  CLIENTE REGISTRATO                       ║"
    @ 3, 3 SAY "╚" + Ripeti("═", 46) + "╝"
    @ 5, 5 SAY "Nome : " + TRIM$(nome)
    @ 6, 5 SAY "Eta' : " + TRIM$(STR$(eta))
    @ 7, 5 SAY "Citta': " + TRIM$(citta)
END SUB
