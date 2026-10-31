' AI.HASHLINE / HASHEDIT / HASHVERIFY — edit ancorato al contenuto (A10).
' Ogni riga è taggata `N#HASH| contenuto`; l'edit riferisce il tag ed è rifiutato se il
' contenuto è cambiato. Runtime condiviso: Basic.Agents.HashlineTool / gmb_hashline.c.
IMPORT AI

SUB Main
  DIM s AS STRING
  s = "function hello() {" + CHR(10) + "  return 1" + CHR(10) + "}"
  PRINT AI.HASHLINE(s)
  PRINT "verifica riga 2: "; AI.HASHVERIFY(s, "2")
  PRINT "edit riga 2:"
  PRINT AI.HASHEDIT(s, "2", "  return 42")
END SUB
