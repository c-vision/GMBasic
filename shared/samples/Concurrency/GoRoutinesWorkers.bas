' FILE: Concurrency/GoRoutinesWorkers.bas
' =================================================================================================
'       GUIDA TEORICA: PATTERN WORKER POOL E GESTIONE DEI RISULTATI ASINCRONI
' =================================================================================================
'
' Questo test implementa un classico pattern di concorrenza: il "Worker Pool".
' Dimostra come distribuire il carico di lavoro su più task paralleli e come aggregare
' i risultati in modo asincrono.
'
' 1. IL PATTERN WORKER POOL
'    - Più "Lavoratori" (Goroutines) ascoltano sullo stesso canale di input (`Lavori`).
'    - Il "Main" (Produttore) invia i compiti sul canale.
'    - Il primo lavoratore libero preleva il compito (load balancing automatico gestito dal canale).
'    - I risultati vengono inviati su un canale di uscita condiviso (`Risultati`).
'
' 2. `GO FUNCTION ... INTO ...` (FUTURE)
'    A differenza di `GO SUB`, che lancia una procedura "fire-and-forget", `GO FUNCTION` avvia
'    un calcolo che restituisce un valore.
'    - `INTO Variabile`: La variabile di destinazione riceve immediatamente un oggetto "Future"
'      (o un placeholder).
'    - Risoluzione (Wait): Quando si tenta di leggere il valore della variabile (es. `PRINT TotaleFinale`
'      o `B = TotaleFinale`), l'esecuzione si blocca automaticamente finché la funzione non ha
'      restituito il valore (`RETURN`).
'
' 3. SEGNALI DI STOP (SENTINEL VALUES)
'    Per terminare i lavoratori in modo pulito, il Main invia un valore speciale (in questo caso `0`)
'    per ogni lavoratore. Quando un lavoratore riceve `0`, esce dal suo ciclo infinito e segnala
'    il completamento al `WAITGROUP`.
'
' 4. GESTIONE MISTA (WAITGROUP + FUTURE)
'    Questo esempio combina:
'    - `WAITGROUP`: Per attendere che tutti i lavoratori abbiano finito la parte fisica del lavoro.
'    - `FUTURE`: Per attendere che il Manager abbia finito di calcolare la somma.
'
' =================================================================================================

REM === TEST COMPLESSO E VISIVO DELLA CONCORRENZA (VERSIONE FINALE CORRETTA) ===

' --- Procedure Concorrenti ---

' Subroutine che simula un lavoratore. Viene lanciata più volte in parallelo.
SUB Lavoratore(ID, Lavori AS CHANNEL OF INTEGER, Risultati AS CHANNEL OF INTEGER, WG AS WAITGROUP)
    PRINT "Lavoratore"; ID; " avviato."
    DIM Compito AS INTEGER
    
    ' Ciclo infinito di elaborazione
    ciclo_lavoro:
    ' Si blocca qui in attesa di un compito dal canale condiviso.
    RECEIVE Compito FROM Lavori
    
    ' Controllo del valore sentinella per la terminazione.
    IF Compito = 0 THEN
        PRINT "Lavoratore"; ID; " ha ricevuto il segnale di stop."
        GOTO fine_lavoro
    END IF
    
    PRINT "Lavoratore"; ID; " ha iniziato il compito:"; Compito
    ' Simula un carico di lavoro proporzionale al valore del compito.
    SLEEP Compito * 100
    PRINT "Lavoratore"; ID; " ha finito il compito:"; Compito
    
    ' Invia il risultato elaborato al canale dei risultati (consumato dal Manager).
    SEND Compito * 10 TO Risultati
    GOTO ciclo_lavoro
    
    fine_lavoro:
    ' La clausola GO ... IN WG registra il task: il runtime esegue DONE implicitamente
    ' quando termina la goroutine, anche in caso di GO EXIT o errore.
END SUB

