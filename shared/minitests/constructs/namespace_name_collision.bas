' 2026-11-07 — un simbolo UTENTE con lo stesso nome di un namespace (case-insensitive)
' non deve rompere la forma qualificata NS.MEMBRO: `Math` (funzione) e `MATH.ABS`
' convivono. Prima: IL lanciava NotSupportedException ("accesso a indice dinamico con
' N indici"), PCode restituiva 0 in silenzio; C99 era già corretto.
' EXPECT: abs= 3||mine= 6||sqr= 4
FUNCTION Math(x AS DOUBLE) AS DOUBLE
  RETURN x * 2
END FUNCTION

SUB Main()
  PRINT "abs="; MATH.ABS(-3)
  PRINT "|mine="; Math(3)
  PRINT "|sqr="; MATH.SQR(16)
END SUB
