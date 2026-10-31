' FILE: Files/FilesIO.bas
' =================================================================================================
'       GUIDA TEORICA: MODALITÀ DI ACCESSO AI FILE (SEQUENTIAL, RANDOM, BINARY)
' =================================================================================================
'
' Questo test esplora le tre modalità fondamentali di accesso ai file in GM-BASIC.
'
' 1. ACCESSO SEQUENZIALE (`OUTPUT`, `APPEND`, `INPUT`)
'    Ideale per file di testo. I dati vengono scritti e letti in ordine, riga per riga.
'    - `WRITE #`: Scrive dati formattati (virgole tra campi, stringhe tra virgolette).
'      Pensato per essere riletto facilmente da `INPUT #`.
'    - `PRINT #`: Scrive dati "raw" esattamente come appaiono a video.
'    - `INPUT #`: Legge dati strutturati (separati da virgole/a capo) in variabili.
'    - `IO.FINPUT(n, handle)`: Legge esattamente `n` caratteri grezzi.
'
' 2. ACCESSO CASUALE (`RANDOM`)
'    Gestisce file strutturati a record di lunghezza fissa. È il precursore dei database.
'    - `OPEN ... LEN=n`: Definisce la lunghezza del record.
'    - `PUT #h, recNum, var`: Scrive la variabile (solitamente un UDT) alla posizione `recNum`.
'    - `GET #h, recNum, var`: Legge il record `recNum` nella variabile.
'    - Accesso O(1): È possibile saltare direttamente a qualsiasi record senza leggere i precedenti.
'
' 3. ACCESSO BINARIO (`BINARY`)
'    Tratta il file come un flusso di byte non strutturato. Nessun concetto di "record".
'    - `PUT #h, , var`: Scrive i byte della variabile alla posizione corrente del cursore.
'    - `GET #h, , var`: Legge i byte nella variabile dalla posizione corrente.
'    - `IO.SEEK(h, pos)`: Sposta arbitrariamente il cursore di lettura/scrittura (byte-level).
'    - Ideale per formati di file personalizzati, immagini, o array di dati grezzi.
'
' 4. GESTIONE FILE SYSTEM
'    - `IO.EOF(h)`: True se il cursore è alla fine del file.
'    - `IO.LOF(h)`: Length Of File (dimensione in byte).
'    - `IO.FREEFILE()`: Restituisce il primo handle numerico disponibile.
'    - `KILL`, `RENAME`: Operazioni sul file system.
'
' =================================================================================================

IMPORT IO
IMPORT STRINGS

' Definizione di un UDT per testare l'accesso RANDOM.
TYPE DatiCliente
  ID AS LONG
  Nome AS STRING * 20
  Saldo AS CURRENCY
  Attivo AS BIT
END TYPE

1 CLS
2 PRINT "╔═════════════════════════════════════════════════════════════════════════╗"
3 PRINT "║ FilesIO - I/O su File                                                   ║"
4 PRINT "╚═════════════════════════════════════════════════════════════════════════╝"
5 PRINT
20 DIM CLIENTE AS DatiCliente, CLIENTE_LETTO AS DatiCliente
30 DIM FILE_SEQ$, FILE_RND$, FILE_BIN$, FILE_RND_NEW$
' Definizione percorsi file (locali per test in qualsiasi ambiente).
40 FILE_SEQ$ = "sequential.txt"
50 FILE_RND$ = "random.dat"
60 FILE_BIN$ = "binary.dat"
70 FILE_RND_NEW$ = "clienti.dat"
80 ON ERROR GOTO gestore_errori

100 PRINT "--- 1. TEST SCRITTURA SEQUENZIALE (WRITE #) ---"
' Apre il file in modalità scrittura (sovrascrive se esiste).
110 OPEN FILE_SEQ$ FOR WRITE AS #1
120 PRINT "Scrivo 3 record formattati in '"; FILE_SEQ$; "'"
' WRITE # inserisce automaticamente virgole e virgolette per le stringhe.
130 WRITE #1, 101, "Mario Rossi", 123.45, TRUE
140 WRITE #1, 102, "Luigi Verdi", -50.00, FALSE
150 WRITE #1, 103, "Anna ""Neri""", 999.99, TRUE
160 CLOSE #1
170 PRINT "Scrittura completata."
180 PRINT

200 PRINT "--- 2. TEST LETTURA SEQUENZIALE (INPUT #) ---"
' Riapre lo stesso file in lettura.
210 OPEN FILE_SEQ$ FOR INPUT AS #1
220 PRINT "Leggo e stampo i record da '"; FILE_SEQ$; "'"
' Ciclo fino alla fine del file.
230 WHILE NOT IO.EOF(1)
  ' INPUT # legge i valori separati da virgola e li assegna alle variabili tipizzate.
  240   INPUT #1, ID, NOME$, SALDO, FLAG
  250   PRINT "  ID:"; ID, "Nome: '"; NOME$; "'", "Saldo:"; SALDO, "Attivo:"; FLAG
