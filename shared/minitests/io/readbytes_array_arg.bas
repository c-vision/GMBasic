' READBYTES + parametro array: un buffer BYTE "dinamico" (DIM b() AS BYTE : b = READBYTES(p))
' deve poter essere passato a un parametro `d() AS BYTE` come un array a bound fisso.
' Prima (C99Native) l'argomento veniva rifiutato in emissione ("l'argomento N non è un array")
' mentre IL/PCode/VM-C lo accettavano: era una divergenza di semantica tra le gambe.
' EXPECT: n= 5|first=A|mid=C|chk= 5
IMPORT IO
IMPORT SYSTEM
  CHDIR PROJECTFOLDER()

ON ERROR RESUME NEXT
MKDIR "work"
KILL "work/readbytes_arg.dat"
ON ERROR GOTO 0

FUNCTION LENCHK(d() AS BYTE, n AS LONG) AS LONG
  RETURN n
END FUNCTION

SUB Main()
  ' file di 5 byte: "ABCDE"
  DIM w(4) AS BYTE
  DIM i AS LONG
  FOR i = 0 TO 4 : w(i) = 65 + i : NEXT i
  OPEN "work/readbytes_arg.dat" FOR BINARY AS #1
  PUT #1, , w
  CLOSE #1

  ' buffer DINAMICO assegnato da READBYTES e passato a un parametro array
  DIM b() AS BYTE
  b = READBYTES("work/readbytes_arg.dat")
  DIM n AS LONG = LEN(b)
  PRINT "n="; n
  PRINT "first="; CHR$(b(0))
  PRINT "mid="; CHR$(b(2))
  PRINT "chk="; LENCHK(b, n)
END SUB
