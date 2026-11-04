' PUT/GET #n, rec, <UDT> su un file RANDOM il cui LEN e' minore del layout canonico dell'UDT:
' errore BASIC 59 "Bad record length" (catturabile) e nessun byte scritto.
' EXPECT: put_caught=1|get_caught=1|size= 0|ok_after=-1
IMPORT IO

TYPE Grande
  A AS LONG
  Nome AS STRING * 40
END TYPE

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM g AS Grande, h AS Grande
  DIM path AS STRING = GMBWORK$ + "/random_udt_toolong.dat"
  ON ERROR RESUME NEXT
  MKDIR GMBWORK$
  KILL path
  ON ERROR GOTO 0
  g.A = 7 : g.Nome = "troppo lungo"

  OPEN path FOR RANDOM AS #1 LEN = 16
  TRY
    PUT #1, 1, g
    PRINT "put_caught=0"
  CATCH
    PRINT "put_caught=1"
  END TRY
  TRY
    GET #1, 1, h
    PRINT "get_caught=0"
  CATCH
    PRINT "get_caught=1"
  END TRY
  CLOSE #1
  DIM bytes = IO.READBYTES(path)
  PRINT "size="; UBOUND(bytes) + 1

  ' con LEN adeguato (49 = 8 + 41) lo stesso PUT riesce
  OPEN path FOR RANDOM AS #1 LEN = 49
  PUT #1, 1, g
  GET #1, 1, h
  PRINT "ok_after="; (h.A = 7)
  CLOSE #1
  ON ERROR RESUME NEXT
  KILL path
END SUB
