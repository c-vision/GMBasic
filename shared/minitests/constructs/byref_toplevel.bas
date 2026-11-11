' EXPECT: x= 7|y= 0|msg=ciao mondo|pX= 15|pY= 20|nums= 111|ret= 14|n= 14|q= 3
' BYREF in un programma di PRIMO LIVELLO (senza SUB Main): le variabili DIM del corpo
' principale vivono nel bag dei globali, non in un local CIL del metodo. Regressione chiusa
' (2026-11-11): il backend IL passava l'indirizzo di un local sintetico mai collegato alla
' variabile, quindi la callee scriveva su una copia e il chiamante non vedeva nulla
' (divergenza silenziosa dalle altre 3 gambe, corrette). Copre: scalari, più parametri BYREF
' nella stessa call, UDT (value type: scrittura nel box del bag), STRING (reference type:
' temporaneo + write-back dopo la call), array BYREF e call dentro un'espressione.
TYPE Punto
    X AS LONG
    Y AS LONG
END TYPE

SUB Somma(BYREF a AS LONG, BYREF b AS LONG)
    a = a + b
    b = 0
END SUB

SUB Saluta(BYREF s AS STRING)
    s = s + " mondo"
END SUB

SUB Sposta(BYREF p AS Punto, dx AS LONG)
    p.X = p.X + dx
END SUB

SUB Riempi(BYREF arr() AS LONG)
    arr(0) = 111
END SUB

FUNCTION Raddoppia(BYREF n AS LONG) AS LONG
    n = n * 2
    RETURN n
END FUNCTION

DIM x AS LONG, y AS LONG
x = 3 : y = 4
Somma(x, y)
PRINT "x="; x
PRINT "y="; y

DIM msg AS STRING
msg = "ciao"
Saluta(msg)
PRINT "msg="; msg

DIM p AS Punto
p.X = 10 : p.Y = 20
Sposta(p, 5)
PRINT "pX="; p.X
PRINT "pY="; p.Y

DIM nums(2) AS LONG
Riempi(nums())
PRINT "nums="; nums(0)

DIM n AS LONG
n = 7
PRINT "ret="; Raddoppia(n)
PRINT "n="; n

DIM z AS LONG
z = 3
CALL Somma(z, y)
PRINT "q="; z
