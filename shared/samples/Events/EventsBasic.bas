' FILE: Events/EventsBasic.bas
' ======================================================================================================================
' TEST UNITARIO: GESTIONE EVENTI (EVENT, RAISE, ADD/REMOVE HANDLER)
' ======================================================================================================================
' Questo programma testa il sistema di eventi "Observer" per le classi,
' che permette di creare codice reattivo e disaccoppiato.
'
' GUIDA AI COMANDI E SINTASSI DEGLI EVENTI:
'
' 1. DICHIARAZIONE DI UN EVENTO (EVENT)
'    Un evento viene dichiarato all'interno di una classe e definisce una "firma"
'    (un set di parametri) che tutti i gestori dovranno rispettare.
'
'    Sintassi: [PUBLIC|PRIVATE|PROTECTED] EVENT NomeEvento([param1 AS Tipo1, ...])
'
'    Esempio:
'    PUBLIC CLASS Downloader
'        ' Definisce un evento che notificherà i progressi con un valore Single.
'        PUBLIC EVENT Progress(percentage AS SINGLE)
'    END CLASS
'
' 2. SCATENAMENTO DI UN EVENTO (RAISE EVENT)
'    All'interno di un metodo della classe, `RAISE EVENT` invoca tutti i gestori
'    che si sono sottoscritti a quell'evento, passando loro gli argomenti richiesti.
'
'    Sintassi: RAISE EVENT NomeEvento(argomento1, ...)
'
'    Esempio:
'    PUBLIC SUB StartDownload()
'        ' ... logica ...
'        ' Notifica a tutti i listener che il progresso è al 50%.
'        RAISE EVENT Progress(0.5)
'    END SUB
'
' 3. SOTTOSCRIZIONE A UN EVENTO (ADD HANDLER)
'    Un codice esterno può "ascoltare" un evento sottoscrivendo una subroutine
'    (handler) la cui firma sia compatibile con quella dell'evento.
'
'    Sintassi: ADD HANDLER oggetto.NomeEvento, NomeSubroutine
'
'    Esempio:
'    DIM Downloader1 AS Downloader = NEW Downloader
'    ' Collega la subroutine 'MostraProgresso' all'evento 'Progress' di Downloader1.
'    ADD HANDLER Downloader1.Progress, MostraProgresso
'
'    SUB MostraProgresso(p AS SINGLE)
'        PRINT "Download al "; p * 100; "%"
'    END SUB
'
' 4. RIMOZIONE DI UNA SOTTOSCRIZIONE (REMOVE HANDLER)
'    Permette di smettere di ascoltare un evento.
'
'    Sintassi: REMOVE HANDLER oggetto.NomeEvento, NomeSubroutine
'
'    Esempio:
'    ' Smette di ascoltare i progressi.
'    REMOVE HANDLER Downloader1.Progress, MostraProgresso
'
' ======================================================================================================================

IMPORT MOUSE
IMPORT GRAPHICS
IMPORT CONSOLE

