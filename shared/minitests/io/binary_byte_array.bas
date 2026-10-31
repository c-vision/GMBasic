' PUT/GET #n, [pos], <array di BYTE> su file BINARY: un byte grezzo per elemento (pos > 0 = offset di
' byte pos-1; assente = posizione corrente). GET riempie l'array e legge 0 oltre la fine del file.
' Il contatore FOR IMPLICITO usato come indice/valore di un array di BYTE (`B(I) = I * 10`) deve restare
' numerico: prima il linker lo restringeva a BYTE (local uint8 su IL, indici errati / overflow a 255).
' EXPECT: pos= 11|b4= 40 b10= 100|ofs= 20  30  40|tail= 0  0|size= 11|big= 300
IMPORT IO

DIM FILE_BIN$
FILE_BIN$ = "work/binary_byte_array.dat"
ON ERROR RESUME NEXT
MKDIR "work"
KILL FILE_BIN$
ON ERROR GOTO 0

DIM SRC(10) AS BYTE
FOR I = 0 TO 10 : SRC(I) = I * 10 : NEXT I

OPEN FILE_BIN$ FOR BINARY AS #1
PUT #1, , SRC
PRINT "pos="; IO.SEEK(1)
SEEK 1, 1
DIM DST(10) AS BYTE
GET #1, , DST
PRINT "b4="; DST(4); "b10="; DST(10)

' lettura da un offset di byte (pos 3 = terzo byte → 20) e oltre la fine del file (zeri)
DIM PART(2) AS BYTE
GET #1, 3, PART
PRINT "ofs="; PART(0); PART(1); PART(2)
DIM TAIL(1) AS BYTE
GET #1, 12, TAIL
PRINT "tail="; TAIL(0); TAIL(1)
PRINT "size="; IO.LOF(1)
CLOSE #1

' contatore implicito che supera 255: non deve essere un BYTE
DIM N(3) AS BYTE
DIM TOT
FOR K = 0 TO 300
  TOT = K
NEXT K
PRINT "big="; TOT
ON ERROR RESUME NEXT
KILL FILE_BIN$
