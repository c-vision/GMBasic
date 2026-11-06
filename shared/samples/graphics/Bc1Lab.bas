REM FILE: Bc1Lab.bas — GFXCODEC: compressione texture BC1/DXT1.
REM Scenario: un blocco 4x4 a gradiente viene compresso in 8 byte e ricostruito; poi una
REM immagine 8x4 viene compressa/decodificata e si confrontano i valori. Deterministico.
IMPORT GFXCODEC

SUB Main()
  DIM rgb(47) AS DOUBLE, enc(7) AS DOUBLE, dec(47) AS DOUBLE
  DIM i AS LONG

  ' blocco 4x4: gradiente diagonale grigio
  FOR i = 0 TO 15
    rgb(i * 3 + 0) = i * 16
    rgb(i * 3 + 1) = i * 16
    rgb(i * 3 + 2) = i * 16
  NEXT i

  PRINT "blk="; GFXCODEC.BC1ENCODEBLOCK(rgb(), enc()); ":";
  PRINT enc(0); ","; enc(1); ","; enc(7)

  PRINT "dblk="; GFXCODEC.BC1DECODEBLOCK(enc(), dec()); ":";
  PRINT dec(0); ","; dec(1); ","; dec(2); ":";
  PRINT dec(45); ","; dec(46); ","; dec(47)

  ' immagine 8x4 (2x1 blocchi)
  DIM img(95) AS DOUBLE, enc2(15) AS DOUBLE, dec2(95) AS DOUBLE
  FOR i = 0 TO 31
    img(i * 3 + 0) = (i * 8) MOD 256
    img(i * 3 + 1) = 255 - ((i * 8) MOD 256)
    img(i * 3 + 2) = 128
  NEXT i
  PRINT "img="; GFXCODEC.BC1ENCODE(img(), 8, 4, enc2()); ":";
  PRINT enc2(0); ","; enc2(8); ","; enc2(15)

  PRINT "dimg="; GFXCODEC.BC1DECODE(enc2(), 8, 4, dec2()); ":";
  PRINT dec2(0); ","; dec2(1); ","; dec2(2); ":";
  PRINT dec2(93); ","; dec2(94); ","; dec2(95)
END SUB
