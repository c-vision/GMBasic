' EXPECT: a=-1 |b=-1 |c=-1 |d=-1 |e=-1 -1
' Letterali con ESPONENTE (2026-11-04): `1E30`, `1.5e-3`, `2.0E+9`. Il lexer C# non
' consumava la parte esponenziale: `E30` restava un identificatore e — a seconda del
' contesto — l'assegnazione veniva troncata in silenzio o falliva con "Token 'E30'
' inatteso" (PCode/IL/VM-C). Il frontend C (C99Native) accettava già gli esponenti.
SUB Main()
  PRINT "a="; ABS(1.0E6 - 1000000) < 0.0001;
  PRINT "|b="; ABS(123.0E2 - 12300) < 0.0001;
  ' 1E30 / 1E29 = 10 (confronto relativo: niente dipendenza dalla formattazione)
  PRINT "|c="; ABS(1E30 / 1E29 - 10) < 0.0001;
  PRINT "|d="; ABS(1.5E-3 - 0.0015) < 1E-12;
  DIM x AS DOUBLE
  x = 2.0E3 : y = 1
  PRINT "|e="; ABS(x - 2000) < 0.0001; (y = 1)
  PRINT ""
END SUB
