' MiniTest: una variabile locale con lo stesso nome (case-insensitive) di un builtin NON deve
' dirottare la CHIAMATA. `DIM inStr` + `INSTR(2, t, "x")`: la regola "la variabile dichiarata
' vince sul token omonimo" vale solo per il riferimento nudo (`x`) o per l'indice di un ARRAY
' (`x(i)` con x array), non per una chiamata con argomenti su una variabile SCALARE.
' REGRESSIONE (2026-11-24): con la regola applicata a ogni chiamata,
' tools/basic/settings_bundle.bas non compilava più ("simbolo non trovato: 'INSTR' è una
' funzione di libreria (namespace STRINGS)").
' EXPECT: j= 3|v= 3|k= 0
FUNCTION F(t AS STRING) AS STRING
  DIM inStr AS INTEGER
  DIM j AS LONG
  inStr = 3
  j = INSTR(2, t, "x")
  RETURN "j=" + CSTR(j)
END FUNCTION

SUB Main()
  DIM count AS LONG
  PRINT F("abxcd")
  PRINT "v="; 3
  count = 0
  PRINT "k="; count
END SUB
