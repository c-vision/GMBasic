' Letterale dict con chiavi NUMERICHE in un DIM tipizzato (DICT OF INTEGER/LONG/DOUBLE, ...): prima errore di compilazione
' ("DICT OF STRING, STRING" non convertibile in "DICT OF INTEGER, STRING"), perche' il parser tratta `1:` come nome (StringExpr)
' e il Linker non lo riportava a numero. Nei backend C le chiavi restano testo canonico ("1"). In C99Native la seconda chiave
' (`2: "due"`) veniva scambiata per un numero di riga e scartata ("coppie chiave/valore sbilanciate").
' EXPECT: n= 3|u=uno|d=due|t=tre|l= 20
SUB Main()
    DIM k AS DICT OF INTEGER, STRING = {1: "uno", 2: "due", 3: "tre"}
    PRINT "n="; k.Count
    PRINT "u="; k(1)
    PRINT "d="; k(2)
    PRINT "t="; k(3)
    DIM q AS DICT OF LONG, LONG = {10: 20, 30: 40}
    PRINT "l="; q(10)
END SUB
