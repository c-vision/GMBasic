' MiniTest: CONVERT.MDIMAGES(doc) — AST da fixture locale fix_images.md.
' NOTA: nessun parser (C# MdSupport / C gmb_md.c) emette blocchi "image" (le immagini
' inline restano testo+link nei paragrafi), quindi MDIMAGES ritorna lista vuota su
' tutti i backend. Oracolo intenzionale: documenta il comportamento attuale.
' (Gap: common/IlBackendRuntime/MdSupport.cs MdImages righe 289-298, MdParse 15-115;
'  native/gmb_runtime_c/src/gmb_md.c riga 378.)
' EXPECT: 0
IMPORT CONVERT
IMPORT IO

SUB Main()
  DIM L = IO.READLINES("fix_images.md")
  DIM md$ = ""
  FOR i = 0 TO UBOUND(L)
    IF i > 0 THEN md$ = md$ + CHR(10)
    md$ = md$ + L(i)
  NEXT
  DIM d = CONVERT.PARSE(md$)
  DIM im = CONVERT.MDIMAGES(d)
  DIM n AS LONG = 0
  FOR EACH x IN im
    n = n + 1
  NEXT
  PRINT n
END SUB
