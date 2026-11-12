' MiniTest: ON WINDOW <EVENTO> OF <handle> — l'evento arriva SOLO alla finestra indicata.
' TAG: exitcode — l'output contiene i FRAME TUI (solo VMC li renderizza): la parita' si verifica sull'exit code.
' Due finestre (handle 1 e 2). Si registrano: MINIMIZE per handle 1, MINIMIZE per handle 2,
' MINIMIZE senza OF (wildcard) e FOCUS per handle 1. Poi si minimizza/attiva SOLO la finestra 2:
'   - l'handler di handle 2 scatta (b=1) e il wildcard pure (anyr=1);
'   - l'handler di handle 1 NON deve scattare (a=0) e nemmeno FOCUS di handle 1 (f1=0).
' Se OF fosse ignorato, a e f1 diventerebbero 1 → FAIL.
'
' Gli handler scrivono FLAG (0/1), non contatori: su IL/PCode il dispatch è un task GPM e due
' incrementi consecutivi sulla stessa variabile possono sovrapporsi (lost update), mentre un
' flag idempotente resta deterministico.
'
' Niente riga EXPECT: creazione/render delle finestre scrivono frame sulla console e l'output
' completo non è confrontabile fra backend. La verifica è l'EXIT CODE (FUNCTION Main AS INTEGER:
' 1 = FAIL), come per tui/window_parity.bas.
IMPORT WINDOWS
SCREEN 0
DIM a = 0
DIM b = 0
DIM anyr = 0
DIM f1 = 0
DIM fails = 0

SUB OnMin1()
    a = 1
END SUB

SUB OnMin2()
    b = 1
END SUB

SUB OnFocus1()
    f1 = 1
END SUB

FUNCTION Main() AS INTEGER
    WINDOW DEFINE w1 FROM 0, 0 TO 10, 30 TITLE "w1"
    WINDOW DEFINE w2 FROM 12, 0 TO 22, 30 TITLE "w2"

    ON WINDOW MINIMIZE OF 1 GOSUB OnMin1
    ON WINDOW MINIMIZE OF 2 GOSUB OnMin2
    ON WINDOW MINIMIZE GOSUB AnyMin          ' senza OF + LABEL LOCALE (non SUB): vale per OGNI finestra
    ON WINDOW FOCUS OF 1 GOSUB OnFocus1

    WINDOW w2 MINIMIZE                       ' solo handle 2
    WINDOW w2 ACTIVATE                       ' FOCUS di handle 2, non di 1

    ' Fuori dalle finestre: i PRINT mentre una finestra è attiva finiscono nel suo buffer.
    WINDOW SELECT 0
    PRINT "flags:"; a; b; anyr; f1
    IF a <> 0 THEN fails = fails + 1         ' handle 1 non deve aver visto eventi di w2
    IF b <> 1 THEN fails = fails + 1
    IF anyr <> 1 THEN fails = fails + 1      ' wildcard: una volta
    IF f1 <> 0 THEN fails = fails + 1
    IF fails = 0 THEN
        PRINT "win-ok"
        RETURN 0
    END IF
    PRINT "win-no"
    RETURN 1

' Handler evento su LABEL LOCALE (dentro la FUNCTION, dopo il RETURN): nessuna gamba può
' saltarci dentro, quindi il compilatore emette una funzione/metodo sintetico (C99:
' gmb_user_lfnev_*, IL: Proc_<id> sintetico, P-Code/VM-C: salto all'indirizzo).
AnyMin:
    anyr = 1
    RETURN
END FUNCTION
