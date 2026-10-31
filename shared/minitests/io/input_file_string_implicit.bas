' INPUT #n, ID, NOME$, ...: la variabile STRINGA IMPLICITA (suffisso $, mai dichiarata) riceve il testo
' del campo CSV. Su PCode / VM-C il compilatore deduceva il tipo dal nome senza suffisso (numerico) e
' NOME$ valeva 0.
' EXPECT: ID= 101 Nome=[Mario Rossi] Saldo= 123.45 Attivo=-1|ID= 102 Nome=[Anna "Neri"] Saldo=-50 Attivo= 0
IMPORT IO

DIM F$
F$ = "bin/input_file_string_implicit.txt"
ON ERROR RESUME NEXT
MKDIR "bin"
ON ERROR GOTO 0
OPEN F$ FOR OUTPUT AS #1
WRITE #1, 101, "Mario Rossi", 123.45, TRUE
WRITE #1, 102, "Anna ""Neri""", -50, FALSE
CLOSE #1

OPEN F$ FOR INPUT AS #1
WHILE NOT IO.EOF(1)
  INPUT #1, ID, NOME$, SALDO, FLAG
  PRINT "ID="; ID; "Nome=["; NOME$; "] Saldo="; SALDO; "Attivo="; FLAG
WEND
CLOSE #1
ON ERROR RESUME NEXT
KILL F$
