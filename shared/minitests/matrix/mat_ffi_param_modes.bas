' EXPECT: ok
' ADR-MATRIX §3: IN/OUT/INOUT sono accettati anche nei DECLARE NATIVE (parità
' parser C#/C). I conteggi restano espliciti; qui il simbolo non viene chiamato,
' quindi il test verifica la sola parità di parsing/registrazione della firma.
DECLARE NATIVE SUB ZFILL LIB "gmbstats" ALIAS "ofx_ts_covmatrix" (OUT m AS MATRIX OF (INT, INT, DOUBLE), n AS LONG, k AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB ZSWAP LIB "gmbstats" ALIAS "ofx_ts_covmatrix" (INOUT m AS MATRIX OF (INT, INT, DOUBLE), n AS LONG, k AS LONG, out() AS DOUBLE)

SUB Main()
  PRINT "ok"
END SUB
