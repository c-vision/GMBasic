' FILE: Network/NetTcpAdvanced.bas
' ======================================================================================================================
' GUIDA TEORICA: FUNZIONALITÀ AVANZATE DEL NAMESPACE NET (TCP)
' ======================================================================================================================
' Questo programma testa le capacità di rete a basso livello di GM-BASIC, dimostrando un ciclo completo di
' comunicazione client-server TCP, l'uso di funzioni di introspezione della connessione e la configurazione
' avanzata dei socket.
'
' 1. COMANDI TCP DI BASE (Client-Server)
'    - TCP OPEN:  Avvia un server in ascolto su una porta.
'    - TCP ACCEPT: Blocca l'esecuzione finché un client non si connette (con timeout opzionale).
'    - TCP CONNECT: Tenta di connettersi a un server remoto (con timeout opzionale).
'    - TCP SEND: Invia dati (stringa o array di byte) su una connessione esistente.
'    - TCP RECV: Riceve dati in un buffer (con timeout opzionale).
'    - TCP CLOSE: Chiude una connessione o un listener.
'
' 2. FUNZIONI DI INTROSPEZIONE (Stato della Connessione)
'    - CONN(handle):      Restituisce TRUE se l'handle di connessione è valido e attivo.
'    - REMOTEIP(handle):   Restituisce l'indirizzo IP del client/server remoto.
'    - REMOTEPORT(handle): Restituisce la porta del client/server remoto.
'    - LOCALHOST():       Restituisce l'indirizzo IP primario della macchina locale.
'    - GETHOSTIP(host$):   Risolve un nome host (es. "localhost") nel suo indirizzo IP.
'
' 3. FUNZIONALITÀ AVANZATE
'    - SETOPTION handle, "Opzione", valore: Permette di configurare opzioni del socket a basso livello.
'      - "SO_REUSEADDR": Consente al server di riavviarsi rapidamente sulla stessa porta.
'      - "TCP_NODELAY": Disabilita l'algoritmo di Nagle, forzando l'invio immediato di piccoli pacchetti
'        (utile per applicazioni a bassa latenza come i giochi).
'    - SHUTDOWN handle, modo: Chiude parzialmente una connessione. `modo = 1` disabilita l'invio,
'      permettendo al client di ricevere dati residui prima della chiusura completa.
'    - PEEK(handle, num_bytes): "Sbircia" i dati in arrivo nel buffer del socket senza rimuoverli.
'      Utile per pre-analizzare i dati prima di decidere come leggerli.
'    - RECV(handle): Funzione (non comando) che restituisce il numero di byte attualmente disponibili
'      per la lettura nel buffer del socket, senza bloccare l'esecuzione.
'
' 4. GESTIONE DELLA CONCORRENZA
'    - `GO SUB` e `WAITGROUP`: Il test avvia il server e il client in due task concorrenti separati,
'      sincronizzandone la terminazione tramite un `WAITGROUP` per garantire che il programma
'      principale attenda la fine di entrambe le operazioni di rete.
' ======================================================================================================================

IMPORT NET

' Subroutine del Server
SUB Server(WG AS WAITGROUP)
  PRINT "[SERVER] Avviato."

  DIM serverHandle AS LONG
  DIM clientHandle AS LONG
    
  ' Apre la porta 18808 per accettare connessioni
  TCP OPEN 18808, serverHandle
  
  IF serverHandle > 0 THEN
    ' Opzione per riutilizzare l'indirizzo, utile per riavvii veloci del server
    SETOPTION serverHandle, "SO_REUSEADDR", 1
    PRINT "[SERVER] In ascolto su porta 18808. Attendo client per max 10 secondi..."
    
    ' Accetta una connessione, con un timeout di 10 secondi
    TCP ACCEPT serverHandle, clientHandle, 10000
    
    ' Controlla se la connessione è stata stabilita con successo
    IF clientHandle > 0 AND CONN(clientHandle) THEN
      PRINT "[SERVER] Client connesso!"
      PRINT "[SERVER]   - Handle Client: "; clientHandle
      PRINT "[SERVER]   - IP Remoto:     "; REMOTEIP(clientHandle)
      PRINT "[SERVER]   - Porta Remota:  "; REMOTEPORT(clientHandle)
      PRINT "[SERVER]   - Porta Locale:  "; LOCALPORT(clientHandle)
      
      DIM bytes AS LONG
      DIM data$
      
      PRINT "[SERVER] Attendo dati per max 5 secondi..."
      ' Riceve dati dal client
      TCP RECV clientHandle, data$, bytes, 5000
      
      IF bytes > 0 THEN
        PRINT "[SERVER] Ricevuti "; bytes; " byte: '"; data$; "'"
        ' Invia una risposta di "echo"
        TCP SEND clientHandle, "ECHO: " + data$
        PRINT "[SERVER] Risposta inviata."
      ELSE
        PRINT "[SERVER] Nessun dato ricevuto o connessione chiusa. (bytes="; bytes; ")"
      END IF
      
      ' Chiude solo il lato di invio del socket
      PRINT "[SERVER] Eseguo SHUTDOWN parziale (lato invio)."
      SHUTDOWN clientHandle, 1
      ' Chiude completamente la connessione con il client
      TCP CLOSE clientHandle
    ELSE
      PRINT "[SERVER] Timeout: Nessun client si è connesso."
    END IF
    
    ' Chiude il socket del server che era in ascolto
    TCP CLOSE serverHandle
  ELSE
    PRINT "[SERVER] Errore: Impossibile aprire la porta 18808."
  END IF
  
  PRINT "[SERVER] Terminato."
  ' Segnala al WaitGroup che il task del server è completato
  DONE WG
