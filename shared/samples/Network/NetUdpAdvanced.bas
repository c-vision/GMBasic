' FILE: Network/NetUdpAdvanced.bas
' ======================================================================================================================
' GUIDA TEORICA: COMUNICAZIONE UDP (USER DATAGRAM PROTOCOL)
' ======================================================================================================================
' Questo programma dimostra la comunicazione "connection-less" tramite UDP, un protocollo di rete veloce ma
' inaffidabile, ideale per applicazioni dove la velocità è più critica della garanzia di consegna (es. giochi,
' streaming audio/video, DNS).
'
' 1. COMANDI UDP
'    - UDP OPEN [<port>], <handleVar>: Crea un "socket" UDP. A differenza di TCP, non c'è una connessione
'      permanente.
'      - Se la porta è specificata, il socket si "lega" a quella porta, pronto a ricevere dati (comportamento da server).
'      - Se la porta è omessa (o 0), il sistema operativo assegna una porta effimera (comportamento da client).
'    - UDP SEND <handle>, <host$>, <port>, <data>: Invia un singolo pacchetto (datagramma) a un destinatario
'      specifico. Ogni invio è un'azione indipendente.
'    - UDP RECV <handle>, <buffer$>, <ip$>, <portVar>, [<timeout>]: Attende la ricezione di un datagramma da
'      qualsiasi mittente. Popola le variabili `ip$` e `portVar` con l'indirizzo del mittente, permettendo così
'      di inviare una risposta.
'    - UDP CLOSE <handle>: Rilascia le risorse associate al socket.
'
' 2. DIFFERENZE CHIAVE TRA TCP e UDP
'    - Connessione: TCP è "connection-oriented" (prima `CONNECT`/`ACCEPT`, poi `SEND`/`RECV`). UDP è "connection-less"
'      (si inviano pacchetti a un indirizzo senza preamboli).
'    - Affidabilità: TCP garantisce che i dati arrivino in ordine, senza errori e senza perdite. UDP non offre
'      nessuna di queste garanzie. Un pacchetto può essere perso, duplicato o arrivare fuori ordine.
'    - Overhead: UDP è molto più "leggero" e veloce di TCP perché non ha la complessità della gestione della
'      connessione e del controllo degli errori.
'
' 3. FLUSSO DI ESECUZIONE DEL TEST
'    - `Main` avvia concorrentemente un server e un client UDP.
'    - `ServerUdp`: Si lega alla porta 9090 e attende un datagramma con `UDP RECV`. Quando lo riceve, stampa
'      i dati e l'indirizzo del mittente, e invia un messaggio di "echo" allo stesso mittente.
'    - `ClientUdp`: Crea un socket su una porta casuale, invia un messaggio al server (`127.0.0.1:9090`) con
'      `UDP SEND`, e si mette in attesa della risposta di echo con `UDP RECV`.
' ======================================================================================================================

IMPORT NET

' Subroutine che agisce da Server UDP
SUB ServerUdp(WG AS WAITGROUP)

  DIM serverHandle AS LONG
  DIM ip$, data$, remotePort ' Rinominato per chiarezza

  PRINT "[SERVER UDP] Avviato."

  ' Apre un socket UDP e lo lega alla porta 9090
  UDP OPEN 9090, serverHandle
  
  IF serverHandle > 0 THEN
    PRINT "[SERVER UDP] In ascolto su porta 9090 per max 10 secondi..."
    
    ' Attende di ricevere un datagramma. Se arriva, popola data$, ip$ e remotePort.
    UDP RECV serverHandle, data$, ip$, remotePort, 10000
    
    IF remotePort > 0 THEN
      PRINT "[SERVER UDP] Ricevuto datagramma da "; ip$; ":"; remotePort
      PRINT "[SERVER UDP]   - Dati: '"; data$; "'"
      
      ' Invia una risposta di "echo" direttamente all'indirizzo del mittente
      DIM response$ = "UDP echo: " + data$
      UDP SEND serverHandle, ip$, remotePort, response$
      PRINT "[SERVER UDP] Risposta inviata a "; ip$
    ELSE
      PRINT "[SERVER UDP] Timeout: Nessun datagramma ricevuto."
    END IF
    
    ' Chiude il socket
    UDP CLOSE serverHandle
  ELSE
    PRINT "[SERVER UDP] Errore: Impossibile aprire la porta 9090."
  END IF
  
  PRINT "[SERVER UDP] Terminato."
  ' Segnala al WaitGroup che il task è completato
  DONE WG
END SUB

' Subroutine che agisce da Client UDP
SUB ClientUdp(WG AS WAITGROUP)
  PRINT "[CLIENT UDP] Avviato."
  
  DIM clientHandle AS LONG
  DIM ip$, response$, remotePort ' Rinominato per chiarezza

  ' Apre un socket UDP su una porta scelta dal sistema operativo
  UDP OPEN, clientHandle
  
  IF clientHandle > 0 THEN
    PRINT "[CLIENT UDP] Socket creato con handle: "; clientHandle
    PRINT "[CLIENT UDP]   - Porta Locale: "; LOCALPORT(clientHandle)
    
    ' Invia un messaggio al server sulla porta 9090
    UDP SEND clientHandle, "127.0.0.1", 9090, "Messaggio UDP!"
    PRINT "[CLIENT UDP] Datagramma inviato."
    
    PRINT "[CLIENT UDP] In attesa di risposta per max 5 secondi..."
    ' Attende la risposta di echo dal server
    UDP RECV clientHandle, response$, ip$, remotePort, 5000
    
    IF remotePort > 0 THEN
      PRINT "[CLIENT UDP] Risposta ricevuta da "; ip$; ":"; remotePort
      PRINT "[CLIENT UDP]   - Dati: '"; response$; "'"
    ELSE
      PRINT "[CLIENT UDP] Timeout: Nessuna risposta dal server."
    END IF

    ' Nessun secondo datagramma e' previsto: il timeout deve vincere una sola volta,
    ' azzerare gli output e riprendere il PCB senza occupare il worker GPM.
    UDP RECV clientHandle, response$, ip$, remotePort, 100
    IF remotePort = 0 AND response$ = "" THEN
      PRINT "UDP-TIMEOUT-RESUME-OK"
    ELSE
      PRINT "FAIL:UDP-TIMEOUT-OUTPUT"
    END IF

    ' Chiude il socket
    UDP CLOSE clientHandle
  ELSE
    PRINT "[CLIENT UDP] Errore: Impossibile creare il socket."
  END IF
  
  PRINT "[CLIENT UDP] Terminato."
  ' Segnala al WaitGroup che il task è completato
  DONE WG
END SUB

' Programma Principale
SUB Main()
  CLS
  PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ NetUdpAdvanced - Test Completo delle Funzionalità UDP                        ║"
  PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
  PRINT
  
  ' Sincronizza l'esecuzione concorrente del server e del client
  DIM Gruppo AS WAITGROUP
  
  ' Avvia il server in un task separato
  PRINT "[MAIN] Avvio GO SUB ServerUdp..."
  GO SUB ServerUdp(Gruppo) IN Gruppo
  
  ' Breve attesa per dare tempo al server di avviarsi e legarsi alla porta
  SLEEP 500
  
  ' Avvia il client in un altro task separato
  PRINT "[MAIN] Avvio GO SUB ClientUdp..."
  GO SUB ClientUdp(Gruppo) IN Gruppo
  
  ' Attende che sia il server che il client abbiano terminato
  PRINT "[MAIN] Attendo il completamento di Server e Client..."
  WAIT Gruppo
  PRINT
  PRINT "--- TEST COMPLETATO ---"
END SUB
