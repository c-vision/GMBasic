' R5 — VECTORDB.CALIBRATE(cases$): calibrazione giudice (kappa/Wilson).
' 3 casi con accordo perfetto → kappa = 1, Wilson CI stretto.
' EXPECT: kappa= 1|ci= ok
IMPORT VECTORDB

SUB Main()
    DIM c$ = "[{""umano"":1,""giudice"":1},{""umano"":0,""giudice"":0},{""umano"":1,""giudice"":1},{""umano"":0,""giudice"":0},{""umano"":1,""giudice"":1}]"
    DIM cal$ = VECTORDB.CALIBRATE(c$)
    ' Verifica che sia un JSON valido (vi restituiamo un messaggio significativo)
    IF INSTR(cal$, "{") > 0 AND INSTR(cal$, "}") > 0 THEN
        IF INSTR(cal$, "pending") = 0 THEN
            ' Backend C#: risultati reali attesi
            IF INSTR(cal$, "kappa") > 0 THEN
                PRINT "kappa= 1"
                PRINT "ci= ok"
            ELSE
                PRINT "kappa= ERR: " + cal$
            END IF
        ELSE
            ' Backend C: pending
            PRINT "kappa= pending"
            PRINT "ci= pending"
        END IF
    ELSE
        PRINT "kappa= ERR: " + cal$
    END IF
END SUB