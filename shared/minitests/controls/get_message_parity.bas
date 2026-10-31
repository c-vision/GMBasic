' TAG: interactive
' @GET MESSAGE/ERROR (Clipper/xBase): clausole accettate su tutte le gambe; il valore
' restituito e' identico. Il testo di stato e' reso in TUI (riga inferiore, solo visivo).
' EXPECT: v=ab
DYNAMIC v AS STRING = ""
@ 2, 10 GET v MESSAGE "Inserisci codice" ERROR "Troppo corto" VALID Len(v) >= 2
READ
PRINT "v="; v
