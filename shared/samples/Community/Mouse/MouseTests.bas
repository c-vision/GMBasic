' FILE: Mouse/MouseTests.bas
' ======================================================================================================================
' TEST UNITARIO: SOTTOSISTEMA MOUSE
' ======================================================================================================================
' Questo programma testa le funzionalità del mouse in modalità Console (e Grafica se disponibile).
' 
' GUIDA AI COMANDI E FUNZIONI (NAMESPACE 'MOUSE'):
' Il modulo supporta due stili di sintassi:
' 1. Legacy:     MOUSE COMANDO argomenti (Richiede IMPORT MOUSE)
' 2. Qualificata: MOUSE.COMANDO(argomenti)
'
' --- COMANDI DI STATO E CONFIGURAZIONE ---
' MOUSE ON / OFF
'   Attiva o disattiva il driver del mouse.
'   Legacy: MOUSE ON             | Qualificata: MOUSE.ON()
'
' MOUSE SHOW / HIDE
'   Mostra o nasconde il cursore del mouse.
'   Legacy: MOUSE SHOW           | Qualificata: MOUSE.SHOW()
'
' MOUSE CURSOR n
'   Cambia la forma del cursore (0=Default, 1=Mano, 2=Croce, etc.).
'   Legacy: MOUSE CURSOR 1       | Qualificata: MOUSE.CURSOR(1)
'
' --- MOVIMENTO E LIMITI ---
' MOUSE SET x, y
'   Sposta forzatamente il cursore alle coordinate specificate.
'   Legacy: MOUSE SET 10, 5      | Qualificata: MOUSE.SET(10, 5)
'
' MOUSE LIMIT x1, y1, x2, y2
'   Restringe il movimento del mouse all'interno di un rettangolo.
'   Legacy: MOUSE LIMIT 0,0,80,25| Qualificata: MOUSE.LIMIT(0,0,80,25)
'
' MOUSE UNLIMIT
'   Rimuove le restrizioni di movimento imposte da LIMIT.
'   Legacy: MOUSE UNLIMIT        | Qualificata: MOUSE.UNLIMIT
'
' --- SINCRONIZZAZIONE ---
' MOUSE WAIT
'   Sospende l'esecuzione del programma finché non si verifica un evento mouse.
'   Legacy: MOUSE WAIT           | Qualificata: MOUSE.WAIT()
'
' --- FUNZIONI DI INPUT (Restituiscono Valori) ---
' X() -> INTEGER
'   Restituisce la coordinata X (Colonna in Console, Pixel in Grafica).
'
' Y() -> INTEGER
'   Restituisce la coordinata Y (Riga in Console, Pixel in Grafica).
'
' MOUSEBUTTON([id]) -> INTEGER
'   Restituisce lo stato dei pulsanti. Se id è omesso, ritorna una bitmask.
'   Se id è specificato (1=SX, 2=DX, 3=Mid), ritorna 1 (premuto) o 0.
'
' WHEEL() -> INTEGER
'   Restituisce il delta dello scorrimento della rotella (positivo/negativo).
'
' PRESENT() -> BIT
'   Restituisce TRUE se un dispositivo mouse è rilevato nel sistema.
'
' --- FUNZIONI HELPER E STATO ---
' LEFT()   -> BIT : Restituisce TRUE se il tasto Sinistro è premuto.
' RIGHT()  -> BIT : Restituisce TRUE se il tasto Destro è premuto.
' MIDDLE() -> BIT : Restituisce TRUE se il tasto Centrale è premuto.
' MOVE()   -> BIT : Restituisce TRUE se il mouse si è mosso dall'ultimo check.
'
' CLICK() -> BIT
'   Restituisce TRUE se è stato rilevato un click (pressione e rilascio rapidi).
'
' CLICK(x1, y1, x2, y2) -> BIT
'   Overload che restituisce TRUE solo se il click è avvenuto dentro il rettangolo.
'
' DBLCLICK() -> BIT
'   Restituisce TRUE se è stato rilevato un doppio click.
' ======================================================================================================================

