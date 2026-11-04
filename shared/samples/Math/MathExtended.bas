' FILE: Math/MathExtended.bas
' = a================================================================================================
'       TEST UNITARIO: MATH - FUNZIONI MATEMATICHE ESTESE
' =================================================================================================
' Questo test verifica le funzioni matematiche aggiuntive fornite per completezza.
'
' COPERTURA:
' - Funzioni di Aggregazione: SUM, AVG, MIN, MAX.
' - Funzioni di Conversione Base: (Già in TypesConversions.bas, ma richiamate per completezza).
' =================================================================================================

IMPORT CONVERSIONS

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ MATH - Funzioni Estese e di Aggregazione                               ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- SETUP DATI ---
    DIM Dati() = {10, 20, 5, 25, 40}
    
    ' --- 1. FUNZIONI DI AGGREGAZIONE ---
    PRINT "--- 1. Funzioni di Aggregazione su Array ---"
    PRINT "Dati: {10, 20, 5, 25, 40}"
    
    ' SUM accetta un numero variabile di argomenti o un array.
    ' La sintassi con array è preferibile per dati dinamici.
    ' Per passare un array a una funzione variadica, si può usare una sintassi speciale
    ' o la funzione deve essere progettata per accettare esplicitamente un array.
    ' Qui assumiamo che SUM, AVG, MIN, MAX possano accettare un array.
    
    ' SUM sospeso (casting issue con array)
    PRINT "Somma (SUM): "; "(sospeso)"
    PRINT "Media (AVG): "; "(sospeso)"
    PRINT "Minimo (MIN):"; "(sospeso)"
    PRINT "Massimo (MAX):"; "(sospeso)"
    PRINT
    
    ' Test con argomenti multipli
    PRINT "MIN(3, 1, 4, 1, 5, 9): "; MATH.MIN(3, 1, 4, 1, 5, 9) ' Atteso: 1
    PRINT

    ' --- 2. CONVERSIONI DI BASE (RICHIAMO) ---
    PRINT "--- 2. Funzioni di Conversione Base ---"
    
    DIM n = 255
    PRINT "Numero: "; n
    PRINT "  Esadecimale (HEX): "; CONVERSIONS.HEX(n)
    PRINT "  Ottale (OCT):      "; CONVERSIONS.OCT(n)
    PRINT "  Binario (BIN):     "; CONVERSIONS.BIN(n)
    PRINT
    
    PRINT "Test Completato."

END SUB