' Funzione che agisce come aggregatore. Viene lanciata come GO FUNCTION.
FUNCTION Manager(Risultati AS CHANNEL OF INTEGER, NumRisultati, WG AS WAITGROUP) AS LONG
    PRINT "Manager avviato. In attesa di"; NumRisultati; "risultati."
    DIM Somma AS LONG, RisultatoParziale AS INTEGER
    Somma = 0
    
    ' Ciclo determinato: sa esattamente quanti risultati deve aspettare.
    FOR I = 1 TO NumRisultati
        ' Si blocca in attesa che un lavoratore invii un risultato.
        RECEIVE RisultatoParziale FROM Risultati
        PRINT "Manager ha ricevuto il risultato parziale:"; RisultatoParziale
        Somma = Somma + RisultatoParziale
    NEXT I
    
    PRINT "Manager ha raccolto tutti i risultati."
    
    ' Restituisce il valore. Questo sbloccherà chiunque stia leggendo la variabile 'Future' associata.
    ' Nota: In GM-BASIC, il RETURN imposta il valore del Future.
    RETURN Somma
    
END FUNCTION

' --- Programma Principale ---
SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ GoRoutinesWorkers.bas - Concorrenza (Goroutines)                             ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    PRINT "--- AVVIO DELLA FABBRICA CONCORRENTE ---"
    PRINT
    
    ' Dichiarazione canali e sincronizzazione
    DIM CanaleLavori AS CHANNEL OF INTEGER
    DIM CanaleRisultati AS CHANNEL OF INTEGER
    DIM GruppoDiAttesa AS WAITGROUP
    DIM NumCompiti AS INTEGER
    NumCompiti = 5
    
    PRINT "Avvio di 3 lavoratori e 1 manager in background..."
    
    ' Avvia 3 lavoratori. Ognuno incrementa il contatore del WaitGroup (IN GruppoDiAttesa).
    GO SUB Lavoratore(1, CanaleLavori, CanaleRisultati, GruppoDiAttesa) IN GruppoDiAttesa
    GO SUB Lavoratore(2, CanaleLavori, CanaleRisultati, GruppoDiAttesa) IN GruppoDiAttesa
    GO SUB Lavoratore(3, CanaleLavori, CanaleRisultati, GruppoDiAttesa) IN GruppoDiAttesa
    
    ' Variabile che conterrà il risultato futuro.
    DIM TotaleFinale AS LONG
    
    ' Anche il Manager appartiene allo stesso WaitGroup; il completamento è implicito.
    ' Il risultato verrà memorizzato in TotaleFinale quando pronto.
    GO FUNCTION Manager(CanaleRisultati, NumCompiti, GruppoDiAttesa) IN GruppoDiAttesa INTO TotaleFinale
    
    PRINT
    PRINT "Il programma principale assegna"; NumCompiti; "compiti ai lavoratori..."
    
    ' Invio dei compiti nel canale. I lavoratori li preleveranno in concorrenza.
    SEND 5 TO CanaleLavori
    SEND 2 TO CanaleLavori
    SEND 8 TO CanaleLavori
    SEND 3 TO CanaleLavori
    SEND 6 TO CanaleLavori
    
    PRINT "Invio segnali di stop ai lavoratori..."
    ' Invia un segnale di stop (0) per ogni lavoratore avviato.
    SEND 0 TO CanaleLavori
    SEND 0 TO CanaleLavori
    SEND 0 TO CanaleLavori
    
    PRINT
    PRINT "Attendo che tutti i task (lavoratori e manager) finiscano..."
    ' Blocca il Main finché il contatore del WaitGroup non torna a zero.
    WAIT GruppoDiAttesa
    PRINT "Tutti i task hanno terminato."
    PRINT
    
    // Accesso al valore Future.
    // Se il Manager non avesse ancora finito (improbabile qui data la WAIT sopra),
    // questa istruzione bloccherebbe il Main.
    DIM SOMMA_CALCOLATA AS LONG
    SOMMA_CALCOLATA = TotaleFinale
    
    PRINT "--- RISULTATO FINALE ---"
    PRINT "La somma totale calcolata dal manager è:"; SOMMA_CALCOLATA 
    PRINT
    PRINT "--- ESECUZIONE COMPLETATA ---"
END SUB