IMPORT MOUSE
IMPORT CONSOLE
IMPORT GRAPHICS ' Per SCREEN e comandi grafici

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════╗"
    PRINT "║      GM-BASIC MOUSE SYSTEM TEST         ║"
    PRINT "╚═════════════════════════════════════════╝"
    PRINT

    ' --- 0. TEST HARDWARE ---
    PRINT "[0] Controllo Hardware..."
    IF PRESENT() THEN
        PRINT "    -> Mouse rilevato!"
    ELSE
        PRINT "    -> ATTENZIONE: Mouse non rilevato o non supportato."
    END IF
    PRINT

    ' --- 1. TEST ATTIVAZIONE E VISIBILITÀ ---
    PRINT "[1] Inizializzazione..."
    MOUSE ON
    MOUSE SHOW
    PRINT "    -> Mouse Attivato e Visibile."
    PRINT "    -> Muovi il mouse. Premi un tasto per passare alla fase successiva."
    
    WHILE INKEY() = ""
        LOCATE 7, 1
        PRINT "    X: "; X(); "  Y: "; Y(); "  BTN: "; BUTTON(); "        "
        SLEEP 50
    WEND
    PRINT

    ' --- 2. TEST POSIZIONAMENTO E LIMITI ---
    PRINT "[2] Test Posizionamento (SET) e Limiti (LIMIT)..."
    PRINT "    -> Sposto il cursore a (10, 10)..."
    MOUSE SET 10, 10
    SLEEP 500
    
    PRINT "    -> Attivo LIMIT (0,0 - 40,10)... Prova a uscire dal rettangolo!"
    PRINT "    -> Premi un tasto per sbloccare."
    
    ' Sintassi Legacy
    MOUSE LIMIT 0, 0, 40, 10
    
    WHILE INKEY() = ""
        LOCATE 13, 1
        PRINT "    [LIMIT ATTIVO] X: "; X(); " Y: "; Y(); "   "
        SLEEP 50
    WEND
    MOUSE UNLIMIT
    PRINT "    -> Limiti rimossi."
    PRINT

    ' --- 3. TEST CURSORE ---
    PRINT "[3] Test Cambio Cursore..."
    PRINT "    -> Cambio cursore a tipo 2 (es. Mano/Croce)..."
    MOUSE CURSOR 2
    SLEEP 1000
    PRINT "    -> Ripristino cursore default (0)..."
    MOUSE CURSOR 0
    PRINT

    ' --- 4. TEST EVENTI E WAIT ---
    PRINT "[4] Test MOUSE WAIT (Sincronizzazione)..."
    PRINT "    -> In attesa di un evento mouse (click o movimento)..."
    
    ' Pulisce eventuali tasti premuti
    WHILE INKEY() <> "" : WEND
    
    ' Attesa bloccante
    MOUSE WAIT
    
    PRINT "    -> Evento rilevato!"
    PRINT "    -> Stato al risveglio -> X:"; X(); " Y:"; Y(); " BTN:"; BUTTON()
    PRINT

    ' --- 5. TEST ZONE E PULSANTI SPECIFICI (Console) ---
    PRINT "[5] Test Zone (Click in area) e Pulsanti Specifici..."
    PRINT "    -> Clicca nell'area riga 20-22, col 5-15 (Rettangolo immaginario)"
    PRINT "    -> Oppure premi Tasto Destro per saltare."
    
    ' Disegna un box "pulsante"
    LOCATE 20, 5 : PRINT "+----------+"
    LOCATE 21, 5 : PRINT "|  CLICCAMI |"
    LOCATE 22, 5 : PRINT "+----------+"
    
    DIM done = FALSE
    WHILE NOT done
        ' Test MOUSECLICK con coordinate (x1, y1, x2, y2)
        ' Nota: In console X è colonna, Y è riga
        IF CLICK(5, 20, 16, 22) THEN
            LOCATE 24, 1
            PRINT "    -> CLICCATO NEL RETTANGOLO! (Successo)   "
            SLEEP 500
            done = TRUE
        END IF
        
        ' Test alias pulsanti
        IF RIGHT() THEN
            LOCATE 24, 1
            PRINT "    -> Tasto Destro rilevato (Skip).         "
            done = TRUE
        END IF
        
        IF MIDDLE() THEN
             LOCATE 24, 1
             PRINT "    -> Tasto Centrale rilevato.              "
        END IF

        ' Aggiorna stato per feedback visivo usando le nuove funzioni LEFT/RIGHT
        LOCATE 25, 1
        PRINT "    X: "; X(); " Y: "; Y(); " L:"; LEFT(); " R:"; RIGHT(); " M:"; MIDDLE(); "    "
        SLEEP 50
    WEND
    PRINT

    ' --- 6. TEST MODALITÀ GRAFICA (Simulazione logica) ---
    PRINT "[6] Test Interazione Grafica (Click & Drawing)..."
    PRINT "    -> Entro in modalità grafica (SCREEN 13)..."
    SLEEP 1000 
    
    ' Nota: SCREEN 13 è una modalità standard VGA 320x200 (o emulata)
    SCREEN 13 
    CLS
    MOUSE SHOW
    
    COLOR 15, 0
    PRINT "Disegna col mouse (Tieni premuto SX)."
    PRINT "Doppio Click per pulire."
    PRINT "Premi Tasto per uscire."
    
    DIM exitLoop = FALSE
    
    WHILE NOT exitLoop
        ' Verifica tastiera
        IF INKEY() <> "" THEN 
            exitLoop = TRUE
        END IF
        
        ' Verifica Mouse usando l'alias LEFT() invece di BUTTON(1)
        IF LEFT() THEN
            ' Disegna pixel se il tasto sinistro è premuto
            PSET (X(), Y()), 10 ' Colore verde brillante
        END IF
        
        IF DBLCLICK() THEN
            CLS
            PRINT "Schermo pulito (Doppio Click)."
        END IF
        
        ' Piccola pausa per non saturare la CPU
        SLEEP 10
    WEND
    
    ' --- CLEANUP ---
    SCREEN 0 ' Torna a modo testo
    MOUSE HIDE
    MOUSE OFF
    
    PRINT "Test Completato. Bye!"
END SUB