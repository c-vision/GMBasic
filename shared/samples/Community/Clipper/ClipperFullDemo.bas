IMPORT WINDOWS
' FILE: Samples/Clipper/ClipperFullDemo.bas
' Demo completa: Window Management + Event System + ON KEY + ON MOUSE

SUB Main()
    PRINT "=== ClipperLib Full Demo ==="
    PRINT "Test: WINDOW, ON WINDOW, ON KEY, ON MOUSE"
    PRINT
    
    ' ── Registra handler eventi ──
    ON WINDOW OPEN GOSUB WinOpened
    ON WINDOW CLOSE GOSUB WinClosed
    ON WINDOW MOVE GOSUB WinMoved
    ON WINDOW FOCUS GOSUB WinFocused
    ON WINDOW MINIMIZE GOSUB WinMinimized
    ON WINDOW RESTORE GOSUB WinRestored
    ON KEY(%K_F1) GOSUB ShowHelp
    ON KEY(%K_ESC) GOSUB DoQuit
    ON MOUSE CLICK GOSUB MouseClicked
    
    ' ── Crea finestre ──
    PRINT "Creazione finestre..."
    WINDOW DEFINE Main FROM 1, 1 TO 50, 20 TITLE "Finestra Principale"
    WINDOW DEFINE Help FROM 22, 1 TO 40, 30 TITLE "Help" DOUBLE
    WINDOW DEFINE Log FROM 1, 32 TO 25, 50 TITLE "Log Eventi"
    
    ' ── Gestisci finestre ──
    PRINT "Attivazione Main..."
    WINDOW Main ACTIVATE
    
    PRINT "Attivazione Help..."
    WINDOW Help ACTIVATE
    
    PRINT "Minimizzazione Help..."
    WINDOW Help MINIMIZE
    
    PRINT "Ripristino Help..."
    WINDOW Help RESTORE
    
    PRINT "Chiusura Help..."
    WINDOW Help CLOSE
    
    ' ── Event loop ──
    PRINT
    PRINT "Demo attiva. Premi ESC per uscire, F1 per help."
    DO WHILE g_running
        ' Eventi gestiti via interrupt
    LOOP
    
    PRINT "Demo completata."
    EXIT SUB

' ── Flag globale ──
DIM g_running AS BOOLEAN
DIM g_clickCount AS INTEGER
g_running = TRUE
g_clickCount = 0

' ── Handler eventi ──
WinOpened:
    PRINT "[EVENT] Finestra aperta"
    RETURN

WinClosed:
    PRINT "[EVENT] Finestra chiusa"
    RETURN

WinMoved:
    PRINT "[EVENT] Finestra spostata"
    RETURN

WinFocused:
    PRINT "[EVENT] Finestra ha ricevuto focus"
    RETURN

WinMinimized:
    PRINT "[EVENT] Finestra minimizzata"
    RETURN

WinRestored:
    PRINT "[EVENT] Finestra ripristinata"
    RETURN

ShowHelp:
    PRINT "[KEY] F1 premuto - Help!"
    PRINT "  WINDOW DEFINE per creare finestre"
    PRINT "  WINDOW ACTIVATE per attivarle"
    PRINT "  ON WINDOW per eventi finestra"
    PRINT "  ON KEY per scorciatoie tastiera"
    PRINT "  ON MOUSE per eventi mouse"
    RETURN

DoQuit:
    PRINT "[KEY] ESC premuto - Uscita..."
    g_running = FALSE
    RETURN

MouseClicked:
    g_clickCount = g_clickCount + 1
    PRINT "[MOUSE] Click #"; g_clickCount
    RETURN
END SUB
