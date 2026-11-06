' EXPECT: blk= 0 : 213 , 43|dblk= 0 : 24 , 12 , 24 : 172 , 186 , 172|img= 8 : 61 , 231|dimg= 8 : 0 , 4 , 8
' FFI gmbgraphics: GFXCODEC — BC1/DXT1 (blocco 4x4 e immagine). Solo calcolo.
IMPORT GFXCODEC

SUB Main()
  DIM rgb(47) AS DOUBLE, enc(7) AS DOUBLE, dec(47) AS DOUBLE
  FOR i = 0 TO 47 : rgb(i) = 10 + (i MOD 16) * 12 : NEXT i

  PRINT "blk="; GFXCODEC.BC1ENCODEBLOCK(rgb(), enc()); ":";
  PRINT enc(0); ","; enc(7)
  PRINT "dblk="; GFXCODEC.BC1DECODEBLOCK(enc(), dec()); ":";
  PRINT dec(0); ","; dec(1); ","; dec(2); ":";
  PRINT dec(45); ","; dec(46); ","; dec(47)

  DIM img(47) AS DOUBLE, enc2(7) AS DOUBLE, dec2(47) AS DOUBLE
  FOR i = 0 TO 47 : img(i) = (i * 5) MOD 256 : NEXT i
  PRINT "img="; GFXCODEC.BC1ENCODE(img(), 4, 4, enc2()); ":";
  PRINT enc2(0); ","; enc2(1)
  PRINT "dimg="; GFXCODEC.BC1DECODE(enc2(), 4, 4, dec2()); ":";
  PRINT dec2(0); ","; dec2(1); ","; dec2(2)
END SUB
