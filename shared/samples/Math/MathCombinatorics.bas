' FILE: Math/MathCombinatorics.bas
' =================================================================================================
'       TEST UNITARIO: MATH - COMBINATORIA E COORDINATE
' =================================================================================================
' Questo test verifica le funzioni matematiche per il calcolo combinatorio e
' le conversioni di coordinate polari/cartesiane.
'
' COPERTURA:
' - Combinatoria: FACT (Fattoriale), PERM (Permutazioni), COMB (Combinazioni).
' - Coordinate: POLAR_X, POLAR_Y (Da Polari a Cartesiane), ANGLE2D.
' - Utilità: CLAMP, HYPOT.
' =================================================================================================


SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ MATH - Combinatoria e Geometria                                        ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. COMBINATORIA ---
    PRINT "--- 1. Calcolo Combinatorio ---"
    
    DIM n = 5
    DIM k = 3
    
    ' Fattoriale: n!
    ' 5! = 120
    PRINT "FACT("; n; "):      "; MATH.FACT(n)
    
    ' Permutazioni: P(n, k) = n! / (n-k)!
    ' P(5, 3) = 120 / 2 = 60
    PRINT "PERM("; n; ", "; k; "):   "; MATH.PERM(n, k)
    
    ' Combinazioni: C(n, k) = n! / (k! * (n-k)!)
    ' C(5, 3) = 120 / (6 * 2) = 10
    PRINT "COMB("; n; ", "; k; "):   "; MATH.COMB(n, k)
    PRINT

    ' --- 2. COORDINATE POLARI ---
    PRINT "--- 2. Conversioni Polari ---"
    
    DIM raggio = 10.0
    DIM angoloDeg = 45.0
    ' Conversione Polare -> Cartesiana
    ' Argomenti: (raggio, angolo in GRADI) — x = r·cos(θ), y = r·sin(θ)
    DIM x = VECTOR.POLAR_X(raggio, angoloDeg)
    DIM y = VECTOR.POLAR_Y(raggio, angoloDeg)
    
    PRINT "Raggio: "; raggio; ", Angolo: "; angoloDeg; "°"
    PRINT "Cartesiane (X, Y): ("; FORMAT(x, "0.00"); ", "; FORMAT(y, "0.00"); ")"
    
    ' Verifica inversa: Angolo tra due punti (ANGLE2D ritorna GRADI)
    ' ANGLE2D(x1, y1, x2, y2)
    DIM calcolatoDeg = VECTOR.Angle2d(0, 0, x, y)
    PRINT "Angolo ricalcolato: "; FORMAT(calcolatoDeg, "0.0"); "°"
    PRINT

    ' --- 3. UTILITÀ MATEMATICHE ---
    PRINT "--- 3. Utilità ---"
    
    ' Ipotenusa (Teorema di Pitagora)
    ' HYPOT(3, 4) = 5
    PRINT "HYPOT(3, 4):        "; MATH.HYPOT(3, 4)
    
    ' Clamp: Restringe un valore in un intervallo
    DIM val = 150
    PRINT "CLAMP(150, 0, 100): "; MATH.CLAMP(val, 0, 100); " (Atteso 100)"
    PRINT "CLAMP(-10, 0, 100): "; MATH.CLAMP(-10, 0, 100); " (Atteso 0)"
    PRINT
    
    PRINT "Test Completato."

END SUB