SUB Main()
    PRINT "Inizializzazione Modalità Grafica..."
    SCREEN 12 ' 640x480 VGA
    CLS
    
    MOUSE ON
    MOUSE SHOW

    PRINT "╔══════════════════════════════════════════════════╗"
    PRINT "║      TEST EVENTI ASINCRONI (MOUSE & WINDOW)      ║"
    PRINT "╠══════════════════════════════════════════════════╣"
    PRINT "║ 1. Clicca col mouse (Singolo/Doppio)             ║"
    PRINT "║ 2. Ruota la rotella del mouse                    ║"
    PRINT "║ 3. Prova a ridimensionare la finestra            ║"
    PRINT "║ 4. Premi Tasto Destro per fermare il tracciamento║"
    PRINT "║ 5. Premi ESC o Chiudi la finestra per uscire     ║"
    PRINT "╚══════════════════════════════════════════════════╝"

    ' Variabile di stato per il loop principale
    DIM Running = TRUE
    
    ' Variabile per attivare/disattivare il log del movimento (molto verboso)
    DIM TrackMove = TRUE

    ' ==========================================================================
    ' 1. REGISTRAZIONE EVENTI MOUSE
    ' ==========================================================================
    
    ' Test GOSUB: Gestisce il click sinistro localmente
    ON MOUSE CLICK GOSUB OnClick
    
    ' Test CALL: Gestisce il doppio click tramite una SUB esterna
    ON MOUSE DBLCLICK OnDoubleClickHandler()
    
    ' Test GOSUB: Gestisce lo scroll
    ON MOUSE SCROLL GOSUB OnScroll
    
    ' Test GOSUB: Gestisce il movimento
    ON MOUSE MOVE GOSUB OnMove
    
    ' Test GOSUB: Gestisce la pressione del tasto destro (DOWN)
    ON MOUSE DOWN GOSUB OnMouseDown

    ' ==========================================================================
    ' 2. REGISTRAZIONE EVENTI FINESTRA
    ' ==========================================================================
    
    ' Test GOSUB: Gestisce la richiesta di chiusura (X della finestra)
    ON WINDOW QUIT GOSUB OnExitRequest
    
    ' Test CALL: Gestisce il ridimensionamento
    ON WINDOW RESIZE CALL OnResizeHandler()

    ' ==========================================================================
    ' 3. REGISTRAZIONE EVENTI TASTIERA (Legacy Integration)
    ' ==========================================================================
    
    ' ESC (Codice 27) per uscire
    ON KEY(27) GOSUB OnExitRequest

    ' ==========================================================================
    ' 4. MAIN LOOP (EVENT DRIVEN)
    ' ==========================================================================
    PRINT "Sistema pronto. In attesa di eventi..."
    
    WHILE Running
        ' Il loop è vuoto o fa operazioni di background minime.
        ' Non c'è bisogno di controllare MOUSEX o INKEY qui, 
        ' gli eventi interromperanno lo SLEEP o il SYNC.
        SLEEP 100 
    WEND

    ' ==========================================================================
    ' 5. CLEANUP
    ' ==========================================================================
    MOUSE OFF
    SCREEN 0
    PRINT "Test Concluso Correttamente."
    EXIT SUB

    ' ==========================================================================
    ' HANDLERS LOCALI (GOSUB)
    ' ==========================================================================
    
    OnClick:
        ' Nota: Usiamo le funzioni di stato per avere i dettagli
        PRINT "[EVENTO] Click Sinistro a (" + STR(X()) + "," + STR(Y()) + ")"
        RETURN

    OnScroll:
        DIM delta = WHEEL()
        PRINT "[EVENTO] Scroll rilevato. Delta: " + STR(delta)
        RETURN

    OnMove:
        ' Stampiamo solo se il tracking è attivo per non intasare la console
        IF TrackMove THEN
            ' Stampa su una riga fissa per pulizia
            LOCATE 10, 1
            PRINT "Posizione Mouse: " + STR(X()) + ", " + STR(Y()) + "     "
        END IF
        RETURN
        
    OnMouseDown:
        ' Se è il tasto destro (2), disabilitiamo/abilitiamo il log del movimento
        IF RIGHT() THEN
            TrackMove = NOT TrackMove
            PRINT "[EVENTO] Tasto Destro: Tracking Movimento = " + STR(TrackMove)
        END IF
        RETURN

    OnExitRequest:
        PRINT "[EVENTO] Richiesta di Uscita ricevuta (Tastiera o Finestra)."
        Running = FALSE
        RETURN

END SUB

' ==============================================================================
' HANDLERS ESTERNI (CALL)
' ==============================================================================

SUB OnDoubleClickHandler()
    PRINT "[EVENTO CALL] Doppio Click Rilevato! (Handler Esterno)"
    ' Esempio di azione visiva
    CIRCLE (X(), Y()), 20
END SUB

SUB OnResizeHandler()
    PRINT "[EVENTO CALL] La finestra è stata ridimensionata."
    ' Qui si potrebbe chiamare una routine di ricalcolo del layout
END SUB