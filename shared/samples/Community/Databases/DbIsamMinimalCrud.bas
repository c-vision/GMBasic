' FILE: Databases/DbIsamMinimalCrud.bas
' =================================================================================================
'       GUIDA TEORICA: OPERAZIONI CRUD ESSENZIALI CON ISAM
' =================================================================================================
'
' Questo test illustra il ciclo di vita minimo di un database ISAM: definizione, creazione,
' scrittura e lettura puntuale. È l'esempio "Hello World" per il motore di database nativo.
'
' 1. DEFINIZIONE DELLA STRUTTURA (`TYPE`)
'    A differenza dei file DBF che hanno un header dinamico, i file ISAM in GM-BASIC sono
'    strettamente tipizzati. È necessario definire un `TYPE` che rispecchi la struttura
'    dei dati. I campi a lunghezza fissa (`STRING * N`) sono preferiti per le prestazioni.
'
' 2. APERTURA E INDICIZZAZIONE (`ISAM OPEN`)
'    Il comando `ISAM OPEN` gestisce sia l'apertura che la creazione.
'    - `OF Utente`: Lega il file alla definizione della struttura in memoria.
'    - `INDEXED BY ID`: Definisce la chiave primaria. ISAM costruisce e mantiene automaticamente
'      un albero B-Tree bilanciato su questo campo per ricerche O(log n).
'
' 3. SCRITTURA (`APPEND`)
'    `ISAM APPEND` scrive il contenuto di una variabile UDT nel file e aggiorna immediatamente
'    tutti gli indici attivi. Non è necessario un `COMMIT` esplicito a meno che non sia stata
'    avviata una transazione.
'
' 4. RICERCA E LETTURA (`SEEK`, `FOUND`, `GET`)
'    - `SEEK`: Sposta il puntatore del record sulla base del valore della chiave.
'    - `FOUND()`: Restituisce `TRUE` se il `SEEK` ha avuto successo (match esatto).
'    - `GET`: Legge i dati dal record corrente (puntato dal cursore) nella variabile di memoria.
'
' =================================================================================================

IMPORT ISAM
5 IMPORT ISAM
6 IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 CLS
12 PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
14 PRINT "║ DbIsamMinimalCrud - Isam CRUD Minimale                                   ║"
16 PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
18 PRINT

20 REM --- Definizione Struttura Record ---
' Definisce il layout binario del record.
30 TYPE Utente
40   ID AS STRING * 10
50   Nome AS STRING * 50
60   Eta AS INTEGER
70 END TYPE

80 REM --- Creazione e Apertura File ---
90 DIM U AS Utente
' Pulisce l'ambiente da esecuzioni precedenti. ON ERROR serve se il file non esiste.
100 REM Pulisce file precedenti (ON ERROR protegge se non esiste)
101 ON ERROR RESUME NEXT
102 KILL g_work + "/minimal_crud.isdb"
103 ON ERROR GOTO 0
' Apre il database (creandolo se non esiste). L'alias 'Minimal' sarà l'handle per i comandi successivi.
' La clausola 'INDEXED BY ID' crea l'indice primario sul campo ID.
110 ISAM OPEN g_work + "/minimal_crud.isdb" OF Utente ALIAS Minimal INDEXED BY ID

120 REM --- Scrittura (Append) del primo record ---
130 U.ID = "MR001"
140 U.Nome = "Mario"
150 U.Eta = 45
' Scrive il record nel file e aggiorna l'indice B-Tree.
160 ISAM APPEND Minimal, U
170 PRINT "Record per 'Mario' aggiunto."

180 REM --- Ricerca e Lettura (Seek & Get) ---
190 DIM UtenteLetto AS Utente
' Cerca nel B-Tree la chiave "MR001".
200 ISAM SEEK Minimal, "MR001"
' Verifica se la ricerca ha avuto successo.
210 IF ISAM.FOUND(Minimal) THEN
  ' Se trovato, il cursore è posizionato. Legge i dati dal disco alla variabile.
220   ISAM GET Minimal, UtenteLetto
230   PRINT "Nome: "; UtenteLetto.Nome
240 ELSE
250   PRINT "ERRORE: Record MR001 non trovato."
260 END IF

' Chiude il file e rilascia le risorse.
270 ISAM CLOSE Minimal
280 PRINT
290 PRINT "Test minimale completato."
300 END
