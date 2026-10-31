' FILE: Functions/FunctionsSubAsAction.bas
' =================================================================================================
'       GUIDA TEORICA: USARE `SUB` COME TIPO DELEGATO (TODO #83 - Soluzione Alternativa)
' =================================================================================================
'
' Questo programma di test dimostra la soluzione proposta per migliorare la sintassi
' della dichiarazione di variabili che contengono riferimenti a procedure `SUB`.
'
' 1. IL PROBLEMA: SINTASSI CONTROINTUITIVA
'    - Attualmente, per dichiarare una variabile che deve contenere una `SUB` (una procedura
'      senza valore di ritorno), si è costretti a usare una sintassi fuorviante:
'      `DIM mioHandler AS FUNCTION(STRING) AS VOID`
'    - Questo è confuso perché si usa la keyword `FUNCTION` per riferirsi a una `SUB`.
'
' 2. LA SOLUZIONE PROPOSTA: `AS SUB(...)`
'    - Questo TODO propone di permettere l'uso della keyword `SUB` direttamente nella
'      dichiarazione del tipo, rendendo il codice semanticamente corretto e più leggibile.
'    - La nuova sintassi sarebbe:
'      `DIM mioHandler AS SUB(STRING)`
'
' 3. COMPORTAMENTO INTERNO DEL COMPILATORE
'    - Questa modifica è puramente "zucchero sintattico".
'    - Il Parser verrebbe aggiornato per riconoscere la sequenza `AS SUB(...)`.
'    - Internamente, il Linker tratterebbe `AS SUB(STRING)` come un alias per
'      `FUNCTION(STRING) AS VOID`, generando lo stesso identico AST finale.
'    - Il risultato è un codice più pulito per il programmatore, senza alcun impatto
'      sull'architettura di runtime.
'
' 4. ESEMPI IN QUESTO FILE
'    - Dichiarazione di una variabile `gestoreSaluto` usando `AS SUB(STRING)`.
'    - Assegnazione della procedura `Saluta` a questa variabile.
'    - Invocazione della procedura tramite la variabile.
'    - Una subroutine `EseguiHandler` che accetta un'altra procedura come parametro,
'      dimostrando l'uso di `AS SUB(...)` nelle firme delle funzioni.
'
' =================================================================================================

' Definiamo una semplice SUB che verrà usata come nostro "valore"
SUB Saluta(nome AS STRING)
    PRINT "--------------------------"
    PRINT "Ciao, "; nome; "!"
    PRINT "Questa SUB e' stata eseguita tramite un riferimento."
    PRINT "--------------------------"
END SUB

' Definiamo un'altra SUB con una firma diversa per i test
SUB MostraLog(messaggio AS STRING, livello AS INTEGER)
    PRINT "[LOG Livello "; livello; "]: "; messaggio
END SUB

' Definiamo una subroutine che accetta un'altra SUB come parametro
SUB EseguiHandler(handler AS SUB(s AS STRING), dato AS STRING)
    PRINT "Sto per invocare l'handler ricevuto..."
    handler(dato)
    PRINT "...handler eseguito."
    PRINT
END SUB


SUB Main()
    PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ Test dell'uso di 'AS SUB(...)' come tipo di dato                         ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. Dichiarazione e Assegnazione ---
    PRINT "--- 1. Dichiarazione di una variabile `AS SUB(STRING)` ---"
    
    ' SINTASSI PROPOSTA: Chiara, concisa e semanticamente corretta.
    DIM gestoreSaluto AS SUB(s AS STRING)
    
    PRINT "Assegnazione della procedura 'Saluta' alla variabile."
    gestoreSaluto = Saluta
    PRINT
    
    ' --- 2. Invocazione tramite variabile ---
    PRINT "--- 2. Invocazione della SUB tramite la variabile..."
    gestoreSaluto("GM-BASIC")
    PRINT
    
    ' --- 3. Passaggio come parametro a un'altra SUB ---
    PRINT "--- 3. Passaggio della variabile a un'altra subroutine `EseguiHandler` ---"
    EseguiHandler(gestoreSaluto, "Mondo")
    
    ' --- 4. Assegnazione di un'altra procedura compatibile (dimostrazione flessibilità) ---
    PRINT "--- 4. Test di riassegnazione con un'altra procedura (con firma diversa) ---"
    
    ' Dichiariamo una variabile per la seconda firma
    DIM gestoreLog AS SUB(s AS STRING, i AS INTEGER)
    gestoreLog = MostraLog
    
    PRINT "Esecuzione del secondo handler:"
    gestoreLog("Test di log", 1)
    PRINT
    
    PRINT "Test completato con successo."
END SUB
