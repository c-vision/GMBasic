' =============================================================================
' ClipperGetAdvanced.bas — Demo avanzata con VALID, RANGE, PICTURE e SPINNER
' =============================================================================

PRINT "=== CLIPPER @...GET AVANZATO ==="
PRINT

' --- Variabili ---
DIM codice AS STRING
codice = ""
DIM quantita AS INTEGER
quantita = 1
DIM prezzo AS DOUBLE
prezzo = 0.0
DIM sconto AS INTEGER
sconto = 0
DIM scadenza AS STRING
scadenza = DATE$()

' --- Form input con validazione ---
CLS
@ 1, 5 SAY "═══════════════════════════════════════"
@ 2, 5 SAY "  INSERIMENTO PRODOTTO                 "
@ 3, 5 SAY "═══════════════════════════════════════"

' Testo con PICTURE @! (uppercase forzato) e VALID
@ 5, 5 SAY "Codice (3-10 car):"
@ 5, 25 GET codice PICTURE "@!AAAAAAAAAA" COLOR "White/Blue"

' Spinner con RANGE
@ 7, 5 SAY "Quantita' (1-999):"
@ 7, 25 GET quantita TYPE "Spinner" RANGE 0, 999 STEP 1 COLOR "White/Blue"

' TextBox con RANGE e PICTURE numerica
@ 9, 5 SAY "Prezzo unitario:"
@ 9, 25 GET prezzo PICTURE "99999.99" RANGE 0, 99999 COLOR "White/Blue"

' TextBox con RANGE
@ 11, 5 SAY "Sconto % (0-100):"
@ 11, 25 GET sconto PICTURE "999" RANGE 0, 100 COLOR "White/Blue"

' DatePicker con MIN/MAX
@ 13, 5 SAY "Data scadenza:"
@ 13, 25 GET scadenza TYPE "DatePicker" MIN "01/01/2025" MAX "31/12/2030" COLOR "White/Blue"

@ 15, 5 SAY "═══════════════════════════════════════"
@ 17, 5 SAY "  ENTER=Conferma  ESC=Annulla"

READ

' --- Calcolo e output ---
PRINT
PRINT "=== RIEPILOGO PRODOTTO ==="
PRINT "Codice:      "; codice
PRINT "Quantita':   "; quantita
PRINT "Prezzo:      "; prezzo
PRINT "Sconto:      "; sconto; "%"

DIM importo AS DOUBLE
importo = quantita * prezzo
DIM scontato AS DOUBLE
scontato = importo - (importo * sconto / 100)

PRINT "Importo:     "; importo
PRINT "Scontato:    "; scontato
PRINT "Scadenza:    "; scadenza
PRINT "=========================="
