' EXPECT: C= 10|C dopo= 10|D='CREATO'|D dopo=CREATO|E= 5
' BYREF con CREAZIONE IMPLICITA al call-site (spec §3.2 + il sample ScopesByRef): se l'argomento
' di un parametro BYREF è un nome MAI dichiarato, il linker crea la variabile al volo e la passa
' per riferimento — la scrittura della callee deve tornare al chiamante e la variabile deve
' esistere dopo la chiamata. Prima PCode/VM-C perdevano la scrittura (BYVAL silenzioso): C=0.
SUB Set10(BYREF V)
    V = 10
END SUB

SUB SetStr(BYREF S$)
    S$ = "CREATO"
END SUB

SUB Aggiungi(BYREF N, Delta)
    N = N + Delta
END SUB

SUB Main()
    Set10(C)
    PRINT "C="; C
    PRINT "C dopo="; C

    SetStr(D$)
    PRINT "D='"; D$; "'"
    PRINT "D dopo="; D$

    ' La variabile creata al primo call-site è poi una normale locale: si può riusare.
    Aggiungi(E, 5)
    PRINT "E="; E
END SUB
