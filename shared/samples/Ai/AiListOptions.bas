' AI LIST OPTIONS — comando-query del registro opzioni generico.
' Mostra in una schermata lo stato delle voci DICHIARATE (quelle con descrizione).
' Forma comando (senza parentesi):  AI LIST OPTIONS
' Forma funzione (lettura, parentesi): v$ = AI.Option("<nome>")
IMPORT AI WITH ALIAS

SUB Main()
  PRINT "== registro opzioni =="
  AI LIST OPTIONS
  PRINT ""
  PRINT "moe_swap_dir = "; AI.Option("moe_swap_dir")
END SUB
