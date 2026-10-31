' FILE: Scopes/ScopesGlobal.bas
' Test: GLOBAL keyword — variabili sempre nello scope radice
' Per specifica VARIABLES #3: GLOBAL crea sempre nello Scope Radice (_global),
' anche se usato dentro namespace, procedure o blocchi. È visibile da OVUNQUE.

IMPORT SYSTEM

' GLOBAL a livello di modulo (come DIM ma nello scope radice)
GLOBAL SuperGlobal AS INTEGER = 999

' DIM normale a livello di modulo (scope di namespace)
DIM NamespaceVar AS INTEGER = 111

SUB TestAccesso()
    PRINT "Dentro SUB: SuperGlobal ="; SuperGlobal
    PRINT "Dentro SUB: NamespaceVar ="; NamespaceVar
END SUB

SUB TestModifica()
    ' Assegnazione implicita: modifica la variabile esistente (global o namespace)
    SuperGlobal = SuperGlobal + 1
    NamespaceVar = NamespaceVar + 1
    PRINT "Dopo incremento in SUB: SuperGlobal ="; SuperGlobal
    PRINT "Dopo incremento in SUB: NamespaceVar ="; NamespaceVar
END SUB

SUB TestLetNonModificaGlobale()
    ' LET su nome uguale a globale → NON modifica la globale (crea locale per shadowing)
    LET SuperGlobal = 777
    PRINT "Dentro SUB con LET: SuperGlobal ="; SuperGlobal
END SUB

SUB Main()
    CLS
    PRINT "=== Test GLOBAL Variables ==="
    PRINT
    PRINT "SuperGlobal (GLOBAL, scope radice):"; SuperGlobal
    PRINT "NamespaceVar (DIM module-level, namespace):"; NamespaceVar
    PRINT

    TestAccesso()
    PRINT

    TestModifica()
    PRINT

    TestLetNonModificaGlobale()
    PRINT "Dopo SUB con LET: SuperGlobal ="; SuperGlobal
    PRINT "Dopo SUB con LET: NamespaceVar ="; NamespaceVar
    PRINT

    PRINT "=== Fine test ==="
END SUB