END SUB

' Subroutine del Client
SUB Client(WG AS WAITGROUP)
  PRINT "[CLIENT] Avviato."
  ' Risolve il nome 'localhost' nel suo indirizzo IP
  ip$ = GETHOSTIP("localhost")
  PRINT "[CLIENT] 'localhost' risolto in: "; ip$
  
  DIM clientHandle AS LONG
  DIM bytes AS LONG
  DIM response$
  
  ' Tenta di connettersi al server, con un timeout di 5 secondi
  TCP CONNECT ip$, 18808, clientHandle, 5000
  
  IF clientHandle > 0 THEN
    PRINT "[CLIENT] Connesso al server con handle: "; clientHandle
    ' Disabilita l'algoritmo di Nagle per inviare i dati immediatamente
    SETOPTION clientHandle, "TCP_NODELAY", 1
    
    ' Invia un messaggio al server
    TCP SEND clientHandle, "Ciao Mondo!"
    
    PRINT "[CLIENT] In attesa di risposta... Sbircio i primi 10 byte..."
    ' Legge i dati in arrivo senza rimuoverli dal buffer
    preview$ = PEEK(clientHandle, 10)
    PRINT "[CLIENT] PEEK ha visto: '"; preview$; "'"
    
    ' Controlla quanti byte sono effettivamente disponibili per la lettura
    available = RECV(clientHandle)
    PRINT "[CLIENT] Byte disponibili da leggere: "; available
    
    ' Legge la risposta completa del server
    TCP RECV clientHandle, response$, bytes, 5000
    
    IF bytes > 0 THEN
      PRINT "[CLIENT] Risposta ricevuta: '"; response$; "'"
    ELSE
      PRINT "[CLIENT] Nessuna risposta dal server. (bytes="; bytes; ")"
    END IF

    ' Il server chiude il lato di invio subito dopo l'echo: una seconda receive deve
    ' riprendere con EOF (zero byte), senza bloccare l'unico worker GPM.
    TCP RECV clientHandle, response$, bytes, 2000
    IF bytes = 0 THEN
      PRINT "TCP-DISCONNECT-RESUME-OK"
    ELSE
      PRINT "FAIL:TCP-DISCONNECT-BYTES="; bytes
    END IF

    ' Chiude la connessione
    TCP CLOSE clientHandle
  ELSE
    PRINT "[CLIENT] Errore: Connessione al server fallita."
  END IF
  
  PRINT "[CLIENT] Terminato."
  ' Segnala al WaitGroup che il task del client è completato
  DONE WG
END SUB

' Programma Principale
SUB Main()
  CLS
  PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ NetTcpAdvanced - Test Completo delle Funzionalità TCP                        ║"
  PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
  PRINT
  PRINT "IP Locale Primario: "; LOCALHOST()
  
  ' Sincronizza l'esecuzione concorrente
  DIM Gruppo AS WAITGROUP
  
  ' Avvia il server in un task separato
  PRINT "[MAIN] Avvio GO SUB Server..."
  GO SUB Server(Gruppo) IN Gruppo
  
  ' Breve attesa per dare tempo al server di avviarsi
  SLEEP 500
  
  ' Avvia il client in un altro task separato
  PRINT "[MAIN] Avvio GO SUB Client..."
  GO SUB Client(Gruppo) IN Gruppo
  
  ' Attende che sia il server che il client abbiano terminato
  PRINT "[MAIN] Attendo il completamento di Server e Client..."
  WAIT Gruppo
  PRINT
  PRINT "--- TEST COMPLETATO ---"
END SUB
