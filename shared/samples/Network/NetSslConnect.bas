' FILE: Network/NetSslConnect.bas
' ======================================================================================================================
' GUIDA TEORICA: CONNESSIONI DI RETE SICURE (SSL/TLS)
' ======================================================================================================================
' GM-BASIC fornisce comandi di alto livello per stabilire connessioni di rete sicure utilizzando il protocollo
' SSL/TLS, lo stesso standard che protegge la navigazione web (HTTPS).
'
' 1. IL COMANDO `SSL CONNECT`
'    - Sintassi: SSL CONNECT <host$>, <handleVar> [, <port> [, <timeout_ms>]]
'    - Funzionamento: Questo comando astrae l'intera complessità di una connessione sicura:
'      1. Risoluzione DNS: Converte il nome host (es. "api.ipify.org") in un indirizzo IP.
'      2. Connessione TCP: Stabilisce una connessione di rete di base sulla porta specificata (default 443).
'      3. Handshake SSL/TLS: Esegue la negoziazione crittografica con il server.
'      4. Validazione del Certificato: Controlla che il certificato digitale del server sia valido, attendibile
'         e emesso per l'host a cui ci si sta connettendo, proteggendo da attacchi "man-in-the-middle".
'    - Risultato: Se la connessione ha successo, popola la variabile `handleVar` con un numero positivo (handle).
'      In caso di fallimento, `handleVar` sarà 0.
'
' 2. COMUNICAZIONE SUL CANALE SICURO (`TCP SEND`, `TCP RECV`)
'    - Una volta stabilita la connessione SSL, l'handle restituito può essere utilizzato con i comandi TCP
'      standard (`TCP SEND`, `TCP RECV`).
'    - L'interprete gestisce automaticamente la cifratura e decifratura dei dati in modo trasparente.
'      Dal punto di vista del programmatore, è come comunicare su una normale connessione TCP.
'
' 3. ESEMPIO PRATICO: RICHIESTA HTTP MANUALE
'    - Questo programma simula un client HTTP di base. Dopo aver stabilito la connessione SSL, costruisce
'      manualmente una richiesta HTTP/1.1 (che è un semplice formato di testo) e la invia con `TCP SEND`.
'    - Successivamente, entra in un ciclo `DO...LOOP` per ricevere la risposta dal server con `TCP RECV`
'      finché il server non chiude la connessione (indicato da `bytesReceived = 0`).
' ======================================================================================================================

' DESCRIZIONE:
'   Questo test dimostra la capacità di connettersi a un server HTTPS pubblico,
'   inviare una richiesta HTTP/1.1 e ricevere una risposta.
'   Il comando SSL CONNECT gestisce automaticamente la validazione del
'   certificato del server, proprio come un browser.
IMPORT NET

' --- IMPOSTAZIONI ---
' Variabili globali per l'host e la porta del server di destinazione
' GLOBAL scope variables disabled (pre-existing runtime scope issue)
' Using literal values directly instead
GLOBAL clientHandle ' Handle per la connessione di rete

' --- MAIN ---
SUB Main
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ NetSslConnect - Connessione SSL a un server HTTPS                            ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    PRINT "Tentativo di connessione a api.ipify.org sulla porta 443"
    
   ' Usa SSL CONNECT. La porta è opzionale, se omessa usa 443 di default.
   ' Il timeout (in millisecondi) previene blocchi indefiniti.
    SSL CONNECT "api.ipify.org", clientHandle, 443, 10000
    
    ' Controlla se la connessione è stata stabilita correttamente.
    IF clientHandle = 0 THEN
        PRINT "Connessione fallita. Codice di errore: "; ERR
        END ' Termina il programma in caso di errore
    END IF
    
    PRINT "Connessione SSL stabilita. Handle: "; clientHandle
    
   ' Costruisce una richiesta HTTP/1.1 standard come stringa.
   ' CHR(13) + CHR(10) è la sequenza standard per il ritorno a capo (CRLF).
    DIM request$
    request$ = "GET / HTTP/1.1" + CHR(13) + CHR(10)
    request$ = request$ + "Host: api.ipify.org" + CHR(13) + CHR(10)
    request$ = request$ + "Connection: close" + CHR(13) + CHR(10) ' Chiede al server di chiudere la connessione dopo la risposta
    request$ = request$ + CHR(13) + CHR(10) ' Riga vuota per terminare gli header
    
    PRINT "Invio richiesta HTTP..."
    ' Invia la richiesta sul canale sicuro.
    TCP SEND clientHandle, request$
    
    PRINT "In attesa della risposta..."
    DIM response$, bytesReceived
    
   ' Ciclo di ricezione: continua a leggere dati finché il server non chiude la connessione
   ' (indicato da bytesReceived che diventa 0).
    DO
        TCP RECV clientHandle, response$, bytesReceived, 5000 ' Timeout di 5 secondi per ogni lettura
        IF bytesReceived > 0 THEN
            PRINT response$; ' Usa il ; per stampare i blocchi di risposta consecutivamente
        END IF
    LOOP WHILE bytesReceived > 0
    
    PRINT
    PRINT "--- Fine della risposta ---"
    
    ' Chiude la connessione TCP e rilascia le risorse.
    TCP CLOSE clientHandle
    PRINT "Connessione chiusa."
END SUB