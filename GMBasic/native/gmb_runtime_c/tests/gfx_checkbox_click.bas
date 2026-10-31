' CHECKBOX: click reale -> valore INTO (verifica C99Native).
' Uso: GMB_FORM_AUTOCLICK="30,70,60" tools/gmb_c99native_run.sh <questo file>
IMPORT CONTROLS
SCREEN 12, 480, 240, 32
DIM v AS INTEGER
CHECKBOX 60, 50, 20, "Check" INTO v
DIM f AS INTEGER
FOR f = 1 TO 120 : WINDOW PUMP 5 : NEXT
SCREEN 0
PRINT "check="; v