260 WEND
270 CLOSE #1
280 PRINT

300 PRINT "--- 3. TEST ACCESSO RANDOM (PUT # / GET #) ---"
' Calcolo manuale della lunghezza del record (somma dei byte dei tipi nel TYPE).
315 LUNGHEZZA_REC = 8 + 21 + 16 + 1 //LONG(8)+STRING*20(20+NUL)+CURRENCY(16)+BIT(1) = layout packed del record
' Apre in modalità RANDOM specificando la lunghezza del record.
320 OPEN FILE_RND$ FOR RANDOM AS #2 LEN = LUNGHEZZA_REC
330 PRINT "Aperto '"; FILE_RND$; "' con record di lunghezza"; LUNGHEZZA_REC
350 CLIENTE.ID = 303 : CLIENTE.Nome = "Paolo Gialli" : CLIENTE.Saldo = 1500.75 : CLIENTE.Attivo = TRUE
' Scrive direttamente al record numero 3 (lasciando 1 e 2 vuoti/zeri).
360 PUT #2, 3, CLIENTE
' Rilegge lo stesso record numero 3.
380 GET #2, 3, CLIENTE_LETTO
390 PRINT "Letto record #3 -> ID:"; CLIENTE_LETTO.ID; " (Atteso 303)"
400 CLOSE #2
410 PRINT

500 PRINT "--- 4. TEST ACCESSO BINARIO (PUT # / GET # con Array) ---"
510 DIM DATI_BYTE(10) AS BYTE
520 FOR I = 0 TO 10 : DATI_BYTE(I) = I * 10 : NEXT I
' Apre in modalità BINARY. Non ci sono record, solo un flusso di byte.
530 OPEN FILE_BIN$ FOR BINARY AS #1
540 PRINT "Scrivo un array di 11 byte nel file binario..."
' Scrive l'intero array. Il secondo parametro (posizione) è omesso, quindi scrive alla posizione corrente.
550 PUT #1, , DATI_BYTE
' Verifica la posizione del cursore (dovrebbe essere avanzato di 11 byte: IO.SEEK e' 0-based).
560 PRINT "Posizione dopo scrittura (IO.SEEK(1)):"; IO.SEEK(1); "(Atteso 11)"
' Sposta il cursore all'inizio del file (byte 1).
570 SEEK 1, 1 //Torna all'inizio
580 DIM DATI_LETTI(10) AS BYTE
' Legge 11 byte nell'array.
590 GET #1, , DATI_LETTI
600 CLOSE #1
610 PRINT "Letti 11 byte. Verifico il 5o elemento (indice 4)..."
620 PRINT "  DATI_LETTI(4) ="; DATI_LETTI(4); "(Atteso 40)"
630 PRINT

700 PRINT "--- 5. TEST FUNZIONI E ALTRI COMANDI ---"
710 OPEN FILE_SEQ$ FOR INPUT AS #1
' IO.LOF restituisce la dimensione totale del file.
720 PRINT "IO.LOF('"; FILE_SEQ$; "') ="; IO.LOF(1); "bytes"
' IO.FINPUT legge un numero preciso di caratteri (raw read).
730 PRIMI_10$ = IO.FINPUT(10, 1)
740 PRINT "IO.FINPUT(10, 1) = '"; PRIMI_10$; "'"
750 CLOSE #1
' IO.FREEFILE restituisce il prossimo handle libero (utile per non confliggere).
760 PRINT "Prossimo handle libero (IO.FREEFILE):"; IO.FREEFILE()
' Rinomina un file.
770 RENAME FILE_RND$ AS FILE_RND_NEW$
780 PRINT "File '"; FILE_RND$; "' rinominato in '"; FILE_RND_NEW$; "'."
790 GOTO pulizia_files

gestore_errori:
810 PRINT "ERRORE INASPETTATO: #" ; ERR.Number; " ALLA RIGA "; ERR.LineNumber
820 CLOSE #1

pulizia_files:
830 PRINT "Eseguo pulizia finale dei file..."
840 ON ERROR RESUME NEXT
' Cancella tutti i file creati durante il test.
850 KILL FILE_SEQ$
860 KILL FILE_RND$
870 KILL FILE_RND_NEW$
880 KILL FILE_BIN$
890 ON ERROR GOTO 0
900 PRINT "Pulizia completata."

fine:
910 PRINT "--- Test I/O completato. ---"
920 END