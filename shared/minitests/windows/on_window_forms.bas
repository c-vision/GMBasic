' MiniTest: ON WINDOW — forme dell'azione e forme della clausola OF.
' TAG: exitcode — l'output contiene i FRAME TUI (solo VMC li renderizza): la parita' si verifica sull'exit code.
' Copre: CALL sub(), GOSUB sub, GOTO <label locale>, GOSUB <label locale>, STOP (deregistra),
' OF <espressione> (handle calcolato) e forma SENZA OF (wildcard: vale per ogni finestra).
' Gli handler girano come TASK GPM: si osservano i flag dopo un SLEEP 0 (contratto della guida).
' Verifica su exit code; FUNCTION Main ritorna 1 se un assert fallisce.
IMPORT WINDOWS
SCREEN 0
DIM callN = 0
DIM gosubN = 0
DIM gotoN = 0
DIM labelN = 0
DIM wildN = 0
DIM repN = 0
DIM stops = 0
DIM h = 1
DIM fails = 0

SUB OnCall()
    callN = callN + 1
END SUB

SUB OnGosub()
    gosubN = gosubN + 1
END SUB

SUB OnWild()
    wildN = wildN + 1
END SUB

' Due handler per lo stesso (evento, handle): vince l'ultimo registrato.
SUB OnRepA()
    repN = repN + 10
END SUB

SUB OnRepB()
    repN = repN + 1
END SUB

FUNCTION Main() AS INTEGER
    WINDOW DEFINE f1 FROM 0, 0 TO 8, 30 TITLE "f1"
    WINDOW DEFINE f2 FROM 10, 0 TO 18, 30 TITLE "f2"

    ON WINDOW MINIMIZE OF h CALL OnCall()      ' CALL + OF <espressione>
    ON WINDOW MINIMIZE OF 2 GOSUB OnGosub      ' GOSUB <sub>
    ON WINDOW FOCUS OF 2 GOTO OnGotoH           ' GOTO <label locale>: stesso dispatch di GOSUB
    ON WINDOW RESTORE GOSUB OnWild             ' senza OF: wildcard
    ON WINDOW CLOSE OF 1 GOSUB OnLabelH        ' GOSUB <label locale>
    ON WINDOW CLOSE OF 2 GOSUB OnRepA          ' sostituito dalla registrazione successiva
    ON WINDOW CLOSE OF 2 GOSUB OnRepB

    ' SLEEP 0 fra un'azione e l'altra: gli handler sono task GPM e devono poter girare
    ' prima dell'evento successivo (contratto documentato: un evento per volta).
    WINDOW f1 MINIMIZE
    SLEEP 0
    WINDOW f2 MINIMIZE
    SLEEP 0
    WINDOW f1 RESTORE
    SLEEP 0
    WINDOW f2 RESTORE
    SLEEP 0
    WINDOW f2 ACTIVATE
    SLEEP 0
    WINDOW SELECT 0                            ' i PRINT tornano sulla console, non nel buffer

    IF callN = 1 THEN PRINT "call-ok" ELSE fails = fails + 1
    IF gosubN = 1 THEN PRINT "gosub-ok" ELSE fails = fails + 1
    IF wildN = 2 THEN PRINT "wild-ok" ELSE fails = fails + 1
    IF gotoN = 1 THEN PRINT "goto-ok" ELSE fails = fails + 1

    ' STOP: deregistra l'handler dell'evento (non deve più scattare).
    ON WINDOW CLOSE OF 1 STOP
    WINDOW f1 CLOSE
    SLEEP 0
    IF labelN = 0 THEN PRINT "stop-ok" ELSE fails = fails + 1

    ' Una sola registrazione per (evento, handle): eseguito solo l'ultimo handler (repN = 1).
    WINDOW f2 CLOSE
    SLEEP 0
    IF repN = 1 THEN PRINT "replace-ok" ELSE fails = fails + 1

    IF fails = 0 THEN
        PRINT "forms-win-ok"
        RETURN 0
    END IF
    PRINT "forms-win-no"
    RETURN 1

' Handler su LABEL LOCALE della FUNCTION (dopo il RETURN): il compilatore ne emette una
' funzione/metodo sintetico (C99: gmb_user_lfnev_*; IL: metodo sintetico).
OnGotoH:
    gotoN = gotoN + 1
    RETURN

OnLabelH:
    labelN = labelN + 1
    RETURN
END FUNCTION
