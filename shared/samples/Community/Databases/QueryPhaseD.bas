IMPORT ISAM
IMPORT JSON
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

10 PRINT "QueryPhaseD — ISAM fixed UDT + Phase D patterns"
20 PRINT

30 TYPE ScoredRow
40   ID AS STRING * 8
50   Name AS STRING * 32
60   Score AS LONG
70 END TYPE

80 DIM R AS ScoredRow
90 KILL g_work + "/query_phase_d.isdb" : ON ERROR RESUME NEXT
100 ISAM OPEN g_work + "/query_phase_d.isdb" OF ScoredRow ALIAS Scored INDEXED BY ID

110 R.ID = "1" : R.Name = "Alpha" : R.Score = 10
120 ISAM APPEND Scored, R
130 R.ID = "2" : R.Name = "Bravo" : R.Score = 0
140 ISAM APPEND Scored, R
150 R.ID = "3" : R.Name = "Charlie" : R.Score = 25
160 ISAM APPEND Scored, R

170 ISAM GO Scored, TOP
180 WHILE NOT ISAM.EOF(Scored)
190   ISAM GET Scored, R
200   PRINT "  Id="; R.ID; " Name="; R.Name; " Score="; R.Score
210   ISAM SKIP Scored
220 WEND

230 ' INTEGRITY ritorna il TESTO JSON: si tiene in una stringa e si converte con JSON.PARSE
231 ' (assegnare direttamente a una variabile AS JSON fa divergere i backend: il valore è testo).
232 DIM checkText AS STRING = ISAM.INTEGRITY(Scored)
233 DIM checkJson AS JSON = JSON.PARSE(checkText)
240 DIM infoJson AS JSON = ISAM.INFO(Scored)
250 PRINT "Integrity valid: "; JSON.EXTRACT(checkJson, "valid")
260 PRINT "Active records: "; JSON.EXTRACT(infoJson, "active_records")

270 ISAM CLOSE Scored
280 END
