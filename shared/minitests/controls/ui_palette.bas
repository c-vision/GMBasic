' Command palette — registro dei comandi, filtro per pertinenza, navigazione, esecuzione.
' La palette è un overlay (Ctrl+K): non crea controlli e non prende il fuoco.
' Le azioni si registrano con PALETTE ADD; PALETTE RUN CALL collega la SUB che le
' esegue (riceve l'id dell'azione scelta). L'esecuzione passa dal GPM: con un frame
' attivo il tasto accoda e il frame esegue; in un programma lineare esegue subito.
' EXPECT: add-ok|exec-ok|ran=file.save|next-ok|ran=file.open|filter-ok|run=view.diff|last-ok|ran=file.new|clear-ok|empty-ok
IMPORT CONTROLS

DIM eseguita AS STRING

SUB OnCommand(id AS STRING)
    eseguita = id
END SUB

SUB Main()
    PALETTE CLEAR
    PALETTE ADD "file.save", "Salva il documento", "Ctrl+S", "File"
    PALETTE ADD "file.open", "Apri un file", "Ctrl+O", "File"
    PALETTE ADD "view.diff", "Mostra le differenze", "Ctrl+D", "Vista"
    PALETTE ADD "file.new", "Nuovo file", "", "File"
    PALETTE RUN CALL OnCommand
    PRINT "add-ok"

    ' --- Enter sulla prima voce (query vuota = ordine di registrazione) ------
    eseguita = ""
    PALETTE OPEN
    PALETTE KEY 13
    PRINT "exec-ok"
    PRINT "ran="; eseguita

    ' --- freccia giù: la voce SUCCESSIVA dell'elenco filtrato ---------------
    eseguita = ""
    PALETTE OPEN
    PALETTE KEY 0x1A02
    PALETTE KEY 13
    PRINT "next-ok"
    PRINT "ran="; eseguita

    ' --- filtro per parola del titolo: "differenze" lascia solo view.diff ---
    eseguita = ""
    PALETTE OPEN
    PALETTE TYPE "differenze"
    PALETTE KEY 13
    PRINT "filter-ok"
    PRINT "run="; eseguita

    ' --- la selezione è relativa all'elenco FILTRATO: ultimo giro -----------
    eseguita = ""
    PALETTE OPEN
    PALETTE KEY 0x1A01
    PALETTE KEY 13
    PRINT "last-ok"
    PRINT "ran="; eseguita

    PALETTE CLEAR
    PRINT "clear-ok"

    ' --- registro vuoto: nessuna esecuzione --------------------------------
    eseguita = ""
    PALETTE OPEN
    PALETTE KEY 13
    IF eseguita = "" THEN PRINT "empty-ok" ELSE PRINT "empty-no"; eseguita
END SUB