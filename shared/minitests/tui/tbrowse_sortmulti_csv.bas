' Minitest TBROWSE — ordinamento MULTI-COLONNA (sortkeys) + export CSV + TIPO colonna (§12.4), 4 backend.
' EXPECT: tbsortmulti-ok
IMPORT CONTROLS
SCREEN 0

SUB Main()
  DIM sel AS INTEGER
  ' colonna V dichiarata NUMERICA ("N"): 10 e 2 devono ordinarsi 2 < 10 (non testuale "10" < "2").
  TBROWSE 2, 2, 30, 6, {{"G", 10}, {"V", 10, "N"}}, {{"b", "10"}, {"a", "2"}, {"a", "10"}, {"b", "2"}} INTO sel NAME "tb2"

  ' ordina per colonna 0 (G) asc, poi 1 (V, numerica) asc
  CONTROL SET "tb2", "sortkeys", 0, "0:asc,1:asc"
  DIM sk AS STRING
  CONTROL GETS "tb2", "sortkeys" INTO sk

  ' export CSV della vista e conversione in una riga (newline -> ';')
  DIM csv AS STRING
  CONTROL GETS "tb2", "csv" INTO csv
  DIM one AS STRING
  one = ""
  DIM i AS INTEGER
  FOR i = 1 TO LEN(csv)
    DIM ch AS STRING
    ch = MID(csv, i, 1)
    IF ch = CHR(10) THEN
      one = one + ";"
    ELSE
      one = one + ch
    END IF
  NEXT i

  IF sk = "0:asc,1:asc" AND one = "G,V;a,2;a,10;b,2;b,10;" THEN
    PRINT "tbsortmulti-ok"
  ELSE
    PRINT "tbsortmulti-no " + sk + " " + one
  END IF
END SUB
