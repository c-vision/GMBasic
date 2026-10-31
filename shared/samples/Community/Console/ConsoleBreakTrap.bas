' FILE: Console/ConsoleBreakTrap.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE INTERRUZIONI (CTRL+C / BREAK)
' =================================================================================================
'
' Questo test verifica la capacità dell'interprete di rilevare segnali di interruzione
' esterni (come la combinazione di tasti CTRL+C) durante l'esecuzione di cicli intensivi.
'
' 1. IL CICLO DI ESECUZIONE E GLI EVENTI
'    Un interprete robusto non deve mai "bloccarsi" completamente durante l'esecuzione di
'    codice utente, specialmente nei cicli stretti. Deve periodicamente (spesso ad ogni
'    nuova istruzione o "tick" della VM) controllare la coda degli eventi di sistema
'    per rilevare richieste di terminazione.
'
' 2. COMPORTAMENTO DI DEFAULT
'    In assenza di gestori specifici definiti dall'utente (come `ON KEY` per il codice 3
'    o routine di gestione errori), la pressione di CTRL+C invia un segnale di "Interrupt".
'    L'interprete deve:
'    1. Rilevare il segnale asincrono.
'    2. Interrompere il flusso di esecuzione corrente.
'    3. Terminare il programma immediatamente, restituendo il controllo al sistema.
'
' 3. SCOPO DEL TEST
'    Questo script esegue un ciclo lungo (100.000 iterazioni) con output a video.
'    Serve a verificare manualmene che l'utente possa interrompere il programma
'    prima che il conteggio finisca. Se il programma non si ferma premendo CTRL+C,
'    significa che l'interprete non sta controllando gli input di sistema durante l'esecuzione.
'
' =================================================================================================

1 CLS
2 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ ConsoleBreakTrap - Intercettazione CTRL+C                                ║"
4 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
5 PRINT
10 PRINT "Il programma eseguirà un lungo ciclo di conteggio."
20 PRINT "Premi CTRL+C in qualsiasi momento per interrompere l'esecuzione."
30 PRINT

' Inizia un ciclo lungo per dare tempo all'utente di premere CTRL+C.
100 FOR I = 1 TO 100000
  ' Usa LOCATE per aggiornare sempre la stessa riga (riga 8), evitando lo scrolling frenetico.
110   LOCATE 8, 1
120   PRINT "Contatore: "; I; " / 100000. Premi CTRL+C per uscire...";
  ' Ad ogni NEXT, l'interprete dovrebbe controllare se è stato richiesto un Break.
130 NEXT I
140 PRINT

' Se l'utente non preme CTRL+C, il programma arriva qui naturalmente.
200 PRINT "Il ciclo è terminato regolarmente senza interruzioni."
210 END