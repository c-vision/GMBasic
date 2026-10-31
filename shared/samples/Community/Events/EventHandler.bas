' FILE: Events/EventHandler.bas
' ======================================================================================================================
' TEST UNITARIO: GESTIONE EVENTI (EVENT, RAISE, ADD/REMOVE HANDLER)
' ======================================================================================================================
' Questo programma testa il sistema di eventi "Observer" per le classi.
' Definisce una classe "Button" con un evento "Click" e dimostra come
' sottoscrivere e disiscrivere dinamicamente i gestori di eventi.
' ======================================================================================================================

IMPORT CONSOLE

' --- DEFINIZIONE DELLA CLASSE CON EVENTI ---

PUBLIC CLASS Button
    ' Definisce la firma dell'evento: chi lo sottoscrive riceverà il bottone stesso come parametro.
    PUBLIC EVENT Click(sender AS Button)
    
    ' Un campo per identificare l'istanza del bottone
    PUBLIC DIM Name AS STRING

    ' Costruttore
    PUBLIC SUB NEW(buttonName AS STRING)
        SELF.Name = buttonName
    END SUB

    ' Metodo che scatena l'evento "Click"
    PUBLIC SUB PerformClick()
        PRINT
        PRINT "--- Bottone '" + SELF.Name + "' cliccato. Scateno l'evento... ---"
        RAISE EVENT Click(SELF)
        PRINT "--- Evento scatenato. ---"
        PRINT
    END SUB
END CLASS

' --- DEFINIZIONE DEI GESTORI DI EVENTI ---

' Handler 1: Risponde specificamente al click del bottone
SUB Button1_ClickHandler(sender AS Button)
    PRINT "[HANDLER 1] L'evento Click è stato ricevuto dal bottone: '" + sender.Name + "'"
END SUB

' Handler 2: Un logger generico che registra l'evento
SUB Logger_ClickHandler(source AS Button)
    PRINT "[HANDLER 2 - Logger] Registrato un click dal bottone con nome: '" + source.Name + "'"
END SUB

' --- PROGRAMMA PRINCIPALE ---

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════╗"
    PRINT "║      Test del Sistema di Gestione Eventi (Observer)        ║"
    PRINT "╚════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' Crea un'istanza della classe Button
    DIM MyButton AS Button = NEW Button("OK")

    ' ======================================================
    ' FASE 1: Sottoscrizione di entrambi i gestori
    ' ======================================================
    PRINT "FASE 1: Sottoscrivo due gestori all'evento 'MyButton.Click'."
    
    ' Collega il primo handler
    ADD HANDLER MyButton.Click, Button1_ClickHandler
    
    ' Collega il secondo handler (logger)
    ADD HANDLER MyButton.Click, Logger_ClickHandler
    
    ' Scatena l'evento. Entrambi gli handler dovrebbero essere invocati.
    MyButton.PerformClick()
    
    REM WAITKEY ' Pausa per osservare l'output

    ' ======================================================
    ' FASE 2: Rimozione di un gestore
    ' ======================================================
    PRINT "FASE 2: Rimuovo il gestore 'Logger_ClickHandler'."
    
    REMOVE HANDLER MyButton.Click, Logger_ClickHandler
    
    ' Scatena di nuovo l'evento. Solo il primo handler dovrebbe essere invocato.
    MyButton.PerformClick()
    
    REM WAITKEY

    ' ======================================================
    ' FASE 3: Rimozione dell'ultimo gestore e test finale
    ' ======================================================
    PRINT "FASE 3: Rimuovo il gestore 'Button1_ClickHandler'."
    
    REMOVE HANDLER MyButton.Click, Button1_ClickHandler
    
    ' Scatena l'evento per l'ultima volta. Nessun handler dovrebbe essere invocato.
    MyButton.PerformClick()
    
    PRINT "Test completato. Nessun output dagli handler come previsto."
    PRINT
END SUB