' ============================================================================
' GraphicsControlsDemo — controlli TUI con CHROME GRAFICO nativo (SDL/Cocoa).
' ============================================================================
' Apre una finestra grafica (SCREEN 12), tema Turbo Vision, e mostra i controlli
' con il chrome pixel nativo (rounded/stroke/highlight). Ogni controllo è MODALE:
' si interagisce col mouse/tastiera e si chiude (ESC/ENTER). I valori scelti sono
' stampati nella finestra.
'
' Controlli: STATUSBAR, CHECKBOX, TOGGLE, RADIO, SLIDER, TAB, LISTBOX,
'            COMBOBOX, MENU (flat), DATEPICKER, COLORPICKER, GRID, TREEVIEW,
'            TBROWSE, FILEDIALOG, SPINNER, PRINTERDIALOG.
'
' Esecuzione: tools/gmb_c99native_run.sh samples/Community/Clipper/GraphicsControlsDemo.bas
' (finestra SDL ~20s; oppure lancialo dal REPL).
' ============================================================================
IMPORT CONTROLS
IMPORT WINDOWS
IMPORT GRAPHICS

SUB Main()
  SCREEN 12
  WINDOW THEME "Turbo"
  WINDOW DEFINE Main FROM 1, 1 TO 22, 78 TITLE "Controlli GUI" DOUBLE SHADOW
  WINDOW Main ACTIVATE
  WINDOW Main PRINT "Controlli TUI con chrome grafico nativo"
  WINDOW Main PRINT ""

  DIM n AS LONG
  DIM s AS STRING
  DIM ps AS STRING
  DIM cp AS LONG
  DIM dx AS LONG

  STATUSBAR "Controlli GUI — ESC per chiudere ogni controllo"

  CHECKBOX 3, 2, "Abilita funzione" INTO n
  WINDOW Main PRINT "checkbox  = " + LTRIM(STR(n))

  TOGGLE 4, 2, "Modalita attiva" INTO n
  WINDOW Main PRINT "toggle    = " + LTRIM(STR(n))

  RADIO 5, 2, ["Rosso", "Verde", "Blu"] INTO s
  WINDOW Main PRINT "radio     = " + s

  SLIDER 6, 2, 30, 0, 100, 5 INTO n
  WINDOW Main PRINT "slider    = " + LTRIM(STR(n))

  TAB 7, 2, 40, 3, ["Generale", "Avanzate", "Aiuto"] INTO n
  WINDOW Main PRINT "tab       = " + LTRIM(STR(n))

  LISTBOX 8, 2, 30, 5, ["Uno", "Due", "Tre"] INTO s
  WINDOW Main PRINT "listbox   = " + s

  COMBOBOX 9, 2, 20, ["Italia", "Francia", "Germania"] INTO s
  WINDOW Main PRINT "combobox  = " + s

  MENU 10, 2, ["File", "Modifica", "Aiuto"] INTO s
  WINDOW Main PRINT "menu      = " + s

  DATEPICKER 11, 2 INTO s
  WINDOW Main PRINT "datepicker= " + s

  COLORPICKER 12, 2, 16 INTO n
  WINDOW Main PRINT "color     = " + LTRIM(STR(n))

  GRID 13, 2, 60, 5, ["Nome", "Valore"], ["a", "1", "b", "2"] INTO n
  WINDOW Main PRINT "grid      = " + LTRIM(STR(n))

  TREEVIEW 14, 2, 30, 10, ["radice", "figlio"] INTO s
  WINDOW Main PRINT "treeview  = " + s

  TBROWSE 15, 2, 70, 15, ["Nome", "Valore"], ["a", "1", "b", "2"] INTO s
  WINDOW Main PRINT "tbrowse   = " + s

  SPINNER 16, 2, 0
  WINDOW Main PRINT "spinner   ok"

  FILEDIALOG "*.bas", "Apri file" INTO s
  WINDOW Main PRINT "file      = " + s

  PRINTERDIALOG INTO ps, cp, dx
  WINDOW Main PRINT "printer   = " + ps + " copie=" + LTRIM(STR(cp)) + " duplex=" + LTRIM(STR(dx))

  WINDOW Main PRINT ""
  WINDOW Main PRINT "=== controlli GUI completati ==="

  DIM i AS INTEGER
  FOR i = 1 TO 10
    SLEEP 1000
  NEXT i
END SUB
