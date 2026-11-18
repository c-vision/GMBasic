' Oracle = golden: GO SELECT con rendezvous (ricezione bloccante da canale).
' Senza CASE DEFAULT il select parcheggia finche' il produttore non invia (semantica Go).
' Sintassi canone: samples/Concurrency/GoSelectSimple.bas
' Verificato 5/5 (IL/PCode/VM-C/C99 gate/C99Native — il valore garbage C99Native e'
' stato corretto da sessioni successive).
' EXPECT: ricevuto= 42|fine

SUB Produttore(C AS CHANNEL OF INTEGER, WG AS WAITGROUP)
    SEND 42 TO C
    DONE WG
END SUB

SUB Main()
    DIM C AS CHANNEL OF INTEGER
    DIM WG AS WAITGROUP
    GO SUB Produttore(C, WG) IN WG

    DIM Valore AS INTEGER
    GO SELECT
        CASE RECEIVE Valore FROM C:
            PRINT "ricevuto="; Valore
    END SELECT

    WAIT WG
    PRINT "fine"
END SUB
