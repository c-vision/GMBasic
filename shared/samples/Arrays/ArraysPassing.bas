' FILE: Arrays/ArrayPassing.bas
' =================================================================================================
'                                GUIDA TEORICA: PASSAGGIO DI ARRAY AI SOTTOPROGRAMMI
' =================================================================================================
'
' In GM-BASIC, gli array possono essere passati a SUB e FUNCTION come argomenti.
'
' 1. SINTASSI DEL PARAMETRO
'    Nella definizione della procedura, il parametro array deve essere dichiarato con le parentesi vuote `()`
'    per indicare che si aspetta un intero array e non un singolo elemento scalare.
'    Es: `SUB Processa(Arr() AS INTEGER)`
'
' 2. PASSAGGIO PER RIFERIMENTO (BYREF)
'    Gli array sono tipi di riferimento. Quando vengono passati a una procedura, viene passato il riferimento
'    alla struttura dati in memoria, non una copia.
'    - La keyword `BYREF` è implicita per gli array, ma è buona norma specificarla per chiarezza.
'    - Qualsiasi modifica agli elementi dell'array all'interno della procedura si riflette sull'array originale.
'
' 3. SINTASSI DELLA CHIAMATA
'    Quando si chiama la procedura, si può passare l'array in due modi:
'    - Solo nome: `Processa(MioArray)` (Stile moderno/VB.NET)
'    - Nome con parentesi: `Processa(MioArray())` (Stile BASIC classico/QBasic).
'      Questa seconda sintassi è ambigua per il parser (sembra una chiamata a funzione senza argomenti o un
'      accesso a un elemento), ma il compilatore la risolve semanticamente sapendo che `MioArray` è un array.
'
' =================================================================================================

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ArrayPassing - Verifica Regressione Passaggio Array con ()                  ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' Dichiariamo e popoliamo un array locale
    DIM Numeri(2) AS INTEGER
    Numeri(0) = 10
    Numeri(1) = 20
    Numeri(2) = 30
    
    PRINT "--- Test 1: Passaggio di un array con la sintassi NOME() ---"
    PRINT "Array prima della chiamata: "; Numeri(0); Numeri(1); Numeri(2)
    
    ' TEST CRITICO: Passaggio con parentesi vuote `Numeri()`.
    ' Il parser vede inizialmente `Numeri()` come un possibile accesso o chiamata.
    ' Il Linker deve risolvere `Numeri` come variabile array e passare il riferimento completo
    ' alla SUB `ModificaArray` che si aspetta un array BYREF.
    ModificaArray(Numeri())
    
    PRINT "Array dopo la chiamata: "; Numeri(0); Numeri(1); Numeri(2)
    
    ' Verifica che le modifiche fatte dalla SUB siano visibili qui (passaggio per riferimento)
    IF Numeri(0) = 100 AND Numeri(1) = 200 AND Numeri(2) = 300 THEN
        PRINT "RISULTATO: SUCCESSO! L'array è stato modificato correttamente."
    ELSE
        PRINT "RISULTATO: FALLIMENTO! L'array non è stato modificato."
    END IF
    PRINT
    
    PRINT "--- Test 2: Chiamata a SUB senza argomenti per confronto ---"
    ' Test di controllo per assicurarsi che il parser distingua ancora correttamente
    ' una normale chiamata a procedura senza argomenti.
    SubSenzaArgomenti()
    
    PRINT
    PRINT "--- Test completato ---"

END SUB

' Questa SUB si aspetta un intero array di INTERI passato per riferimento.
' Modificherà l'array originale.
SUB ModificaArray(BYREF Arr() AS INTEGER)
    PRINT "  -> Dentro ModificaArray. Modifico l'array..."
    ' Modifica gli elementi dell'array passato.
    Arr(0) = 100
    Arr(1) = 200
    Arr(2) = 300
END SUB

' Questa SUB viene usata per verificare che il parser distingua
' correttamente tra una chiamata a SUB senza argomenti e il passaggio di un array.
SUB SubSenzaArgomenti()
    PRINT "  -> Chiamata a 'SubSenzaArgomenti' eseguita con successo."
END SUB