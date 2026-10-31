' FILE: Json/JsonFiles.bas
' =================================================================================================
'       GUIDA TEORICA: I/O SU FILE CON IL NAMESPACE JSON
' =================================================================================================
'
' Questo test dimostra le capacità di I/O (Input/Output) di GM-BASIC specifiche per la
' gestione di file contenenti dati in formato JSON (comandi legacy a spazio).
'
' 1. SINTASSI LEGACY PER I/O (`JSON OPEN`)
'    Per semplificare e rendere più intuitivo l'accesso ai file JSON, GM-BASIC fornisce
'    una sintassi dedicata all'interno del namespace JSON.
'    - `JSON.OPEN file$ FOR mode$ AS #handleVar`: Questo comando astrae la vecchia sintassi `OPEN ... TYPE JSON`.
'      - `file$`: Il percorso del file.
'      - `modo$`: Una stringa che specifica la modalità ("READ", "WRITE", "APPEND").
'      - `handleVar`: Una variabile numerica che riceverà l'handle del file.
'    - Questa sintassi è più leggibile e coerente con il resto del namespace JSON.
'
' 2. IL TIPO `JSON` COME TARGET DI I/O
'    A differenza dei tipi di dato standard, il tipo `JSON` può essere usato direttamente
'    nelle operazioni di lettura e scrittura su file.
'    - `PRINT #handle, jsonObj`: Serializza l'oggetto `jsonObj` e lo scrive sul file.
'    - `INPUT #handle, jsonObj`: Legge l'intero contenuto del file, lo parsa come JSON
'      e lo assegna alla variabile `jsonObj`.
'
' 3. INTROSPEZIONE CON `JSON.KEYS()`
'    Per rendere il tipo `JSON` potente quanto il tipo `DICT`, è disponibile la funzione
'    `JSON.KEYS()`, che restituisce un array di stringhe contenente i nomi di tutte le
'    proprietà di primo livello dell'oggetto JSON.
'
' =================================================================================================

' Importa il namespace JSON per rendere disponibili i comandi e le funzioni.
IMPORT JSON
IMPORT IO

SUB TestJsonFileIO
  DIM FILE_JSON$ AS STRING
  ' Output di categoria 4 (AGENTS §11): in `work/` accanto al sample, mai nella cartella del
  ' sample (pattern `**/work/` gitignorato).
  IF NOT IO.DIREXISTS("work") THEN MKDIR "work"
  FILE_JSON$ = "work/data.json"
  DIM handle AS LONG

  PRINT "--- 1. Creazione e scrittura di un oggetto JSON su file ---"
  DIM Dati AS JSON
  Dati = JSON.Parse("{ ""utente"": ""Mario"", ""punteggio"": 100, ""attivo"": true }")
  PRINT "Oggetto JSON in memoria:"
  PRINT Dati
  PRINT
  
  PRINT "Salvo questo oggetto JSON in '"; FILE_JSON$; "' usando JSON OPEN..."
  ' Apre il file per la scrittura.
  JSON OPEN FILE_JSON$ FOR "WRITE" AS #handle
  
  ' Scrive l'oggetto JSON nel file associato all'handle.
  PRINT #handle, Dati
  CLOSE #handle
  PRINT "Salvataggio completato."
  PRINT

  PRINT "--- 2. Lettura del file JSON in una nuova variabile ---"
  DIM DatiLetti AS JSON
  PRINT "Apro '"; FILE_JSON$; "' per la lettura usando JSON OPEN..."
  ' Riapre il file in lettura.
  JSON OPEN FILE_JSON$ FOR "READ" AS #handle
  ' Legge l'intero file, lo parsa e assegna l'oggetto JSON risultante a 'DatiLetti'.
  INPUT #handle, DatiLetti
  CLOSE #handle
  PRINT "Lettura completata."
  PRINT "Oggetto JSON letto da file:"
  PRINT DatiLetti
  PRINT

  PRINT "--- 3. Verifica dei dati letti e test di JSON.KEYS() ---"
  PRINT "Utente letto: "; DatiLetti.utente; " (Atteso: Mario)"
  PRINT "Punteggio letto: "; DatiLetti.punteggio; " (Atteso: 100)"
  PRINT "Attivo letto: "; DatiLetti.attivo; " (Atteso: True)"
  
  ' Test della funzione JSON.KEYS()
  DIM chiavi() AS STRING = JSON.KEYS(DatiLetti)
  PRINT "Chiavi dell'oggetto (da JSON.KEYS()): "; JOIN(chiavi, ", ")
  PRINT

  ' --- Sezione di pulizia alla fine della sub ---
  PRINT "Eseguo pulizia finale..."
  KILL FILE_JSON$
  PRINT "Pulizia completata."
  
END SUB

10 ' --- Programma Principale (Lanciatore) ---
20 ON ERROR GOTO gestore_errori
21 CLS
22 PRINT "╔════════════════════════════════════════════════════════════════════════╗"
23 PRINT "║ JsonFiles - Files JSON (Sintassi Legacy)                              ║"
24 PRINT "╚════════════════════════════════════════════════════════════════════════╝"
25 PRINT
40 TestJsonFileIO()  ' Chiama la subroutine principale
50 GOTO fine

gestore_errori:
60 PRINT "ERRORE CATTURATO: #" ; ERR.Number; " ALLA RIGA "; ERR.LineNumber; " - "; ERR.Description
70 CLOSE ' Tenta di chiudere tutti i file aperti in caso di errore.

fine:
80 PRINT
90 PRINT "--- Test JSON I/O completato. ---"
100 END