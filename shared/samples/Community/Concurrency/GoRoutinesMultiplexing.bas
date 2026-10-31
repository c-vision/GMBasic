' FILE: Concurrency/GoRoutinesMultiplexing.bas
' =================================================================================================
'       GUIDA TEORICA: MULTIPLEXING DEI CANALI (GO SELECT)
' =================================================================================================
'
' Il costrutto `GO SELECT` è lo strumento fondamentale per gestire la concorrenza complessa.
' Permette a un task di attendere su più operazioni di comunicazione (invio o ricezione)
' simultaneamente, sbloccandosi non appena una di esse diventa disponibile.
'
' 1. SINTASSI `GO SELECT`
'    GO SELECT
'        CASE RECEIVE var FROM canaleA: 
'            ' Eseguito se canaleA ha un valore pronto
'        CASE SEND valore TO canaleB:
'            ' Eseguito se canaleB è pronto a ricevere (es. ha spazio nel buffer)
'        CASE DEFAULT:
'            ' Eseguito immediatamente se nessun altro caso è pronto (opzionale)
'    END SELECT
'
' 2. LOGICA DI ESECUZIONE
'    - Se più casi sono pronti contemporaneamente, lo scheduler ne sceglie uno in modo pseudo-random
'      per garantire l'equità (fairness).
'    - Se nessun caso è pronto e non c'è un `CASE DEFAULT`, il task viene messo in pausa finché
'      una delle risorse non diventa disponibile.
'    - `CASE DEFAULT` trasforma la SELECT in un'operazione non bloccante (polling).
'
' 3. `GO EXIT`
'    Termina immediatamente la goroutine corrente. È il modo più pulito per uscire da un task
'    senza dover risalire l'intera catena di chiamate o gestire flag globali.
'
' =================================================================================================

' --- PRODUTTORE DI MESSAGGI ---
SUB Produttore(ID AS STRING, Ritardo, Canale AS CHANNEL OF STRING)
    FOR I = 1 TO 5
        SLEEP Ritardo
        DIM Msg AS STRING
        Msg = "Messaggio da " + ID + " #" + STR(I)
        SEND Msg TO Canale
    NEXT I
    ' Il produttore finisce il suo lavoro
END SUB

' --- CONSUMATORE CON MULTIPLEXING ---
SUB Monitor(C1 AS CHANNEL OF STRING, C2 AS CHANNEL OF STRING, WG AS WAITGROUP)
    PRINT "[MONITOR] Avviato. In attesa di dati da più sorgenti..."
    
    DIM Msg AS STRING
    DIM TotaleRicevuti AS INTEGER = 0
    DIM CicliVuoti AS INTEGER = 0

    ' Continua finché non abbiamo ricevuto tutti i messaggi attesi (5 da C1 + 5 da C2)
    WHILE TotaleRicevuti < 10
        GO SELECT
            CASE RECEIVE Msg FROM C1:
                PRINT "[MONITOR] Ricevuto da Sorgente A: "; Msg
                TotaleRicevuti = TotaleRicevuti + 1

            CASE RECEIVE Msg FROM C2:
                PRINT "[MONITOR] Ricevuto da Sorgente B: "; Msg
                TotaleRicevuti = TotaleRicevuti + 1

            CASE DEFAULT:
                ' Questo blocco viene eseguito se nessun messaggio è pronto.
                ' Permette di fare altro lavoro o semplicemente non restare bloccati.
                CicliVuoti = CicliVuoti + 1
                SLEEP 50 ' Evita di saturare la CPU in questo loop di polling
        END SELECT
    WEND

    PRINT "[MONITOR] Lavoro completato."
    PRINT "[MONITOR] Statistiche: Messaggi totali="; TotaleRicevuti; " Cicli in IDLE="; CicliVuoti
    
    ' Segnala la fine al Main
    DONE WG
    
    ' Esempio di uscita forzata tramite GO EXIT
    PRINT "[MONITOR] Eseguo shutdown definitivo."
    GO EXIT
    PRINT "Questa riga non verrà mai stampata!"
END SUB

' --- PROGRAMMA PRINCIPALE ---
SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ GoRoutinesMultiplexing.bas - Multiplexing con GO SELECT                      ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' Creazione dei canali
    DIM SorgenteA AS CHANNEL OF STRING
    DIM SorgenteB AS CHANNEL OF STRING
    DIM GruppoAttesa AS WAITGROUP

    ' Avvio dei produttori con ritardi differenti
    ' Sorgente A invia ogni 200ms, Sorgente B ogni 500ms.
    PRINT "[MAIN] Avvio produttori concorrenti..."
    GO SUB Produttore("A", 200, SorgenteA)
    GO SUB Produttore("B", 500, SorgenteB)

    ' Avvio del Monitor (Multiplexer)
    PRINT "[MAIN] Avvio Monitor in background..."
    GO SUB Monitor(SorgenteA, SorgenteB, GruppoAttesa) IN GruppoAttesa

    PRINT "[MAIN] Monitoraggio in corso..."
    PRINT

    ' Il Main attende che il Monitor finisca tramite il WaitGroup
    WAIT GruppoAttesa

    PRINT
    PRINT "--- TEST COMPLETATO CON SUCCESSO ---"
END SUB
