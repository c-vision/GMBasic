' EXPECT: l1=[A: Mario               ] len= 23|l2=[R1: Mario               ] len= 24
' PRINT #n, <stringa> (bug PRINTFile-Argomenti-Stringa-PCode-IL-Divergenza, risolto 2026-10-01):
' due problemi distinti, entrambi causati dallo stesso bug radice nel reader di riga per
' INPUT #/LINE INPUT # (PCodeProcessor.Syscalls.cs + IlRuntime.cs creavano un NUOVO StreamReader
' ad ogni chiamata — il buffering interno del reader consumava più byte dello stream di quanti
' ne restituiva, perdendo la SECONDA riga letta e oltre). Copre: (1) il padding di un campo
' STRING*N concatenato, (2) due letture consecutive sullo stesso file handle.
TYPE P
  Nome AS STRING * 20
END TYPE
SUB Main()
  DIM p AS P
  p.Nome = "Mario"
  DIM s AS STRING = "R1: " + p.Nome
  OPEN "/tmp/gmb_mt_printfile_padding.txt" FOR OUTPUT AS #1
  PRINT #1, "A: " + p.Nome
  PRINT #1, s
  CLOSE #1
  OPEN "/tmp/gmb_mt_printfile_padding.txt" FOR INPUT AS #2
  DIM l1 AS STRING
  DIM l2 AS STRING
  INPUT #2, l1
  INPUT #2, l2
  CLOSE #2
  PRINT "l1=["; l1; "] len="; LEN(l1)
  PRINT "l2=["; l2; "] len="; LEN(l2)
  KILL "/tmp/gmb_mt_printfile_padding.txt"
END SUB
