' Oracle = golden: ONCE RUN — la SUB di init gira UNA sola volta anche a chiamate ripetute
' (parita' Go sync.Once). Sintassi canone: samples/Community/Concurrency/PCodeGpmOnce.bas
' Verificato 5/5: IL/PCode/VM-C/C99 gate/C99Native -> counter= 1.
' EXPECT: counter= 1
DIM Counter AS LONG = 0

SUB DoInit()
    Counter = Counter + 1
END SUB

SUB Main()
    DIM o AS ONCE
    ONCE RUN o, DoInit
    ONCE RUN o, DoInit
    ONCE RUN o, DoInit
    PRINT "counter="; Counter
END SUB
