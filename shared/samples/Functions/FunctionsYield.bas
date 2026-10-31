' FILE: Functions/FunctionsYield.bas
' =================================================================================================
'       GUIDA TEORICA: GENERATORI, YIELD E LA FUNZIONE NEXT
' =================================================================================================
'
' Questo test introduce il concetto di "Generatori", una speciale tipologia di funzione che
' permette di produrre una sequenza di valori in modo "lazy" (pigro), ovvero uno alla volta
' e solo quando richiesto.
'
' 1. IL `GENERATOR`
'    - Sintassi: `GENERATOR Nome(...) AS TipoValore`
'    - A differenza di una `FUNCTION` normale che calcola e restituisce un singolo valore,
'      un `GENERATOR` restituisce un oggetto "iteratore" (o generatore) quando viene chiamato.
'    - L'esecuzione del corpo del generatore non inizia subito, ma viene sospesa.
'
' 2. L'ISTRUZIONE `YIELD`
'    - `YIELD valore`: È l'istruzione chiave all'interno di un generatore.
'    - Quando l'interprete incontra `YIELD`, "produce" il valore specificato, lo consegna
'      al chiamante e *sospende* l'esecuzione del generatore, mantenendo intatto il suo stato
'      locale (es. il valore della variabile di ciclo `i`).
'
' 3. LA FUNZIONE `NEXT()`
'    - `NEXT(iteratore)`: È la funzione di sistema che "risveglia" un generatore sospeso.
'    - Quando `NEXT` viene chiamato, l'esecuzione del generatore riprende dall'istruzione
'      *successiva* all'ultimo `YIELD` eseguito.
'    - Il ciclo continua finché il generatore non termina (esce dalla funzione) o incontra
'      un altro `YIELD`.
'
' 4. LA FINE DELL'ITERAZIONE (`StopIteration`)
'    - Quando il generatore termina la sua esecuzione (es. la fine di un `FOR` loop),
'      la successiva chiamata a `NEXT` non ha più valori da produrre.
'    - Invece di restituire un valore, lancia un'eccezione speciale con `ERR.Number = 144`
'      (StopIteration).
'    - Questo segnale deve essere catturato con un blocco `TRY...CATCH` per terminare
'      correttamente il ciclo di consumo dei valori.
'
' Questo meccanismo è estremamente efficiente per la memoria, in quanto permette di
' processare sequenze potenzialmente infinite di dati senza doverle mai memorizzare
' completamente in un array o in una lista.
'
' =================================================================================================

' --- Definizione del Generatore ---
' Questo generatore produrrà una sequenza di numeri da 1 a n.
GENERATOR Count(n AS LONG) AS LONG
    DIM i AS LONG
    FOR i = 1 TO n
        ' YIELD "produce" il valore corrente di 'i' e sospende l'esecuzione.
        ' Alla chiamata successiva di NEXT(), l'esecuzione riprenderà da qui (dal NEXT i).
        YIELD i
    NEXT i
    ' Quando il FOR finisce, il generatore termina. La prossima chiamata a NEXT()
    ' lancerà l'errore StopIteration.
END GENERATOR

SUB Main()
    DIM it = Count(3) ' 'it' ora è un oggetto generatore, il codice di Count non è ancora stato eseguito.
    DIM value AS LONG

    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ FunctionsYield - Test Generators, Next() and Yield                          ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    PRINT "Inizio del ciclo del generatore..."
    DO
        TRY
            ' Chiama NEXT() per ottenere il prossimo valore dal generatore 'it'.
            ' La prima volta, l'esecuzione di Count() inizierà da capo.
            ' Le volte successive, riprenderà dopo l'ultimo YIELD.
            value = NEXT(it)
            PRINT "Valore ricevuto:", value
        CATCH e AS ERR WHEN e.Number = 144 ' 144 = StopIteration
            ' Cattura l'eccezione che segnala la fine della sequenza.
            PRINT "Fine del generatore."
            ' Esce dal loop di consumo.
            EXIT DO
        END TRY
    LOOP
END SUB
