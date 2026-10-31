' =============================================================================
' ClipperFormDemo.bas — Demo form completo con tutte le varianti di controllo
' =============================================================================

PRINT "=== CLIPPER FORM COMPLETO ==="
PRINT

' --- Variabili per tutti i tipi di controllo ---
DIM txtNome AS STRING
txtNome = ""
DIM txtIndirizzo AS STRING
txtIndirizzo = ""
DIM dtNascita AS STRING
dtNascita = "15/06/1985"
DIM cmbTitolo AS STRING
cmbTitolo = "Sig."
DIM lstReparto AS STRING
lstReparto = "Vendite"
DIM chkNewsletter AS INTEGER
chkNewsletter = 1
DIM chkPrivacy AS INTEGER
chkPrivacy = 0
DIM spnEta AS INTEGER
spnEta = 30
DIM memoBiografia AS STRING
memoBiografia = "Inserisci qui la tua biografia..."
DIM sldSoddisfazione AS INTEGER
sldSoddisfazione = 8
DIM prgCompletamento AS INTEGER
prgCompletamento = 0

' --- Array per ComboBox e ListBox ---
DIM titoli(3) AS STRING
titoli(0) = "Sig."
titoli(1) = "Sig.ra"
titoli(2) = "Dott."
titoli(3) = "Prof."

DIM reparti(5) AS STRING
reparti(0) = "Vendite"
reparti(1) = "Marketing"
reparti(2) = "IT"
reparti(3) = "Risorse Umane"
reparti(4) = "Amministrazione"
reparti(5) = "Direzione"

' --- Form ---
CLS
@ 1, 3 SAY "╔══════════════════════════════════════════════════╗"
@ 2, 3 SAY "║         REGISTRAZIONE DIPENDENTE                  ║"
@ 3, 3 SAY "╚══════════════════════════════════════════════════╝"

' --- Riga 5-6: TextBox base ---
@ 5, 5 SAY "Nome completo:"
@ 5, 22 GET txtNome PICTURE "@!" COLOR "Yellow/Blue"

@ 6, 5 SAY "Indirizzo....:"
@ 6, 22 GET txtIndirizzo PICTURE "@!" COLOR "Yellow/Blue"

' --- Riga 8: DatePicker ---
@ 8, 5 SAY "Data nascita.:"
@ 8, 22 GET dtNascita TYPE "DatePicker" COLOR "Yellow/Blue"

' --- Riga 10: ComboBox ---
@ 10, 5 SAY "Titolo.......:"
@ 10, 22 GET cmbTitolo TYPE "ComboBox" ITEMS titoli SIZE 10 COLOR "Yellow/Blue"

' --- Riga 12: ListBox ---
@ 12, 5 SAY "Reparto......:"
@ 12, 22 GET lstReparto TYPE "ListBox" ITEMS reparti SIZE 16, 4 COLOR "Yellow/Blue"

' --- Riga 17: CheckBox (due) ---
@ 17, 5 SAY "Opzioni......:"
@ 17, 22 GET chkNewsletter TYPE "CheckBox" CAPTION " Iscrivimi alla newsletter" COLOR "Yellow/Blue"
@ 18, 22 GET chkPrivacy TYPE "CheckBox" CAPTION " Accetto la privacy policy" COLOR "Yellow/Blue"

' --- Riga 20: Spinner ---
@ 20, 5 SAY "Eta'.........:"
@ 20, 22 GET spnEta TYPE "Spinner" RANGE 18, 120 STEP 1 COLOR "Yellow/Blue"

' --- Riga 22: Memo ---
@ 22, 5 SAY "Biografia:"
@ 22, 22 GET memoBiografia TYPE "Memo" SIZE 40, 4 COLOR "Yellow/Blue"

' --- Riga 27: ProgressBar (read-only) ---
@ 27, 5 SAY "Completamento:"
@ 27, 22 SAY prgCompletamento PICTURE "999"
@ 28, 5 GET prgCompletamento TYPE "ProgressBar" SIZE 40 COLOR "Green/Black"

' --- Footer ---
@ 30, 3 SAY "══════════════════════════════════════════════════"
@ 31, 5 SAY "TAB=next  SHIFT+TAB=prev  ENTER=OK  ESC=annulla"

READ

' --- Output risultati ---
CLS
PRINT "=== DATI REGISTRATI ==="
PRINT "Nome:         "; txtNome
PRINT "Indirizzo:    "; txtIndirizzo
PRINT "Nascita:      "; dtNascita
PRINT "Titolo:       "; cmbTitolo
PRINT "Reparto:      "; lstReparto
PRINT "Newsletter:   "; chkNewsletter
PRINT "Privacy:      "; chkPrivacy
PRINT "Eta':         "; spnEta
PRINT "Biografia:    "; memoBiografia
PRINT "Soddisfaz:    "; sldSoddisfazione
PRINT "Completamento:"; prgCompletamento; "%"
PRINT "========================"
