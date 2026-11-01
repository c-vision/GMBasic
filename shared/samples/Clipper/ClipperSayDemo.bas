' =============================================================================
' ClipperSayDemo.bas — Demo completa @...SAY con tutte le opzioni
' =============================================================================

PRINT "=== CLIPPER @...SAY DEMO ==="

' --- SAY base ---
@ 2, 5 SAY "Benvenuto in GMBasic!"
PRINT

' --- SAY con PICTURE (formattazione numerica) ---
DIM prezzo AS DOUBLE
prezzo = 1234.56
@ 4, 5 SAY "Prezzo base:"
@ 4, 20 SAY prezzo
@ 5, 5 SAY "Prezzo formato:"
@ 5, 20 SAY prezzo PICTURE "999,999.99"
PRINT

' --- SAY con espressioni ---
DIM nome AS STRING
nome = "Mario Rossi"
DIM eta AS INTEGER
eta = 42
@ 7, 5 SAY "Nome: " + nome
@ 8, 5 SAY "Eta': "
@ 8, 12 SAY eta
PRINT

' --- SAY multipli per form ---
@ 10, 5 SAY "══════════════════════════════════"
@ 11, 5 SAY "  SCHEDA ANAGRAFICA CLIENTE      "
@ 12, 5 SAY "══════════════════════════════════"
@ 14, 5 SAY "Nome.......:"
@ 15, 5 SAY "Indirizzo..:"
@ 16, 5 SAY "Citta'.....:"
@ 17, 5 SAY "Telefono...:"
@ 18, 5 SAY "══════════════════════════════════"
PRINT

' --- SAY con funzioni ---
DIM oggi AS STRING
oggi = DATE$()
@ 20, 5 SAY "Data: " + oggi
@ 21, 5 SAY "Ora: " + TIME$()
PRINT

PRINT "Demo @...SAY completata."
