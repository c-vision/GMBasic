' FOR EACH su matrice: destructuring della riga (N variabili = N colonne).
' Nota: il backend IL non supporta ancora 3+ variabili (NotSupported → test_il SKIP);
' il P-Code VM sì. Questo sample copre la sintassi in entrambi i backend.
SUB Main()
    DIM m(2, 2) AS INTEGER
    m(0,0)=1 : m(0,1)=2 : m(0,2)=3
    m(1,0)=4 : m(1,1)=5 : m(1,2)=6
    m(2,0)=7 : m(2,1)=8 : m(2,2)=9
    DIM a AS INTEGER, b AS INTEGER, c AS INTEGER
    FOR EACH a, b, c IN m
        PRINT a; ","; b; ","; c
    NEXT
END SUB
