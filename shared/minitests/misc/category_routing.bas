' MiniTest: orchestrazione agenti (A1) — categorie di task → provider/modello.
' Solo operazioni DETERMINISTICHE (SET/FALLBACK/AUTO/PICK/LIST). Runtime condiviso
' Basic.AI.Providers.CategoryRegistry (C#) / gmb_category.c (C): stessa semantica.
' EXPECT: set= 1  1  1|auto= 1|pick=gpt-frontier|list={"auto":true,"categories":{"quick":{"provider":"local-small","fallback":""},"ultrabrain":{"provider":"gpt-frontier","fallback":"gpt-fallback"}}}
IMPORT AI

SUB Main
  DIM r1 AS DOUBLE
  DIM r2 AS DOUBLE
  DIM r3 AS DOUBLE
  DIM p AS STRING
  DIM l AS STRING

  r1 = AI.CATEGORYSET("ultrabrain", "gpt-frontier")
  r2 = AI.CATEGORYSET("quick", "local-small")
  r3 = AI.CATEGORYFALLBACK("ultrabrain", "gpt-fallback")
  PRINT "set="; r1; r2; r3

  r1 = AI.CATEGORYAUTO(1)
  PRINT "auto="; r1

  p = AI.CATEGORYPICK("ultrabrain")
  PRINT "pick="; p

  l = AI.CATEGORYLIST()
  PRINT "list="; l
END SUB
