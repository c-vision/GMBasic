' AI.CATEGORYSET / CATEGORYFALLBACK / CATEGORYAUTO / CATEGORYPICK / CATEGORYLIST
' Orchestrazione agenti (A1): categorie di task → provider/modello (policy di routing).
' La conoscenza della categoria è separata dall'esecuzione: qui si registra solo la policy.
' Runtime condiviso: Basic.AI.Providers.CategoryRegistry (C#) / gmb_category.c (C).
IMPORT AI

SUB Main
  DIM ok AS DOUBLE

  ok = AI.CATEGORYSET("ultrabrain", "gpt-frontier")
  ok = AI.CATEGORYSET("quick", "local-small")
  ok = AI.CATEGORYSET("deep", "gpt-deep")
  ok = AI.CATEGORYFALLBACK("ultrabrain", "gpt-fallback")
  ok = AI.CATEGORYAUTO(1)

  PRINT "ultrabrain -> "; AI.CATEGORYPICK("ultrabrain")
  PRINT "quick      -> "; AI.CATEGORYPICK("quick")
  PRINT "deep       -> "; AI.CATEGORYPICK("deep")
  PRINT AI.CATEGORYLIST()
END SUB
