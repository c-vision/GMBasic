' FILE: Databases/DbIsamNavAndModify.bas
IMPORT ISAM
IMPORT IO
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 TYPE Cliente
20   ID AS STRING * 10
30   Cognome AS STRING * 50
40   Eta AS INTEGER
50 END TYPE

60 KILL g_work + "/clienti.isdb" : ON ERROR RESUME NEXT
70 DIM C AS Cliente
80 ISAM OPEN g_work + "/clienti.isdb" OF Cliente ALIAS Clienti INDEXED BY ID

90 PRINT "Aggiunti 3 record."
100 C.ID = "001" : C.Cognome = "Rossi" : C.Eta = 30 : ISAM APPEND Clienti, C
110 C.ID = "002" : C.Cognome = "Verdi" : C.Eta = 25 : ISAM APPEND Clienti, C
120 C.ID = "003" : C.Cognome = "Bianchi" : C.Eta = 40 : ISAM APPEND Clienti, C

130 PRINT : PRINT "--- 1. Scansione Sequenziale (SKIP +1) ---"
140 ISAM GO Clienti, TOP
150 WHILE NOT ISAM.EOF(Clienti)
160   ISAM GET Clienti, C
170   PRINT "  ID:"; C.ID, "Cognome:"; C.Cognome
180   ISAM SKIP Clienti
190 WEND

200 PRINT : PRINT "--- 2. Modifica Record (PUT) ---"
210 ISAM SEEK Clienti, "002"
220 IF ISAM.FOUND(Clienti) THEN
230   ISAM GET Clienti, C
240   C.Eta = 26
250   ISAM PUT Clienti, C
260   PRINT "  Nuova eta di Verdi:", C.Eta
270 END IF

280 PRINT : PRINT "--- 3. Scansione Inversa (SKIP -1) ---"
290 ISAM GO Clienti, BOTTOM
300 WHILE NOT ISAM.BOF(Clienti)
310   ISAM GET Clienti, C
320   PRINT "  ID:"; C.ID, "Cognome:"; C.Cognome
330   ISAM SKIP Clienti, -1
340 WEND

350 PRINT : PRINT "--- 4. Cancellazione e PACK ---"
360 ISAM DELETE Clienti
370 ISAM PACK Clienti
380 PRINT "Numero record dopo PACK:"; ISAM.LASTREC(Clienti)

390 ISAM CLOSE Clienti
