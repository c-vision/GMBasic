' FILE: Concurrency/GoRoutinesMinimal.bas
' =================================================================================================
'       GUIDA TEORICA: CONCORRENZA BASILARE (GOROUTINE, CANALI, WAITGROUP)
' =================================================================================================
'
' Questo test introduce il modello di concorrenza di GM-BASIC, ispirato a quello del linguaggio Go.
' Permette l'esecuzione parallela (o concorrente) di flussi di codice comunicanti.
'
' 1. GOROUTINE (`GO SUB` / `GO FUNCTION`)
'    Una Goroutine è un "lightweight thread" (task) gestito dallo scheduler del runtime (GPM).
'    - `GO SUB NomeProcedura(args...)`: Avvia la procedura in un nuovo task concorrente.
'      L'esecuzione del chiamante prosegue immediatamente senza attendere la fine della procedura.
'
' 2. WAITGROUP (`WAITGROUP`)
'    Meccanismo di sincronizzazione per attendere il completamento di un gruppo di task.
'    - `DIM wg AS WAITGROUP`: Dichiara un gruppo di attesa.
'    - `GO ... IN wg`: Sintassi speciale che incrementa automaticamente il contatore del WaitGroup
'      prima di avviare la goroutine.
'    - `DONE wg`: Decrementa il contatore del WaitGroup. Va chiamato alla fine della goroutine.
'    - `WAIT wg`: Blocca il task corrente finché il contatore del WaitGroup non torna a zero.
'
' 3. CANALI (`CHANNEL`)
'    Condotti tipizzati per la comunicazione e la sincronizzazione tra goroutine.
'    - `DIM c AS CHANNEL OF TIPO`: Crea un canale che trasporta valori di tipo `TIPO`.
'    - `SEND valore TO c`: Invia un valore nel canale. È un'operazione bloccante: se il canale
'      è pieno (o non ha buffer), il mittente attende che qualcuno riceva.
'    - `RECEIVE var FROM c`: Riceve un valore dal canale. È un'operazione bloccante: se il
'      canale è vuoto, il ricevente attende che qualcuno invii.
'
' =================================================================================================

' --- Subroutine PRODUTTORE ---
' Questa procedura verrà eseguita come una Goroutine indipendente.
' Invia una sequenza di numeri sul canale e segnala il completamento tramite WaitGroup.
SUB Produttore(C AS CHANNEL OF INTEGER, WG AS WAITGROUP)
  PRINT "[PRODUTTORE] Avviato."
  
  PRINT "[PRODUTTORE] Invio 10..."
  ' Invia il valore 10 al canale. Se il Main non è pronto a ricevere, questa istruzione blocca.
  SEND 10 TO C
  
  ' Simula un lavoro o un ritardo per dimostrare l'indipendenza temporale dei task.
  SLEEP 100 
  
  PRINT "[PRODUTTORE] Invio 20..."
  SEND 20 TO C
  
  SLEEP 100
  
  PRINT "[PRODUTTORE] Invio 30..."
  SEND 30 TO C
  
  PRINT "[PRODUTTORE] Finito. Eseguo DONE."
  ' Segnala al WaitGroup che questo task è terminato (decrementa il contatore).
  DONE WG
END SUB

' --- PROGRAMMA PRINCIPALE ---
SUB Main()
  CLS
  PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ GoRoutinesMinimal - Concorrenza (Goroutines) Minimale                        ║"
  PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
  PRINT
  
  ' Dichiarazione degli oggetti di sincronizzazione.
  DIM Canale AS CHANNEL OF INTEGER
  DIM Gruppo AS WAITGROUP
  DIM Valore AS INTEGER
  
  PRINT "[MAIN] Avvio GO SUB Produttore..."
  ' Avvia la subroutine Produttore in un nuovo task concorrente.
  ' La clausola 'IN Gruppo' incrementa automaticamente il contatore del WaitGroup.
  GO SUB Produttore(Canale, Gruppo) IN Gruppo
  
  ' Il Main ora agisce come CONSUMATORE.
  
  PRINT "[MAIN] Ricevo primo valore..."
  ' Si blocca qui finché il Produttore non invia un dato.
  RECEIVE Valore FROM Canale
  PRINT "[MAIN] Ricevuto: "; Valore
  
  PRINT "[MAIN] Ricevo secondo valore..."
  RECEIVE Valore FROM Canale
  PRINT "[MAIN] Ricevuto: "; Valore
  
  PRINT "[MAIN] Ricevo terzo valore..."
  RECEIVE Valore FROM Canale
  PRINT "[MAIN] Ricevuto: "; Valore
  
  PRINT "[MAIN] Attendo il completamento del produttore..."
  ' Attende che il contatore del WaitGroup scenda a zero (cioè che Produttore chiami DONE).
  WAIT Gruppo
  PRINT "[MAIN] Produttore terminato."
  PRINT
  PRINT "--- TEST COMPLETATO ---"
  
END SUB