' FILE: Stricts/StrictsArrayInitializer.bas
' TEST: Divieto di Conversioni "Narrowing" negli Inizializzatori di Array
' =================================================================
' Con OPTION STRICT ON, ogni elemento in un letterale array (es. { ... })
' deve essere implicitamente convertibile al tipo base dell'array
' senza alcuna perdita di dati (narrowing).
' =================================================================
OPTION STRICT OFF
SUB Main()
    PRINT "Test: Validazione Tipi in Inizializzatori di Array"
    
    ' --- CASI VALIDI ---
    
    ' Assegnazione di interi a un array di double (widening)
    DIM wid_a() AS DOUBLE = {1, 2, 3}
    
    ' Assegnazione di double senza parte frazionaria a un array di integer
    DIM ok_a() AS INTEGER = {10, 20.0, 30}
    
    ' --- CASI DI VIOLAZIONE ---

    ' VIOLATION 1: Tentativo di assegnare un double con parte frazionaria
    ' a un array di integer.
    ' DIM arr1() AS INTEGER = {1, 2, 3.14}
    PRINT "  -> OK se la riga 'DIM arr1() = {1, 2, 3.14}' ha causato un errore."
    
    ' VIOLATION 2: Tentativo di assegnare un numero fuori dall'intervallo
    ' a un array di byte.
    ' DIM arr2() AS BYTE = {10, 20, 300}
    PRINT "  -> OK se la riga 'DIM arr2() = {10, 20, 300}' ha causato un errore."

    ' VIOLATION 3: Tentativo di assegnare una stringa a un array di numeri.
    ' DIM arr3() AS LONG = {100, 200, "300"}
    PRINT "  -> OK se la riga 'DIM arr3() = {100, 200, ""300""}' ha causato un errore."
    PRINT
    
    PRINT "--- Test completato ---"
    PRINT "Se vedi questo messaggio con OPTION STRICT ON, il test è FALLITO."
END SUB
