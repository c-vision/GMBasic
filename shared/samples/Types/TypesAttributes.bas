' FILE: Types/TypesAttributes.bas
' =================================================================================================
'       TEST: Attributes per GM-Basic
' =================================================================================================

IMPORT SYSTEM

10 CLS
20 PRINT "=== Attributes Test ==="
30 PRINT

GOSUB TestClassAttributes
GOSUB TestFieldAttributes
GOSUB TestFunctionAttributes

90 PRINT
95 PRINT "=== Tutti i test attributes completati ==="
100 END

' -------------------------------------------------------------------------
' 1. <Serializable> su classe
' -------------------------------------------------------------------------
1000 TestClassAttributes:
1010 PRINT "--- 1. <Serializable> su classe ---"
1020 DIM p AS Persona
1025 p = NEW Persona()
1030 p.Nome = "Mario"
1040 p.Eta = 30
1050 PRINT "  - Persona created: "; p.Nome; ", "; p.Eta
1060 RETURN

' -------------------------------------------------------------------------
' 2. <Required> su campo
' -------------------------------------------------------------------------
2000 TestFieldAttributes:
2010 PRINT "--- 2. <Required> su campo ---"
2020 DIM a AS Indirizzo
2025 a = NEW Indirizzo()
2030 a.Via = "Roma 1"
2040 PRINT "  - Indirizzo created: "; a.Via
2050 RETURN

' -------------------------------------------------------------------------
' 3. Attributes su funzione
' -------------------------------------------------------------------------
3000 TestFunctionAttributes:
3010 PRINT "--- 3. <Serializable> su FUNCTION ---"
3020 DIM result = Somma(10, 20)
3030 PRINT "  - Somma(10, 20) ="; result
3040 RETURN

' -------------------------------------------------------------------------
' Classi e funzioni con attributes
' -------------------------------------------------------------------------
<Serializable>
CLASS Persona
    PUBLIC DIM Nome AS STRING
    PUBLIC DIM Eta AS INTEGER
END CLASS

<Serializable>
<Required>
CLASS Indirizzo
    PUBLIC DIM Via AS STRING
    PUBLIC DIM Citta AS STRING
END CLASS

<Serializable>
FUNCTION Somma(BYVAL a AS INTEGER, BYVAL b AS INTEGER) AS INTEGER
    RETURN a + b
END FUNCTION
