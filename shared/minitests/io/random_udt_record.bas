' PUT/GET #n, rec, <UDT> su file RANDOM: il record e' letto/scritto direttamente nei campi dell'UDT.
' Layout canonico (SIZEOF, packed, little-endian): LONG=8, STRING*n=n+1 (spazi + NUL), CURRENCY=16
' (int64 valore*10000 + 8 byte a zero), BIT=1, BYTE=1, INTEGER=2, SINGLE=4, DOUBLE=8; record = LEN byte,
' offset (rec-1)*LEN. Verifica sia il round trip sia i BYTE su disco (identici su tutti i backend).
' EXPECT: ID= 303|Nome=[Paolo Gialli        ]|Saldo= 1500.75|Attivo=-1|vuoto ID= 0 Nome=[                    ]|seq1=Anna|seq2=Bruno|misto= 200 -1234  1.5  3.14159265358979  123456789012 [abc]|size= 192|b0= 47  1|nome= 80  0|saldo= 12  255  228|attivo= 1  0|rec1= 0
IMPORT IO

TYPE Cliente
  ID AS LONG
  Nome AS STRING * 20
  Saldo AS CURRENCY
  Attivo AS BIT
END TYPE

TYPE Misto
  B AS BYTE
  I AS INTEGER
  S AS SINGLE
  D AS DOUBLE
  Q AS QUAD
  T AS STRING * 3
END TYPE

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM c AS Cliente, d AS Cliente, x AS Cliente, y AS Cliente
  DIM m AS Misto, n AS Misto
  DIM path AS STRING = GMBWORK$ + "/random_udt_record.dat"
  ON ERROR RESUME NEXT
  MKDIR GMBWORK$
  KILL path
  ON ERROR GOTO 0

  c.ID = 303 : c.Nome = "Paolo Gialli" : c.Saldo = 1500.75 : c.Attivo = TRUE
  OPEN path FOR RANDOM AS #2 LEN = 64
  PUT #2, 3, c
  GET #2, 3, d
  PRINT "ID="; d.ID
  PRINT "Nome=["; d.Nome; "]"
  PRINT "Saldo="; d.Saldo
  PRINT "Attivo="; d.Attivo
  GET #2, 1, d
  PRINT "vuoto ID="; d.ID; "Nome=["; d.Nome; "]"
  CLOSE #2

  ' accesso sequenziale (record omesso = posizione corrente)
  OPEN path FOR RANDOM AS #3 LEN = 64
  x.ID = 1 : x.Nome = "Anna" : x.Saldo = 0 : x.Attivo = FALSE
  y.ID = 2 : y.Nome = "Bruno" : y.Saldo = 0 : y.Attivo = FALSE
  PUT #3, 1, x
  PUT #3, , y
  GET #3, 1, d
  PRINT "seq1="; TRIM(d.Nome)
  GET #3, , d
  PRINT "seq2="; TRIM(d.Nome)
  CLOSE #3

  ' tipi misti
  m.B = 200 : m.I = -1234 : m.S = 1.5 : m.D = 3.14159265358979 : m.Q = 123456789012 : m.T = "abcdef"
  OPEN GMBWORK$ + "/random_udt_misto.dat" FOR RANDOM AS #4 LEN = 32
  PUT #4, 1, m
  GET #4, 1, n
  PRINT "misto="; n.B; n.I; n.S; n.D; n.Q; "["; n.T; "]"
  CLOSE #4

  ' byte su disco del file clienti: record 3 (offset 128) al posto di 2 dopo le PUT sequenziali
  OPEN path FOR RANDOM AS #5 LEN = 64
  PUT #5, 2, c
  CLOSE #5
  DIM bytes = IO.READBYTES(path)
  PRINT "size="; UBOUND(bytes) + 1
  PRINT "b0="; bytes(64); bytes(65)
  PRINT "nome="; bytes(72); bytes(92)
  PRINT "saldo="; bytes(93); bytes(94); bytes(95)
  PRINT "attivo="; bytes(109); bytes(110)
  PRINT "rec1="; bytes(60)
  ON ERROR RESUME NEXT
  KILL path
  KILL GMBWORK$ + "/random_udt_misto.dat"
END SUB
