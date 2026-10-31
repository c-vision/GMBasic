' =============================================================================
' ClipperGetDemo.bas — Demo completa @...GET con tutti i controlli
' =============================================================================

PRINT "=== CLIPPER @...GET + READ DEMO ==="
PRINT "Compila i campi e premi ENTER per confermare, ESC per annullare."
PRINT

' --- Dichiara variabili per i campi ---
DIM nome AS STRING
nome = ""
DIM cognome AS STRING
cognome = ""
DIM nascita AS STRING
nascita = "01/01/2000"
DIM citta AS STRING
citta = "Milano"
DIM note AS STRING
note = "Nessuna nota"
DIM attivo AS INTEGER
attivo = 1

' --- Dichiara array per ComboBox ---
DIM cittaList(4) AS STRING
cittaList(0) = "Milano"
cittaList(1) = "Roma"
cittaList(2) = "Napoli"
cittaList(3) = "Torino"
cittaList(4) = "Firenze"

DIM colori(2) AS STRING
colori(0) = "Rosso"
colori(1) = "Verde"
colori(2) = "Blu"

' --- Pulisci schermo e disegna form ---
CLS
@ 1, 5 SAY "═══════════════════════════════════════════"
@ 2, 5 SAY "  REGISTRAZIONE NUOVO UTENTE               "
@ 3, 5 SAY "═══════════════════════════════════════════"

' --- Campi di input ---
@ 5, 5 SAY "Nome........:"
@ 5, 20 GET nome PICTURE "@!" COLOR "Yellow/Blue"

@ 7, 5 SAY "Cognome.....:"
@ 7, 20 GET cognome PICTURE "@!" COLOR "Yellow/Blue"

@ 9, 5 SAY "Data nascita:"
@ 9, 20 GET nascita TYPE "DatePicker" COLOR "Yellow/Blue"

@ 11, 5 SAY "Citta'......:"
@ 11, 20 GET citta TYPE "ComboBox" ITEMS cittaList SIZE 15 COLOR "Yellow/Blue"

@ 13, 5 SAY "Colore pref.:"
@ 13, 20 GET colori(0) TYPE "ListBox" ITEMS colori SIZE 10, 3 COLOR "Yellow/Blue"

@ 15, 5 SAY "Attivo......:"
@ 15, 20 GET attivo TYPE "CheckBox" CAPTION " Utente attivo" COLOR "Yellow/Blue"

@ 17, 5 SAY "Note........:"
@ 17, 20 GET note TYPE "Memo" SIZE 30, 5 COLOR "Yellow/Blue"

@ 19, 5 SAY "═══════════════════════════════════════════"
@ 21, 5 SAY "  TAB=prossimo campo  ENTER=conferma  ESC=annulla"

' --- Loop interattivo ---
READ

' --- Mostra risultati ---
PRINT
PRINT "=== DATI INSERITI ==="
PRINT "Nome:        "; nome
PRINT "Cognome:     "; cognome
PRINT "Nascita:     "; nascita
PRINT "Citta':      "; citta
PRINT "Colore:      "; colori(0)
PRINT "Attivo:      "; attivo
PRINT "Note:        "; note
PRINT "===================="
