' FILE: Network/NetSslServer.bas
' TAG: interactive
' ======================================================================================================================
' GUIDA TEORICA: CREAZIONE DI SERVER SICURI CON SSL/TLS
' ======================================================================================================================
' Questo programma dimostra il ciclo di vita completo della creazione di un server TCP che comunica in modo sicuro
' utilizzando il protocollo SSL/TLS (lo stesso di HTTPS).
'
' 1. IL COMANDO `SSL OPEN`
'    - Sintassi: SSL OPEN <port>, <handleVar>, <certPath$>, [<certPass$>]
'    - Funzionamento: A differenza di `TCP OPEN` che crea un server non protetto, `SSL OPEN` richiede un certificato
'      digitale (in formato PFX o P12) per autenticarsi presso i client. Questo comando apre la porta specificata
'      in modalità di ascolto e la prepara per accettare connessioni sicure.
'    - Certificato: Il file del certificato (es. "server.pfx") deve contenere sia la chiave pubblica che la
'      chiave privata del server ed è protetto da una password.
'
' 2. IL COMANDO `TCP ACCEPT`
'    - Funzionamento: Una volta che il listener SSL è attivo (`SSL OPEN`), `TCP ACCEPT` viene utilizzato per
'      attendere e accettare una connessione da un client.
'    - Handshake Automatico: Durante la chiamata a `TCP ACCEPT`, il runtime di GM-BASIC esegue automaticamente
'      l'handshake SSL/TLS con il client. Se l'handshake ha successo, viene stabilito un canale di comunicazione
'      cifrato e viene restituito un handle univoco per il client. Se fallisce, l'handle restituito è 0.
'
' 3. COMUNICAZIONE CIFRATA (`TCP SEND` / `TCP RECV`)
'    - Una volta che `TCP ACCEPT` ha restituito un handle di client valido, tutte le comunicazioni che utilizzano
'      quell'handle sono automaticamente cifrate e decifrate dal runtime.
'    - Dal punto di vista del programmatore, `TCP SEND` e `TCP RECV` si usano esattamente come si farebbe con
'      una connessione TCP non sicura, astraendo tutta la complessità della crittografia.
'
' 4. CICLO DI VITA DEL SERVER
'    1. Avvio (`SSL OPEN`): Si crea il socket di ascolto sulla porta desiderata.
'    2. Attesa (`TCP ACCEPT`): Il programma si blocca in attesa di un client.
'    3. Comunicazione (`TCP RECV`, `TCP SEND`): Si scambiano dati sul canale sicuro.
'    4. Chiusura (`TCP CLOSE`): Si chiudono prima l'handle del client e poi l'handle del listener.
' ======================================================================================================================

' DESCRIZIONE:
'   Questo test dimostra la creazione di un server SSL. Apre una porta
'   in ascolto usando un certificato, attende una connessione, esegue
'   un semplice "echo" dei dati ricevuti e chiude.
'
' PREREQUISITI:
'   Un file certificato 'server.pfx' deve essere presente nella stessa
'   directory. La password del certificato deve corrispondere a quella
'   specificata nel codice.
IMPORT NET

' --- IMPOSTAZIONI GLOBALI ---
' GLOBAL scope disabled (pre-existing runtime scope issue)
GLOBAL listenerHandle, clientHandle

' --- SUB MAIN (PUNTO DI INGRESSO) ---
SUB Main
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ NetSslServer - Creazione di un server SSL                                    ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    PRINT "Tentativo di avviare il server SSL sulla porta 8443"
    
    ' Avvia il listener SSL
    SSL OPEN 8443, listenerHandle, "server.pfx", "test"
    
    ' Controlla se l'avvio del server ha avuto successo
    IF listenerHandle = 0 THEN
        PRINT "Avvio del server fallito. Controllare il file del certificato e la password."
        PRINT "Codice di errore: "; ERR
        END ' Termina il programma in caso di errore
    END IF
    
    PRINT "Server SSL in ascolto. Handle: "; listenerHandle
    PRINT "In attesa di una connessione... (usare 'openssl s_client -connect 127.0.0.1:8443')"
    
    ' Mettiti in attesa di una connessione client. Timeout di 30 secondi.
    TCP ACCEPT listenerHandle, clientHandle, 30000
    
    ' Controlla se un client si è connesso
    IF clientHandle = 0 THEN
        PRINT "Nessuna connessione ricevuta o handshake fallito."
        TCP CLOSE listenerHandle
        END
    END IF
    
    PRINT "Connessione sicura accettata da un client. Handle: "; clientHandle
    
    ' Riceve un messaggio dal client con un timeout di 5 secondi
    DIM receivedData$, bytesReceived
    TCP RECV clientHandle, receivedData$, bytesReceived, 5000
    
    IF bytesReceived > 0 THEN
        PRINT "Dati ricevuti: "; receivedData$
        
        ' Invia una risposta (echo) al client
        DIM response$ = "SERVER ECHO: " + receivedData$
        TCP SEND clientHandle, response$
        PRINT "Risposta inviata."
    ELSE
        PRINT "Nessun dato ricevuto dal client."
    END IF
    
    ' Chiude entrambe le connessioni per liberare le risorse
    PRINT "Chiusura delle connessioni."
    TCP CLOSE clientHandle
    TCP CLOSE listenerHandle
    
    PRINT "Server terminato."
END SUB
