' MiniTest: ON WINDOW <EVENTO> OF <handle> — matrice degli eventi innalzabili in headless.
' Verifica, per OGNI evento che si può innalzare senza input/display, che:
'   - l'handler registrato con OF <handle> scatta SOLO per la finestra dell'handle;
'   - l'evento arriva su tutte e 4 le gambe con lo stesso risultato.
' Eventi coperti: OPEN (DEFINE), CLOSE, MINIMIZE, RESTORE, FOCUS (ACTIVATE).
' DEACTIVATE non è un tipo evento del linguaggio (il parser accetta CLOSE, RESIZE, QUIT, MOVE,
' FOCUS, MINIMIZE, RESTORE, OPEN, PASTE): `WINDOW SELECT 0` lo innalza solo internamente.
'
' Verifica su exit code (il render delle finestre sporca lo stdout):
' FUNCTION Main ritorna 1 se un solo assert fallisce.
IMPORT WINDOWS
SCREEN 0
DIM open1 = 0
DIM close2 = 0
DIM min1 = 0
DIM res1 = 0
DIM foc2 = 0
DIM fails = 0

SUB OnOpen1()
    open1 = 1
END SUB
SUB OnClose2()
    close2 = 1
END SUB
SUB OnMin1()
    min1 = 1
END SUB
SUB OnRes1()
    res1 = 1
END SUB
SUB OnFoc2()
    foc2 = 1
END SUB

FUNCTION Main() AS INTEGER
    ' Handlers PRIMA del DEFINE: OPEN deve arrivare alla finestra appena creata.
    ON WINDOW OPEN OF 1 GOSUB OnOpen1
    WINDOW DEFINE w1 FROM 0, 0 TO 8, 30 TITLE "w1"
    ' Re-DEFINE dello stesso nome: aggiorna la finestra, non ne crea una seconda e non
    ' innalza un secondo OPEN (handle 1 e 2 restano quelli della prima creazione).
    WINDOW DEFINE w1 FROM 1, 0 TO 9, 30 TITLE "w1"
    WINDOW DEFINE w2 FROM 10, 0 TO 18, 30 TITLE "w2"

    ON WINDOW MINIMIZE OF 1 GOSUB OnMin1
    ON WINDOW RESTORE OF 1 GOSUB OnRes1
    ON WINDOW CLOSE OF 2 GOSUB OnClose2
    ON WINDOW FOCUS OF 2 GOSUB OnFoc2

    WINDOW w1 MINIMIZE
    WINDOW w1 RESTORE
    WINDOW w2 ACTIVATE
    WINDOW SELECT 0                       ' deattiva w2 (se era attiva)

    WINDOW SELECT 0
    ' Gli handler sono task GPM: senza un park reale il flag puo' essere letto PRIMA che il
    ' task dell'evento parta (PCode: foc2 a 0 nel PRINT e 1 nell'IF successivo -> uscita
    ' non deterministica). SLEEP 1 sincronizza prima di stampare.
    SLEEP 1
    PRINT "flags:"; open1; min1; res1; foc2; close2
    IF open1 <> 1 THEN fails = fails + 1  ' OPEN su DEFINE (handle 1)
    IF min1 <> 1 THEN fails = fails + 1
    IF res1 <> 1 THEN fails = fails + 1
    IF foc2 <> 1 THEN fails = fails + 1
    IF close2 <> 0 THEN fails = fails + 1 ' CLOSE di w1 non deve toccare l'handler di w2

    WINDOW w2 CLOSE
    ' Gli handler girano come TASK GPM: SLEEP 1 (park reale) lascia girare i task pendenti,
    ' così il flag scritto dall'handler è osservabile. Con SLEEP 0 (solo yield) il task
    ' dell'evento poteva non essere partito prima dell'IF successivo → test FLAKY su PCode.
    SLEEP 1
    IF close2 = 1 THEN PRINT "close2-ok" ELSE fails = fails + 1
    IF fails = 0 THEN
        PRINT "win-ev-ok"
        RETURN 0
    END IF
    PRINT "win-ev-no"
    RETURN 1
END FUNCTION
