' FILE: Functions/FunctionsLambda.bas
' =================================================================================================
'       GUIDA TEORICA: FUNZIONI ANONIME (LAMBDA) E FUNZIONI DI ORDINE SUPERIORE
' =================================================================================================
'
' Questo test esplora una delle funzionalità più avanzate e moderne di GM-BASIC: le funzioni anonime,
' comunemente note come "lambda expressions".
'
' 1. COS'È UNA FUNZIONE LAMBDA?
'    Una funzione lambda è una funzione senza un nome, definita "al volo" direttamente dove serve.
'    È un costrutto potente che permette di scrivere codice più conciso e in stile funzionale.
'
' 2. SINTASSI IN GM-BASIC
'    GM-BASIC offre due sintassi per le lambda, che devono SEMPRE essere parte di un'espressione
'    (tipicamente un'assegnazione o il passaggio come argomento).
'
'    A. Corpo a Espressione (`=>`)
'       - Sintassi: `FN(param1 AS TIPO, ...) AS TIPO_RITORNO => espressione`
'       - Uso: Per funzioni che consistono in una singola espressione di ritorno.
'       - Esempio: `FN(x AS INT) AS INT => x * 2`
'
'    B. Corpo a Blocco (`{...}` o `: ... END FN`)
'       - Sintassi: `FN(...) AS ... { ... }`  o  `FN(...) AS ...: ... END FN`
'       - Uso: Per funzioni più complesse che richiedono più istruzioni. Il blocco deve
'         seguire immediatamente la firma della lambda sulla stessa riga logica.
'       - Esempio (con `:`):
'         `FN(s AS STRING) AS STRING:
'              DIM r$ = REVERSE(s)
'              RETURN r$
'          END FN`
'       - Esempio (con `{}`):
'         `FN(s AS STRING) AS STRING {
'              DIM r$ = REVERSE(s)
'              RETURN r$
'          }`
'
' 3. FUNZIONI DI ORDINE SUPERIORE
'    Una funzione di ordine superiore è una funzione che accetta un'altra funzione come argomento.
'    Le lambda sono perfette per essere passate a queste funzioni (es. `AFILTER`, `AMAP`).
'
' 4. DELEGATI E TIPI FUNZIONE
'    Le lambda vengono trattate come "puntatori a funzione" o "delegati". Possono essere
'    assegnate a variabili dichiarate con il tipo `FUNCTION`.
'    - `DIM myFunc AS FUNCTION(INTEGER) AS INTEGER`: Dichiara una variabile che può
'      contenere una funzione che accetta un intero e restituisce un intero.
'
' =================================================================================================

IMPORT ARRAYS
IMPORT JSON

' --- Funzione di Ordine Superiore Definita dall'Utente ---
' Questa funzione accetta un array e un'altra funzione (il "predicato") come argomenti.
FUNCTION CountIf(arr() AS DOUBLE, predicate AS FUNCTION(DOUBLE) AS BIT) AS INTEGER
    DIM counter = 0
    FOR EACH x IN arr()
        IF predicate(x) THEN
            counter = counter + 1
        END IF
    NEXT
    CountIf = counter
END FUNCTION

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════╗"
    PRINT "║ FunctionsLambda - Funzioni Anonime (Lambda) e Ordine Superiore ║"
    PRINT "╚════════════════════════════════════════════════════════════════╝"
    PRINT
    
    DIM numeri = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    
    ' --- 1. Lambda in Funzioni Built-in (AFILTER, AMAP) ---
    PRINT "--- 1. Lambda in Funzioni Built-in (AFILTER, AMAP) ---"
    
    ' AFILTER: usa una lambda a corpo di espressione per filtrare i numeri pari.
    DIM pari = AFILTER(numeri, FN(n AS DOUBLE) AS BIT => (n MOD 2) = 0)
    PRINT "Numeri Pari (AFILTER): "; JSON.Stringify(pari)
    
    ' AMAP: usa una lambda per trasformare ogni numero nel suo quadrato.
    DIM quadrati = AMAP(numeri, FN(n AS DOUBLE) AS DOUBLE => n * n)
    PRINT "Quadrati (AMAP): "; JSON.Stringify(quadrati)
    PRINT
    
    ' --- 2. Lambda assegnata a una Variabile ---
    PRINT "--- 2. Lambda assegnata a una Variabile ---"
    ' Dichiara una variabile 'moltiplica' che può contenere un puntatore a funzione.
    DIM moltiplica AS FUNCTION(DOUBLE, DOUBLE) AS DOUBLE
    ' Assegna una lambda alla variabile.
    moltiplica = FN(a AS DOUBLE, b AS DOUBLE) AS DOUBLE => a * b
    ' Invoca la funzione tramite la variabile.
    PRINT "Risultato di moltiplica(5, 4):"; moltiplica(5, 4)
    PRINT
    
    ' --- 3. Lambda in una Funzione di Ordine Superiore Utente ---
    PRINT "--- 3. Lambda in una Funzione Utente (CountIf) ---"
    ' Passa una lambda alla funzione CountIf per contare i numeri maggiori di 5.
    DIM maggioriDi5 = CountIf(numeri, FN(x AS DOUBLE) AS BIT => x > 5)
    PRINT "Numeri maggiori di 5: "; maggioriDi5
    PRINT
    
    ' --- 4. Lambda con Corpo a Blocco ---
    PRINT "--- 4. Lambda con Corpo a Blocco ---"
    
    ' Definisce una lambda più complessa. L'assegnazione e la definizione devono essere sulla stessa riga logica.
    ' La sintassi corretta è: `variabile = FN(...) : corpo END FN`
    DIM saluta AS FUNCTION(nome AS STRING, ora AS INTEGER) AS STRING = FN(nome AS STRING, ora AS INTEGER) AS STRING:
        DIM messaggio AS STRING
        IF ora < 12 THEN
          messaggio = "Buongiorno"
        ELSEIF ora < 18 THEN
          messaggio = "Buon pomeriggio"
        ELSE
          messaggio = "Buonasera"
        END IF
        RETURN messaggio + ", " + nome + "!"
    END FN
    
    PRINT saluta("Mario", 10) ' Stampa "Buongiorno, Mario!"
    PRINT saluta("Giulia", 19) ' Stampa "Buonasera, Giulia!"
    PRINT
    
    PRINT "--- Test completato ---"

END SUB