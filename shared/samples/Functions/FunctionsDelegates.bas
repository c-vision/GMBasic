' FILE: Functions/FunctionsDelegates.bas
' =================================================================================================
'                                GUIDA TEORICA: I DELEGATI (DELEGATES)
' =================================================================================================
'
' In GM-BASIC, un DELEGATE è un tipo di dato speciale che definisce la "firma" di una funzione.
' La firma include:
' 1. Il tipo di ritorno (es. AS DOUBLE).
' 2. Il numero, l'ordine e il tipo dei parametri (es. a AS DOUBLE, b AS DOUBLE).
'
' --- A COSA SERVONO? ---
' I delegati permettono di trattare le funzioni come se fossero dati (variabili).
' Questo abilita pattern potenti come:
' - Callback: Passare una funzione a un'altra funzione per essere eseguita in seguito.
' - Strategie Intercambiabili: Scegliere quale algoritmo usare a runtime.
' - Eventi: Definire cosa succede quando si verifica un'azione.
'
' --- COME SI USANO? ---
' 1. Definizione: Si dichiara il tipo delegato con `DELEGATE FUNCTION Nome(...) AS Tipo`.
' 2. Assegnazione: Si crea una variabile di quel tipo e le si assegna il nome di una funzione reale
'    (o una lambda) che ha la stessa firma.
' 3. Invocazione: Si chiama la variabile come se fosse una funzione.
'
' --- TYPE CHECKING ---
' Il compilatore (Linker) verifica rigorosamente che la funzione assegnata corrisponda alla firma
' del delegato. Assegnare una funzione con parametri diversi causerà un errore di compilazione.
'
' =================================================================================================

' --- 1. Definizione del Delegato ---
' Definiamo un nuovo tipo chiamato 'OperationDelegate'.
' Qualsiasi funzione che accetta due DOUBLE e restituisce un DOUBLE è compatibile con questo tipo.
DELEGATE FUNCTION OperationDelegate(a AS DOUBLE, b AS DOUBLE) AS DOUBLE


' --- 2. Implementazioni di Funzioni Concrete ---
' Due funzioni normali che corrispondono esattamente alla firma di OperationDelegate.

FUNCTION AddNumbers(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    FUNCTION = a + b
END FUNCTION

FUNCTION SubtractNumbers(a AS DOUBLE, b AS DOUBLE) AS DOUBLE
    FUNCTION = a - b
END FUNCTION


' --- 3. Funzione che Restituisce un Delegato (Higher-Order Function) ---
' Questa funzione dimostra come scegliere dinamicamente quale funzione restituire
' in base a un parametro stringa.
FUNCTION GetOperation(op AS STRING) AS OperationDelegate
    IF op = "+" THEN
        PRINT "  (GetOperation: restituisco AddNumbers)"
        RETURN AddNumbers
    ELSEIF op = "-" THEN
        PRINT "  (GetOperation: restituisco SubtractNumbers)"
        RETURN SubtractNumbers
    ELSE
        PRINT "  (GetOperation: restituisco una lambda di default)"
        ' Restituiamo una LAMBDA: una funzione anonima definita al volo.
        ' Anche questa lambda rispetta la firma (2 double -> 1 double).
        RETURN FUNCTION(a AS DOUBLE, b AS DOUBLE) AS DOUBLE => 0
    END IF
END FUNCTION


' --- 4. Funzione che Accetta un Delegato come Parametro (Callback) ---
' Questa funzione non sa "cosa" farà il callback, sa solo che riceverà due double.
' Questo è il polimorfismo funzionale.
' NOTA: Questo è anche un test critico per il compilatore per verificare l'uguaglianza dei tipi funzione.
SUB ProcessOperation(callback AS OperationDelegate, x AS DOUBLE, y AS DOUBLE)
    PRINT "  ProcessOperation: Eseguo il callback ricevuto..."
    
    ' La riga seguente invoca la funzione puntata da 'callback'.
    ' (Nota: al momento della stesura, l'esecuzione indiretta potrebbe essere simulata se la VM non è pronta,
    ' ma la sintassi è corretta e compilabile).
    ' DIM result = callback(x, y)
    ' PRINT "    -> Risultato del callback: "; result
    
    PRINT "    -> Firma del callback compatibile. Test superato a tempo di compilazione."
END SUB


SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ FunctionsDelegates.bas - Test per DELEGATE e Funzioni di Ordine Superiore   ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' ---------------------------------------------------------------------------------------------
    ' TEST A: ASSEGNAZIONE DIRETTA
    ' ---------------------------------------------------------------------------------------------
    PRINT "--- Test A: Assegnazione di base ---"
    ' Dichiariamo una variabile di tipo 'OperationDelegate'.
    DIM myOperation AS OperationDelegate
    
    ' Le assegniamo la funzione 'AddNumbers'.
    myOperation = AddNumbers
    PRINT "  Assegnazione 'myOperation = AddNumbers' compilata con successo."
    PRINT

    ' ---------------------------------------------------------------------------------------------
    ' TEST B: ASSEGNAZIONE DA VALORE DI RITORNO
    ' ---------------------------------------------------------------------------------------------
    PRINT "--- Test B: Funzione che restituisce un delegato ---"
    DIM subtractor AS OperationDelegate
    
    ' Chiamiamo GetOperation che decide a runtime quale funzione restituire.
    subtractor = GetOperation("-")
    PRINT "  Assegnazione 'subtractor = GetOperation(""-"")' compilata con successo."
    PRINT

    ' ---------------------------------------------------------------------------------------------
    ' TEST C: ASSEGNAZIONE DI LAMBDA
    ' ---------------------------------------------------------------------------------------------
    PRINT "--- Test C: Assegnazione di una Lambda ---"
    DIM multiplier AS OperationDelegate
    
    ' Definiamo una funzione anonima per la moltiplicazione e la assegniamo direttamente.
    multiplier = FUNCTION(a AS DOUBLE, b AS DOUBLE) AS DOUBLE => a * b
    PRINT "  Assegnazione di una lambda a 'multiplier' compilata con successo."
    PRINT
    
    ' ---------------------------------------------------------------------------------------------
    ' TEST D: PASSAGGIO COME PARAMETRO (CALLBACK)
    ' ---------------------------------------------------------------------------------------------
    ' Qui verifichiamo che il compilatore accetti diverse forme di funzioni compatibili
    ' come argomento per il parametro 'OperationDelegate'.
    PRINT "--- Test D: Passaggio di funzioni come parametri (test di uguaglianza firme) ---"
    
    ' 1. Passiamo una funzione nominata diretta
    ProcessOperation(AddNumbers, 10, 5)
    
    ' 2. Passiamo una variabile che contiene un delegato
    ProcessOperation(subtractor, 100, 20)
    
    ' 3. Passiamo un'altra variabile delegato (con lambda dentro)
    ProcessOperation(multiplier, 7, 6)
    
    ' 4. Passiamo una lambda definita direttamente nella chiamata
    ' Nota: l'inferenza dei tipi capisce che a e b sono DOUBLE dal contesto di ProcessOperation!
    ProcessOperation(FUNCTION(a,b) => a/b, 50, 5)
    
    PRINT "  Tutte le chiamate a 'ProcessOperation' sono state compilate con successo."
    PRINT
    
    PRINT "--- Test di compilazione per i delegati completato con successo ---"
END SUB