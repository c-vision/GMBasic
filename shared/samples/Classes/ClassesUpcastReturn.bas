' FILE: samples/Classes/ClassesUpcastReturn.bas
' =================================================================================================
'   GUIDA TEORICA: Upcast nel RETURN + Dispatch Dinamico su variabili dirette (fix Z1)
' =================================================================================================
' Prima del fix Z1:
'   - assegnare una classe derivata a una variabile del tipo base (o ritornarla da una FUNCTION)
'     falliva: il polimorfismo funzionava SOLO attraverso elementi di ARRAY;
'   - la chiamata di un metodo VIRTUAL su una variabile DIRETTA del tipo base invocava la
'     versione BASE, non l'OVERRIDE (il dispatch dinamico scattava solo sugli array).
'
' Dopo il fix Z1:
'   1. l'UPCAST DERIVATA→BASE nel RETURN di una FUNCTION è supportato:
'        FUNCTION Crea() AS Base
'            Crea = NEW Derivata()
'        END FUNCTION
'   2. il DISPATCH DINAMICO VIRTUAL/OVERRIDE funziona anche su VARIABILI DIRETTE del tipo base
'      (non solo sugli array): una variabile `AS Base` che contiene una derivata invoca
'      l'OVERRIDE, non il metodo base.
'
' Questo sample dimostra entrambi con una gerarchia "Pagamento" e una factory.
' =================================================================================================

IMPORT MATH

' --- Gerarchia: forme di pagamento ---
PUBLIC ABSTRACT CLASS Pagamento
    PUBLIC DIM Importo AS DOUBLE

    PUBLIC SUB NEW(importo AS DOUBLE)
        SELF.Importo = importo
    END SUB

    PUBLIC ABSTRACT FUNCTION Commissione() AS DOUBLE
    PUBLIC VIRTUAL FUNCTION Descrizione() AS STRING
        FUNCTION = "pagamento generico"
    END FUNCTION
END CLASS

PUBLIC CLASS Carta INHERITS Pagamento
    PUBLIC DIM Circuito AS STRING

    PUBLIC SUB NEW(importo AS DOUBLE, circuito AS STRING)
        BASE.NEW(importo)
        SELF.Circuito = circuito
    END SUB

    PUBLIC OVERRIDE FUNCTION Commissione() AS DOUBLE
        FUNCTION = SELF.Importo * 0.02
    END FUNCTION
    PUBLIC OVERRIDE FUNCTION Descrizione() AS STRING
        FUNCTION = "carta " + SELF.Circuito
    END FUNCTION
END CLASS

PUBLIC CLASS PayPal INHERITS Pagamento
    PUBLIC DIM Email AS STRING

    PUBLIC SUB NEW(importo AS DOUBLE, email AS STRING)
        BASE.NEW(importo)
        SELF.Email = email
    END SUB

    PUBLIC OVERRIDE FUNCTION Commissione() AS DOUBLE
        FUNCTION = SELF.Importo * 0.05
    END FUNCTION
    PUBLIC OVERRIDE FUNCTION Descrizione() AS STRING
        FUNCTION = "paypal (" + SELF.Email + ")"
    END FUNCTION
END CLASS

PUBLIC CLASS Contanti INHERITS Pagamento
    PUBLIC SUB NEW(importo AS DOUBLE)
        BASE.NEW(importo)
    END SUB

    PUBLIC OVERRIDE FUNCTION Commissione() AS DOUBLE
        FUNCTION = 0
    END FUNCTION
    PUBLIC OVERRIDE FUNCTION Descrizione() AS STRING
        FUNCTION = "contanti"
    END FUNCTION
END CLASS

' --- FACTORY: upcast derivata→base nel RETURN ---
' La FUNCTION dichiara di ritornare la BASE (Pagamento) ma costruisce e
' restituisce oggetti delle DERIVATE. È l'upcast nel RETURN (fix Z1).
FUNCTION CreaPagamento(tipo AS STRING, importo AS DOUBLE) AS Pagamento
    IF tipo = "carta" THEN
        CreaPagamento = NEW Carta(importo, "Visa")
    ELSEIF tipo = "paypal" THEN
        CreaPagamento = NEW PayPal(importo, "cliente@example.com")
    ELSE
        CreaPagamento = NEW Contanti(importo)
    END IF
END FUNCTION

SUB Main()
    PRINT "== Classi — Upcast nel RETURN + dispatch dinamico su variabili dirette =="
    PRINT

    ' --- Dispatch su VARIABILI DIRETTE (fix Z1) ---
    PRINT ">> Dispatch dinamico su variabili dirette (AS Pagamento)"
    DIM p1 AS Pagamento = CreaPagamento("carta", 100)
    DIM p2 AS Pagamento = CreaPagamento("paypal", 200)
    DIM p3 AS Pagamento = CreaPagamento("contanti", 50)

    PRINT "   "; p1.Descrizione(); " -> commissione "; p1.Commissione()
    PRINT "   "; p2.Descrizione(); " -> commissione "; p2.Commissione()
    PRINT "   "; p3.Descrizione(); " -> commissione "; p3.Commissione()
    PRINT

    ' --- Il caso equivalente con ARRAY (comportamento classico, ancora valido) ---
    PRINT ">> Stesso risultato attraverso un ARRAY di tipo base"
    DIM pagamenti(2) AS Pagamento
    pagamenti(0) = CreaPagamento("carta", 100)
    pagamenti(1) = CreaPagamento("paypal", 200)
    pagamenti(2) = CreaPagamento("contanti", 50)

    DIM i AS LONG
    FOR i = 0 TO 2
        PRINT "   "; pagamenti(i).Descrizione(); " -> commissione "; pagamenti(i).Commissione()
    NEXT i
    PRINT

    ' --- Totale delle commissioni via polimorfismo ---
    DIM totale AS DOUBLE = 0
    FOR i = 0 TO 2
        totale = totale + pagamenti(i).Commissione()
    NEXT i
    PRINT ">> Totale commissioni (polimorfismo su array): "; totale
    PRINT "   Atteso: 2 + 10 + 0 = 12"
END SUB
