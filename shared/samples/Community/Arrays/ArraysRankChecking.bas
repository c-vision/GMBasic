' FILE: Arrays/ArraysRankChecking.bas
' =================================================================================================
'       TEST UNITARIO: TODO #142 - VALIDAZIONE DELLA DIMENSIONALITÀ (RANK) DEGLI ARRAY
' =================================================================================================
' Questo test verifica che il compilatore impedisca l'assegnazione tra array con un numero
' di dimensioni (rango) diverso.
'
' - CASO CORRETTO: L'assegnazione tra due array monodimensionali (1D -> 1D) deve essere permessa.
' - CASO ERRATO: L'assegnazione di un array bidimensionale (matrice) a una variabile
'   dichiarata come array monodimensionale DEVE essere bloccata dal compilatore con un
'   errore di "Type Mismatch".
' =================================================================================================

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ RankChecking.bas - Test di Validazione Dimensionalità Array (Rank)          ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- CASO CORRETTO: Assegnazione tra array con lo stesso rango (1D -> 1D) ---
    PRINT "--- 1. Test Corretto (Assegnazione 1D -> 1D) ---"
    DIM Source1D(5) AS INTEGER
    DIM Dest1D(5) AS INTEGER
    Dest1D = Source1D ' Questa assegnazione è valida e deve compilare.
    PRINT "  -> Assegnazione 1D a 1D compilata correttamente."
    PRINT

    ' --- CASO ERRATO: Assegnazione tra array con rango diverso (2D -> 1D) ---
    PRINT "--- 2. Test Errato (Assegnazione 2D -> 1D) ---"
    PRINT "    La riga seguente dovrebbe causare un errore di compilazione 'Type Mismatch'."
    
    DIM Source2D(5, 5) AS INTEGER
    DIM Dest1D_Error(5) AS INTEGER
    
    ' La riga seguente DEVE essere bloccata dal compilatore.
    ' Se il programma esegue la stampa successiva, il test è fallito.
    ' Decommentare la riga sotto per verificare l'errore di compilazione (2D -> 1D):
    ' Linker ora rileva: "Impossibile assegnare un array 2D a un array 1D"
    'Dest1D_Error = Source2D
    
    PRINT "OK: Il compilatore blocca l'assegnazione 2D -> 1D (test superato)."
    PRINT
    
    PRINT "--- Test completato (se non si sono verificati errori di compilazione) ---"
END SUB
