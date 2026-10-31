' =============================================================================
' ClipperFormFull.bas — Test completo @...GET + READ con PICTURE, VALID, RANGE
' =============================================================================

DIM nome AS STRING
nome = ""
DIM cognome AS STRING
cognome = ""
DIM anni AS INTEGER
anni = 0

@ 5, 5 SAY "Nome (uppercase):"
@ 5, 24 GET nome PICTURE "@!" COLOR "Yellow/Blue"

@ 7, 5 SAY "Cognome:"
@ 7, 24 GET cognome COLOR "Yellow/Blue"

@ 9, 5 SAY "Eta' (18-120):"
@ 9, 24 GET anni RANGE 18, 120 COLOR "Yellow/Blue"

@ 12, 5 SAY "TAB=next  ENTER=submit  ESC=cancel"
READ

' Solo se ENTER (accepted) viene raggiunto
PRINT "Nome: "; nome
PRINT "Cognome: "; cognome
PRINT "Anni: "; anni
PRINT "ALL_OK"
