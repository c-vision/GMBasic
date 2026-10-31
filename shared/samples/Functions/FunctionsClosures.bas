' FILE: Functions/FunctionsClosures.bas
' ======================================================================================================================
' TEST DELLE CLOSURE E DELLA CATTURA DI VARIABILI
' ======================================================================================================================
' Questo file verifica che le funzioni locali e le lambda possano "catturare"
' variabili dallo scope in cui sono definite e che mantengano lo stato
' tra le chiamate (Heap Promotion).
' ======================================================================================================================

IMPORT SYSTEM

' Funzione che restituisce un'altra funzione (un contatore).
' La variabile 'count' deve essere catturata dalla lambda restituita.
FUNCTION MakeCounter() AS FUNCTION () AS LONG
    DIM count = 0
    ' Restituisce una lambda che incrementa e restituisce 'count'.
    ' Il compilatore deve promuovere 'count' in un oggetto heap (closure).
    FUNCTION = FN() AS LONG => count := count + 1
END FUNCTION

SUB Main()
    PRINT "╔═════════════════════════════════════════╗"
    PRINT "║      Test di Funzionalità Closure       ║"
    PRINT "╚═════════════════════════════════════════╝"
    PRINT

    ' ======================================================
    ' 1. Test di base: Creazione e Invocazione
    ' ======================================================
    PRINT "--- 1. Creazione e Invocazione di una Closure ---"
    
    ' Crea la prima istanza del contatore.
    DIM c1 = MakeCounter()
    PRINT "Contatore 'c1' creato."
    
    PRINT "Chiamata 1 a c1(): "; c1(); " (Atteso: 1)"
    PRINT "Chiamata 2 a c1(): "; c1(); " (Atteso: 2)"
    PRINT "Chiamata 3 a c1(): "; c1(); " (Atteso: 3)"
    PRINT

    ' ======================================================
    ' 2. Test di Isolamento: Istanze multiple
    ' ======================================================
    PRINT "--- 2. Test di Isolamento (Istanze Multiple) ---"
    
    ' Crea una seconda istanza del contatore.
    ' Il suo stato deve essere indipendente da 'c1'.
    DIM c2 = MakeCounter()
    PRINT "Contatore 'c2' creato."

    PRINT "Chiamata 1 a c2(): "; c2(); " (Atteso: 1)"
    PRINT "Verifica stato di c1: "; c1(); " (Atteso: 4, deve continuare da dove era rimasto)"
    PRINT "Chiamata 2 a c2(): "; c2(); " (Atteso: 2)"
    PRINT
    
    PRINT "Test Closure completato con successo."

END SUB