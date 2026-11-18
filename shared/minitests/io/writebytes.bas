' IO.WRITEBYTES(path, array di BYTE): scrive i byte nell'ordine dell'array e ritorna il
' numero di byte scritti (-1 su errore). A differenza di IO.FWRITE (fputs/CryptoBytes) NON si
' ferma al primo 0x00 e NON reinterpreta i valori come testo: il file ha esattamente UBound+1 byte.
' Il file viene ricreato (troncato) a ogni chiamata: una scrittura piu' corta accorcia il file.
' EXPECT: n= 4|ub= 3|size= 4|n1= 1|size1= 1|err=-1
IMPORT IO
  CHDIR PROJECTFOLDER()

ON ERROR RESUME NEXT
MKDIR "work"
KILL "work/writebytes.dat"
ON ERROR GOTO 0

DIM B(3) AS BYTE
B(0) = 66
B(1) = 77
B(2) = 0
B(3) = 255

N = IO.WRITEBYTES("work/writebytes.dat", B)
PRINT "n="; N

DIM R()
R = IO.READBYTES("work/writebytes.dat")
PRINT "ub="; UBOUND(R)

H = IO.FOPEN("work/writebytes.dat")
PRINT "size="; IO.LOF(H)
IO.FCLOSE(H)

' riscrittura piu' corta: il file deve restare di 1 byte (troncamento, non append)
DIM ONE(0) AS BYTE
ONE(0) = 7
N1 = IO.WRITEBYTES("work/writebytes.dat", ONE)
PRINT "n1="; N1
H = IO.FOPEN("work/writebytes.dat")
PRINT "size1="; IO.LOF(H)
IO.FCLOSE(H)

' percorso non scrivibile: errore esplicito, mai un falso successo
PRINT "err="; IO.WRITEBYTES("work/writebytes.dat/x.dat", ONE)   ' ENOTDIR: il padre e' un FILE, mai una directory

ON ERROR RESUME NEXT
KILL "work/writebytes.dat"
