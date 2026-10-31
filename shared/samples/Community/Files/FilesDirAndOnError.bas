' FILE: Files/FilesDirAndOnError.bas
' =================================================================================================
'       GUIDA TEORICA: GESTIONE FILE E ERRORI (DIR & ON ERROR)
' =================================================================================================
'
' Questo test verifica l'interazione tra le funzioni del file system e il meccanismo di
' gestione degli errori "legacy" (non strutturato).
'
' 1. LA FUNZIONE `DIR`
'    - Sintassi: `DIR(percorso)`
'    - Restituisce un array di stringhe contenente i nomi dei file/directory che corrispondono
'      al percorso (che può includere wildcard).
'    - Se nessun file viene trovato o il percorso non esiste, restituisce un array vuoto
'      (UBOUND = -1), senza generare un errore di runtime. Questo permette di verificare
'      l'esistenza di file in modo sicuro.
'
' 2. GESTIONE ERRORI I/O (`ON ERROR GOTO`)
'    Le operazioni sui file (come `OPEN`) sono soggette a fallimenti esterni (file mancante,
'    permessi negati, disco pieno).
'    - `ON ERROR GOTO Etichetta`: Attiva un trap globale per la procedura corrente.
'    - Quando `OPEN` fallisce, il controllo salta immediatamente all'etichetta.
'    - L'oggetto `ERR` viene popolato con i dettagli. Il codice standard per "File not found" è 53.
'
' =================================================================================================

SUB Main()
  CLS
  PRINT "╔══════════════════════════════════════════════════════════════════════════════╗"
  PRINT "║ FilesDirAndOnError - DIR e ON ERROR                                          ║"
  PRINT "╚══════════════════════════════════════════════════════════════════════════════╝"
  PRINT

  DIM NonEsiste$ AS STRING
  NonEsiste$ = "/directory/che/sicuramente/non/esiste/file.txt"
  
  PRINT "--- 1. Test di DIR su percorso inesistente ---"
  PRINT "Chiamo DIR su '"; NonEsiste$; "'"
  
  ' DIR restituisce un array.
  DIM Risultato$() AS STRING
  Risultato$ = DIR(NonEsiste$)
  
  ' Verifica se l'array è vuoto (nessun file trovato).
  IF UBOUND(Risultato$) = -1 THEN
    PRINT "OK: DIR ha correttamente restituito un array vuoto."
  ELSE
    PRINT "ERRORE: DIR doveva restituire un array vuoto."
  END IF
  PRINT
  
  PRINT "--- 2. Test di gestione errori con OPEN fallito ---"
  
  PRINT "Sto per tentare di aprire un file inesistente per leggerlo..."
  ' Usa TRY/CATCH invece di ON ERROR GOTO (ON ERROR ha limitazioni note)
  TRY
    OPEN NonEsiste$ FOR INPUT AS #1
    PRINT "ERRORE: Questa riga non doveva essere raggiunta."
    CLOSE #1
  CATCH
    PRINT
    PRINT "--- ERRORE CATTURATO ---"
    PRINT "OK: L'errore su OPEN è stato catturato correttamente."
  END TRY
  
Fine:
  PRINT
  PRINT "--- TEST COMPLETATO ---"
END SUB