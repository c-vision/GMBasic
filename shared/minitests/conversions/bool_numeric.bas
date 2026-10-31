' MiniTest convenzione numerica BIT/BOOL: TRUE = -1, FALSE = 0 (4 backend).
' Solo confronti (niente PRINT numerico: formattazione del segno diverge fra gambe).
' EXPECT: conv-t|conv-f|val-t|abs-t|absv-t|arith-t|arith1-t|sub-t|div-t|mod-t|ccur-t|ccur-f|ccur-n|cmp-lt|cmp-ge|cmp-long|if-t|if-f
SUB Main()
  DIM n AS INTEGER
  n = TRUE
  IF n = -1 THEN PRINT "conv-t"
  n = FALSE
  IF n = 0 THEN PRINT "conv-f"
  IF VAL(TRUE) = -1 THEN PRINT "val-t"
  IF ABS(TRUE) = 1 THEN PRINT "abs-t"
  DIM b AS BIT
  b = TRUE
  IF ABS(b) = 1 THEN PRINT "absv-t"
  IF TRUE + 0 = -1 THEN PRINT "arith-t"
  ' L'ADDIZIONE esplicita (non identità): su PCode OP_ADD aveva una copia inline della
  ' coercizione che perdeva il bool (TryParse("True")→0) → TRUE + 1 dava 1 invece di 0,
  ' mentre TRUE * 3 dava -3 (via PopNum). Vedi
  ' Bugs/(solved) - JSON-Stringify-Scalare-Diverge-Tra-Gambe.md, difetto 3.
  IF TRUE + 1 = 0 THEN PRINT "arith1-t"
  ' Resto dell'asse aritmetico: sottrazione, divisione e MOD devono usare la STESSA
  ' coercizione (NumOf) — TRUE = -1, non 1 e non 0.
  IF TRUE - 1 = -2 THEN PRINT "sub-t"
  IF TRUE / 2 = -0.5 THEN PRINT "div-t"
  IF TRUE MOD 3 = -1 THEN PRINT "mod-t"
  ' CCUR = CURRENCY (decimal): stesso canone numerico, TRUE = -1 e non 1.
  ' Sul backend IL non esisteva la conversione bool→Decimal e CCUR(TRUE) non compilava
  ' (NotSupportedException "nessun opcode di conversione per 'Decimal'"); su PCode
  ' Convert.ToDecimal(bool) dava 1.
  IF CCUR(TRUE) = -1 THEN PRINT "ccur-t"
  IF CCUR(FALSE) = 0 THEN PRINT "ccur-f"
  IF CCUR(5) = 5 THEN PRINT "ccur-n"
  ' Confronti con operandi MISTI: bool contro un numero (e viceversa) passano dalla
  ' coercizione numerica, non da un confronto per identità.
  IF TRUE < 0 THEN PRINT "cmp-lt"
  IF 0 >= FALSE THEN PRINT "cmp-ge"
  DIM lv AS LONG
  lv = -1
  IF lv = TRUE THEN PRINT "cmp-long"
  IF TRUE THEN PRINT "if-t"
  IF NOT FALSE THEN PRINT "if-f"
END SUB
