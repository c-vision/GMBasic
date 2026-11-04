' FILE: windows/WindowEventsComplete.bas
' Sample COMPLETO del modello eventi finestra: usa ogni forma di ON WINDOW implementata.
'
'   - OPEN / CLOSE / MINIMIZE / RESTORE / FOCUS (eventi innalzabili dal programma)
'   - OF <handle> con handle in VARIABILE (espressione, non solo letterale)
'   - forma SENZA OF (wildcard: vale per ogni finestra)
'   - azioni: CALL sub(), GOSUB sub, GOTO <label locale>, STOP (deregistra)
'
' Deterministico e headless: nessun input, nessuna attesa. Gli handler girano come TASK GPM,
' quindi fra un'azione e l'altra si usa SLEEP 0 per lasciarli girare (contratto documentato
' in Guide/utente/Finestre-Eventi-Guida-Utente.md).
' Stesso esito su IL, PCodeVM, VM/C e C99Native: l'ultima riga è sempre `events-complete-ok`.

IMPORT WINDOWS

SCREEN 0

DIM openCount = 0
DIM closeCount = 0
DIM minCount = 0
DIM focusCount = 0
DIM wildCount = 0
DIM labelCount = 0
DIM doc = 1                          ' handle della finestra "doc" (OF <espressione>)
DIM logwin = 2

SUB OnOpenDoc()
    openCount = openCount + 1
END SUB

SUB OnCloseDoc()
    closeCount = closeCount + 1
END SUB

SUB OnMinLog()
    minCount = minCount + 1
END SUB

SUB OnFocusDoc()
    focusCount = focusCount + 1
END SUB

SUB OnWildRestore()
    wildCount = wildCount + 1
END SUB

FUNCTION Main() AS INTEGER
    ' 1) Handler registrati PRIMA del DEFINE: OPEN arriva alla finestra appena creata.
    ON WINDOW OPEN OF doc GOSUB OnOpenDoc
    ON WINDOW CLOSE OF doc CALL OnCloseDoc()
    ON WINDOW FOCUS OF doc GOSUB OnFocusDoc
    ON WINDOW MINIMIZE OF logwin GOSUB OnMinLog
    ON WINDOW RESTORE GOSUB OnWildRestore        ' senza OF → ogni finestra
    ON WINDOW CLOSE OF logwin GOTO OnLabelHit    ' GOTO <label locale>

    WINDOW DEFINE doc FROM 0, 0 TO 10, 32 TITLE "doc"
    WINDOW DEFINE log FROM 12, 0 TO 22, 32 TITLE "log"
    SLEEP 0

    ' 2) Ogni azione innalza il suo evento; un SLEEP 0 fra le azioni lascia girare il task.
    '    RESTORE su una finestra NON minimizzata è un no-op (nessun evento): `doc` non lo è.
    WINDOW log MINIMIZE
    SLEEP 0
    WINDOW log RESTORE
    SLEEP 0
    WINDOW doc RESTORE
    SLEEP 0
    WINDOW doc ACTIVATE
    SLEEP 0
    WINDOW log CLOSE
    SLEEP 0

    ' 3) STOP: dopo la deregistrazione l'evento non chiama più l'handler.
    ON WINDOW CLOSE OF logwin STOP
    WINDOW doc CLOSE
    SLEEP 0

    PRINT "open="; openCount; " close="; closeCount; " min="; minCount; " focus="; focusCount
    PRINT "wild="; wildCount; " label="; labelCount

    IF openCount = 1 AND closeCount = 1 AND minCount = 1 AND focusCount = 1 AND wildCount = 1 AND labelCount = 1 THEN
        PRINT "events-complete-ok"
    ELSE
        PRINT "events-complete-no"
    END IF
    RETURN 0

' Handler su LABEL LOCALE della FUNCTION (dopo il RETURN): il compilatore ne emette una
' funzione/metodo sintetico (C99Native: gmb_user_lfnev_*; IL: metodo Proc_<id>).
OnLabelHit:
    labelCount = labelCount + 1
    RETURN
END FUNCTION
