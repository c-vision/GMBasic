' FILE: Math/MathTrigonometry.bas
' =================================================================================================
'       TEST UNITARIO: MATH - FUNZIONI TRIGONOMETRICHE E IPERBOLICHE
' =================================================================================================
' Questo test verifica il corretto funzionamento delle funzioni trigonometriche,
' delle loro inverse e delle funzioni iperboliche.
'
' COPERTURA:
' - Trigonometriche: SIN, COS, TAN.
' - Inverse: ASIN, ACOS, ATN.
' - Iperboliche: SINH, COSH, TANH (se implementate).
' - Utilità: PI(), DEG2RAD, RAD2DEG.
' =================================================================================================

IMPORT MATH

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ MATH - Funzioni Trigonometriche e Iperboliche                          ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- Valori di test ---
    DIM pi_val = MATH.PI()
    DIM angle_deg = 45.0
    DIM angle_rad = MATH.DEG2RAD(angle_deg)
    
    ' --- 1. FUNZIONI TRIGONOMETRICHE ---
    PRINT "--- 1. Funzioni Trigonometriche (Angolo 45°) ---"
    
    PRINT "Seno (SIN):       "; MATH.SIN(angle_rad)
    PRINT "Coseno (COS):     "; MATH.COS(angle_rad)
    PRINT "Tangente (TAN):   "; MATH.TAN(angle_rad)
    PRINT

    ' --- 2. FUNZIONI TRIGONOMETRICHE INVERSE ---
    PRINT "--- 2. Funzioni Trigonometriche Inverse ---"
    
    ' ATN(1) = 45° in radianti
    PRINT "Arcotangente (ATN) di 1:    "; MATH.ATN(1); " (rad)"
    
    ' ASIN(1) = 90° in radianti
    PRINT "Arcoseno (ASIN) di 1:       "; MATH.ASIN(1); " (rad)"
    
    ' ACOS(0) = 90° in radianti
    PRINT "Arcocoseno (ACOS) di 0:     "; MATH.ACOS(0); " (rad)"
    PRINT

    ' --- 3. CONVERSIONE GRADI/RADIANTI ---
    PRINT "--- 3. Conversione Gradi <-> Radianti ---"
    
    PRINT "180° in Radianti:           "; MATH.DEG2RAD(180)
    PRINT "PI Radianti in Gradi:       "; MATH.RAD2DEG(pi_val)
    PRINT

    ' --- 4. FUNZIONI IPERBOLICHE (se disponibili) ---
    PRINT "--- 4. Funzioni Iperboliche (se implementate) ---"
    
    ' Funzioni iperboliche — implementate nella VM (SINH/COSH/TANH).
    
    PRINT "Seno Iperbolico (SINH) di 1:    "; MATH.SINH(1)
    PRINT "Coseno Iperbolico (COSH) di 1:  "; MATH.COSH(1)
    PRINT "Tangente Iperbolica (TANH) di 1: "; MATH.TANH(1)
    
    PRINT
    
    PRINT "Test Completato."

END SUB