' EXPECT: s= 0.5|b= 0|b2=-1|b3= 0|l= 99|f= 0.5|f2= 12
' BYREF e LARGHEZZA DELLO STORAGE (2026-11-11): il tipo logico del parametro non coincide sempre
' con la larghezza della cella puntata. Copre i tre casi che divergevano fra le gambe:
'  1. `BYREF v AS SINGLE` su una VARIABILE: lo storage del dialetto è DOUBLE (8 byte) — uno store
'     a 4 byte lasciava il valore vecchio nella metà alta (il chiamante leggeva 1.25000023469329).
'  2. `BYREF v AS BIT`: lo storage è INT32 (4 byte), il tipo logico è 1 byte — uno store a 1 byte
'     lasciava i 3 byte alti sporchi (`v = FALSE` su un TRUE dava -256).
'  3. `BYREF` di un ELEMENTO di array: va passato l'indirizzo DELL'ELEMENTO (con l'indice
'     corretto) e non un temporaneo — per un array SINGLE (elementi a 4 byte) il valore va
'     comunque convertito, altrimenti si scrive a 8 byte sull'elemento sbagliato.
SUB AzzeraS(BYREF v AS SINGLE)
    v = 0.5
END SUB

SUB Spegni(BYREF v AS BIT)
    v = FALSE
END SUB

SUB Accendi(BYREF v AS BIT)
    v = TRUE
END SUB

SUB SetL(BYREF v AS LONG)
    v = 99
END SUB

SUB SetF(BYREF v AS SINGLE)
    v = 0.5
END SUB

SUB Main()
    DIM s AS SINGLE
    s = 1.25
    AzzeraS(s)
    PRINT "s="; s

    DIM b AS BIT
    b = TRUE
    Spegni(b)
    PRINT "b="; b
    Accendi(b)
    PRINT "b2="; b
    Spegni(b)
    PRINT "b3="; b

    DIM nums(3) AS LONG
    nums(1) = 11 : nums(2) = 12
    SetL(nums(1))
    PRINT "l="; nums(1)

    DIM f(3) AS SINGLE
    f(1) = 1.25 : f(2) = 12
    SetF(f(1))
    PRINT "f="; f(1)
    PRINT "f2="; f(2)
END SUB
