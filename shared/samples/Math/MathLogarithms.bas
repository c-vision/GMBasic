' FILE: Math/MathLogarithms.bas
' =================================================================================================
'       TEST UNITARIO: MATH - FUNZIONI LOGARITMICHE ED ESPONENZIALI
' =================================================================================================
' Questo test verifica il corretto funzionamento delle funzioni matematiche di base per
' logaritmi, esponenziali, radici e arrotondamento.
'
' COPERTURA:
' - LOG: Logaritmo naturale (base 'e').
' - LOG10: Logaritmo in base 10.
' - EXP: Esponenziale (e^x).
' - SQR: Radice quadrata.
' - Arrotondamento: ROUND, TRUNC, FIX, INT, FLOOR, CEILING.
' =================================================================================================

IMPORT CONVERSIONS

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ MATH - Logaritmi, Esponenziali e Arrotondamento                        ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. LOGARITMI ED ESPONENZIALI ---
    PRINT "--- 1. Funzioni Logaritmiche ed Esponenziali ---"
    
    ' EXP(1) è la costante di Nepero, 'e'
    DIM e = MATH.EXP(1)
    PRINT "Valore di 'e' (EXP(1)):     "; e
    
    ' LOG è il logaritmo naturale (base 'e')
    PRINT "LOG(e):                     "; MATH.LOG(e); " (Atteso: 1)"
    
    ' LOG10 è il logaritmo in base 10
    PRINT "LOG10(1000):                "; MATH.LOG10(1000); " (Atteso: 3)"
    PRINT

    ' --- 2. RADICE QUADRATA ---
    PRINT "--- 2. Radice Quadrata (SQR) ---"
    
    PRINT "SQR(25):                    "; MATH.SQR(25); " (Atteso: 5)"
    PRINT "SQR(2):                     "; MATH.SQR(2)
    PRINT

    ' --- 3. ARROTONDAMENTO E TRONCAMENTO ---
    PRINT "--- 3. Funzioni di Arrotondamento ---"
    
    DIM num_pos = 123.567
    DIM num_neg = -123.567
    
    PRINT "Numero Positivo:", num_pos
    PRINT "  ROUND(num, 2): "; MATH.ROUND(num_pos, 2)
    PRINT "  TRUNC(num, 2): "; MATH.TRUNC(num_pos, 2)
    PRINT "  FIX(num):      "; MATH.FIX(num_pos)
    PRINT "  INT(num):      "; MATH.INT(num_pos)
    PRINT "  FLOOR(num):    "; MATH.FLOOR(num_pos)
    PRINT "  CEILING(num):  "; MATH.CEILING(num_pos)
    PRINT
    
    PRINT "Numero Negativo:", num_neg
    PRINT "  ROUND(num, 2): "; MATH.ROUND(num_neg, 2)
    PRINT "  TRUNC(num, 2): "; MATH.TRUNC(num_neg, 2)
    PRINT "  FIX(num):      "; MATH.FIX(num_neg)
    PRINT "  INT(num):      "; MATH.INT(num_neg); " (Diverso da FIX)"
    PRINT "  FLOOR(num):    "; MATH.FLOOR(num_neg)
    PRINT "  CEILING(num):  "; MATH.CEILING(num_neg)
    PRINT

    ' --- 4. FUNZIONI DI SEGNO E VALORE ASSOLUTO ---
    PRINT "--- 4. Segno e Valore Assoluto ---"
    
    PRINT "ABS(-10.5): "; MATH.ABS(-10.5)
    PRINT "SGN(-10.5): "; MATH.SGN(-10.5)
    PRINT "SGN(0):     "; MATH.SGN(0)
    PRINT "SGN(10.5):  "; MATH.SGN(10.5)
    
    PRINT
    PRINT "Test Completato."

END SUB
