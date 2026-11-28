' ============================================================================
' CommandPalette — editor minimo con la palette dei comandi (Ctrl+K).
' ============================================================================
' La palette è un overlay: non crea controlli e non prende il fuoco. Il programma
' registra le azioni una volta sola e collega la SUB che le esegue:
'
'     PALETTE ADD "file.save", "Salva il documento", "Ctrl+S", "File"
'     PALETTE RUN CALL OnCommand          ' OnCommand(id AS STRING)
'
' L'esecuzione passa dal GPM: il tasto accoda e il frame esegue. Il sample la
' guida in modo deterministico con PALETTE TYPE / PALETTE KEY (stessi codici
' della guida: 13 = Enter, 0x1A01/0x1A02 = frecce), così l'esito è verificabile
' senza una finestra. In una app reale basta l'overlay: Ctrl+K e le frecce.
'
' Esecuzione:  tools/sh/gmb_c99native_run.sh ../shared/samples/Controls/CommandPalette.bas
' ============================================================================
IMPORT CONTROLS

DIM ultimo    AS STRING
DIM documento AS STRING
DIM stato     AS STRING
DIM riga      AS STRING

' L'unico punto che tocca il documento: riceve l'ID dell'azione scelta.
SUB OnCommand(id AS STRING)
    ultimo = id
    SELECT CASE id
        CASE "file.save"
            stato = "salvato (" + STR$(LEN(documento)) + " caratteri)"
        CASE "file.open"
            documento = "README.md" + CHR$(10) + "# Progetto Aurora"
            stato = "aperto"
        CASE "view.diff"
            riga = "diff del branch corrente"
            stato = "diff"
        CASE "edit.clear"
            documento = ""
            stato = "svuotato"
        CASE ELSE
            stato = "azione sconosciuta: " + id
    END SELECT
END SUB

SUB Main()
    SCREEN 0

    ' --- registro delle azioni (una volta, all'avvio) -----------------------
    PALETTE CLEAR
    PALETTE ADD "file.save",  "Salva il documento",   "Ctrl+S", "File"
    PALETTE ADD "file.open",  "Apri un file",         "Ctrl+O", "File"
    PALETTE ADD "view.diff",  "Mostra le differenze", "Ctrl+D", "Vista"
    PALETTE ADD "edit.clear", "Svuota il documento",  "",       "Modifica"
    PALETTE RUN CALL OnCommand

    LISTBOX 1, 1, 100, 10, "" NAME "editor" STYLE "mode=markdown"
    TEXTBOX 1, 12, 100, 1, documento NAME "nome"

    ' --- 1) Enter sulla prima voce: file.save ------------------------------
    documento = "# Progetto Aurora"
    ultimo = "" : stato = ""
    PALETTE OPEN
    PALETTE KEY 13
    PRINT "1) "; ultimo; " -> "; stato

    ' --- 2) filtro per categoria: "vista" lascia solo view.diff ------------
    ultimo = "" : stato = ""
    PALETTE OPEN
    PALETTE TYPE "vista"
    PALETTE KEY 13
    PRINT "2) "; ultimo; " -> "; stato

    ' --- 3) filtro per scorciatoia: "ctrl+o" apre ---------------------------
    ultimo = "" : stato = ""
    documento = ""
    PALETTE OPEN
    PALETTE TYPE "ctrl+o"
    PALETTE KEY 13
    PRINT "3) "; ultimo; " -> "; stato
    PRINT "    documento: "; documento

    ' --- 4) nessun risultato: Enter non esegue nulla ----------------------
    ultimo = ""
    PALETTE OPEN
    PALETTE TYPE "zzz"
    PALETTE KEY 13
    IF ultimo = "" THEN PRINT "4) nessuna azione (come previsto)" ELSE PRINT "4) inatteso: "; ultimo

    ' --- 5) ESC chiude senza eseguire --------------------------------------
    PALETTE OPEN
    PALETTE KEY 27
    PRINT "5) chiusa con ESC"
    PRINT "=== fine ==="
END SUB