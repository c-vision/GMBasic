' FILE: Network/NetSslConcurrency.bas
' ======================================================================================================================
' GUIDA TEORICA: CONCORRENZA, GO SUB E WAITGROUP
' ======================================================================================================================
' Questo programma testa la capacità di GM-BASIC di gestire operazioni di rete concorrenti (multiple alla volta)
' in modo sicuro ed efficiente, utilizzando il modello di concorrenza ispirato a Go.
'
' 1. CONCORRENZA CON `GO SUB`
'    - `GO SUB <nome_sub>`: Avvia l'esecuzione di una subroutine in un nuovo "task" o "goroutine" separato.
'      L'esecuzione del programma principale continua immediatamente senza attendere la fine della subroutine.
'    - Questo permette di eseguire operazioni lunghe (come una connessione di rete) in background, mantenendo
'      il programma principale reattivo.
'
' 2. SINCRONIZZAZIONE CON `WAITGROUP`
'    - `WAITGROUP` è un tipo di dato speciale che funge da contatore di task. È fondamentale per sapere quando
'      un gruppo di operazioni concorrenti è terminato.
'    - `IN <waitgroup>`: La clausola `IN` in `GO SUB` registra il nuovo task nel WaitGroup specificato
'      (incrementando il suo contatore interno).
'    - `DONE <waitgroup>`: Chiamato alla fine di un task concorrente, decrementa il contatore del WaitGroup,
'      segnalando che il task ha completato il suo lavoro.
'    - `WAIT <waitgroup>`: Blocca l'esecuzione del task corrente (in questo caso, `Main`) finché il contatore
'      del WaitGroup non scende a zero.
'
' 3. FLUSSO DI ESECUZIONE DEL TEST
'    - `Main` crea un `WAITGROUP`.
'    - Un ciclo `FOR` avvia `numClients` (es. 10) istanze della subroutine `SslClient` usando `GO SUB`.
'      Ognuna viene registrata nel `WAITGROUP`.
'    - `Main` raggiunge immediatamente l'istruzione `WAIT` e si mette in pausa.
'    - Nel frattempo, le 10 subroutine `SslClient` vengono eseguite in parallelo dal runtime.
'    - Ogni `SslClient` tenta una connessione SSL, stampa il risultato e poi chiama `DONE`.
'    - Quando l'ultimo `SslClient` chiama `DONE`, il contatore del `WAITGROUP` arriva a zero.
'    - L'istruzione `WAIT` in `Main` si sblocca, e il programma principale prosegue fino alla fine.
' ======================================================================================================================

' DESCRIZIONE:
'   Lancia multiple connessioni SSL concorrenti per verificare la
'   robustezza del motore di rete e l'assenza di race condition.
IMPORT NET

' GLOBAL scope disabled (pre-existing runtime scope issue)

' Subroutine che rappresenta un singolo client SSL.
' Viene eseguita in un task separato.
SUB SslClient(WG AS WAITGROUP, id)
    ' Ogni client ha il suo handle di connessione.
    DIM clientHandle
    
    ' Tenta di stabilire una connessione SSL sicura.
    ' È un'operazione bloccante (per questo task), gestita dal runtime.
    SSL CONNECT "api.ipify.org", clientHandle
    
    ' Controlla l'esito della connessione
    IF clientHandle > 0 THEN
        PRINT "[Client "; id; "] Connessione OK. Handle: "; clientHandle
        ' Chiude la connessione
        TCP CLOSE clientHandle
    ELSE
        PRINT "[Client "; id; "] Connessione FALLITA."
    END IF
    
    ' Segnala al WaitGroup che questo task ha terminato.
    DONE WG
END SUB

SUB Main
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ NetSslConcurrency - Test di connessioni SSL concorrenti                      ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    PRINT "Avvio di 10 client SSL concorrenti verso api.ipify.org"
    
    ' Crea un WaitGroup per sincronizzare i task.
    DIM Gruppo AS WAITGROUP
    
    ' Avvia N client in parallelo.
    FOR i = 1 TO 10
        ' GO SUB avvia la subroutine in un nuovo task.
        ' IN Gruppo registra il task nel WaitGroup.
        GO SUB SslClient(Gruppo, i) IN Gruppo
    NEXT i
    
    PRINT "Attendo il completamento di tutti i client..."
    ' WAIT si blocca finché tutti i task registrati in 'Gruppo' non chiamano DONE.
    WAIT Gruppo
    
    PRINT
    PRINT "--- TEST DI CONCORRENZA COMPLETATO ---"
END SUB