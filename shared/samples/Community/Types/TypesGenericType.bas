' FILE: Types/TypesGenericType.bas
' Generic TYPE: TYPE Name<T1, T2> ... END TYPE
' Verifica che TYPE con parametri di tipo compili correttamente, che i campi che
' referenziano T1, T2 siano risolti, e che un'istanziazione concreta (es.
' Box<DOUBLE>, Pair<INTEGER, STRING>) funzioni end-to-end: campi tipizzati,
' assegnazione, lettura e chiamata di un metodo generico monomorfizzato.

IMPORT SYSTEM

CLS
PRINT "=== Generic TYPE<T> ==="
PRINT

TYPE Pair<T1, T2>
    First AS T1
    Second AS T2
END TYPE

TYPE Box<T>
    Value AS T
    FUNCTION GetValue() AS T
        GETVALUE = SELF.Value
    END FUNCTION
END TYPE

PRINT "Generic TYPE definitions compiled OK."
PRINT

PRINT "=== Istanziazione generica: Box<DOUBLE> ==="
DIM b AS Box<DOUBLE>
b.Value = 42.5
PRINT "b.Value = "; b.Value
PRINT "b.GetValue() = "; b.GetValue()
PRINT

PRINT "=== Istanziazione generica: Pair<INTEGER, STRING> ==="
DIM p AS Pair<INTEGER, STRING>
p.First = 7
p.Second = "sette"
PRINT "p.First = "; p.First
PRINT "p.Second = "; p.Second
PRINT

PRINT "=== Fine ==="
END
