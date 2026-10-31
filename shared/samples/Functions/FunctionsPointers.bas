' FILE: Functions/FunctionPointers.bas
' =================================================================================================
'       GUIDA TEORICA: PUNTATORI A FUNZIONE E DELEGATI
' =================================================================================================
'
' Questo test esplora il concetto di "puntatori a funzione" (o delegati), che permette di
' trattare le procedure (`SUB` e `FUNCTION`) come dati: possono essere passate come argomenti
' ad altre procedure, restituite come risultati e assegnate a variabili.
'
' 1. TIPI DI FUNZIONE
'    La firma di una procedura (nome, parametri, tipo di ritorno) definisce il suo "tipo".
'    In GM-BASIC, questo può essere specificato esplicitamente.
'    - `AS SUB(...)`: Rappresenta una procedura che non restituisce un valore.
'    - `AS FUNCTION(...) AS TIPO`: Rappresenta una funzione che restituisce un valore.
'    - `AS FN(...)`: Un alias più breve per `AS FUNCTION`.
'
' 2. PASSAGGIO DI FUNZIONI COME PARAMETRI (CALLBACK)
'    Una funzione può accettare un'altra funzione come argomento. Questo è il pattern "callback",
'    fondamentale per la programmazione a eventi e asincrona.
'    - `SUB EseguiCallback(callback AS SUB(val AS INTEGER))`: `EseguiCallback` accetta
'      qualsiasi `SUB` che, a sua volta, accetti un singolo `INTEGER`.
'
' 3. OTTENERE UN RIFERIMENTO A UNA FUNZIONE
'    Per passare una funzione come argomento, non si chiama la funzione, ma si passa un
'    riferimento ad essa.
'    - Nome Diretto: Se il nome non è ambiguo, GM-BASIC capisce che `EseguiCallback(MioCallback)`
'      sta passando un riferimento a `MioCallback`, non il risultato della sua chiamata.
'      L'operatore `@` (`@MioCallback`) può essere usato per essere espliciti, ma spesso non è necessario.
'
' 4. FUNZIONI ANONIME (LAMBDA) COME CALLBACK
'    Le funzioni lambda sono perfette per essere usate come callback, in quanto possono essere
'    definite "al volo" direttamente nella chiamata della funzione di ordine superiore.
'    - `EseguiCallback(SUB(x AS INTEGER): ... END SUB)`: Una `SUB` anonima viene creata
'      e passata come argomento. Il compilatore ne verifica la compatibilità della firma.
'
' 5. SFIDA PER IL COMPILATORE
'    Il compito del Linker è cruciale: deve verificare che il tipo della funzione passata
'    (sia essa una `SUB` nominata o una lambda) sia *semanticamente equivalente* al tipo
'    richiesto dal parametro della funzione ricevente. Questo richiede un confronto profondo
'    delle firme (numero, tipo e ordine dei parametri, e tipo di ritorno).
'
' =================================================================================================

' --- Funzione Target di Ordine Superiore ---
' Accetta un'altra procedura (un "callback") come parametro.
' Il tipo del parametro è definito come "una SUB che accetta un INTEGER".
SUB EseguiCallback(callback AS SUB(val AS INTEGER))
    PRINT "Eseguo il callback con il valore 100..."
    ' La chiamata vera e propria (`callback(100)`) sarà gestita dall'Executor.
    ' Per ora, il test si concentra sulla validazione a tempo di compilazione.
    ' callback(100) 
    PRINT "Callback eseguito (compilazione OK)."
    PRINT
END SUB

' --- Implementazione del Callback ---
' Una SUB con una firma compatibile: accetta un singolo INTEGER.
SUB MioCallback(numero AS INTEGER)
    PRINT "  [MioCallback] Ricevuto valore: "; numero
END SUB


SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TestFunctionPointers.bas - Verifica Equality di FunctionPointerType         ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' --- Test 1: Chiamata con Riferimento a Funzione Nominata ---
    ' Il compilatore deve verificare che la firma di 'MioCallback'
    ' sia compatibile con il tipo del parametro `callback` di 'EseguiCallback'.
    PRINT "--- Test 1: Chiamata con riferimento a una SUB esistente ---"
    EseguiCallback(MioCallback)

    ' --- Test 2: Chiamata con una Lambda ---
    ' Il compilatore deve inferire il tipo della lambda (una SUB che accetta un INTEGER)
    ' e verificare che sia compatibile con il parametro di 'EseguiCallback'.
    PRINT "--- Test 2: Chiamata con una lambda (funzione anonima) ---"
    EseguiCallback(SUB(x AS INTEGER):
        PRINT "  [Lambda] Ricevuto valore: "; x
    END SUB)
    
    PRINT "--- Test completato con successo ---"
END SUB