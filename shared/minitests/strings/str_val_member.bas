' STR() di un MEMBRO di valore dinamico (JSON): il membro si legge come testo, ma STR vuole
' il NUMERO → " 7" (canone spazio-segno), non "7". Combinazione non coperta dai test esistenti
' (presuppone: FOR EACH/JSON + STR + spazio-segno). Regresso: C99 emetteva il testo grezzo.
' EXPECT: len= 2|eq=-1|json= 7
IMPORT JSON
SUB Main()
  DIM j AS JSON
  j = JSON.PARSE("{" + CHR$(34) + "n" + CHR$(34) + ":7}")
  PRINT "len="; LEN(STR(j.n))
  PRINT "eq="; (STR(7) = STR(j.n))
  PRINT "json="; STR(j.n)
END SUB
