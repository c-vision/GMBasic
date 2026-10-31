' ============================================================================
' Widget TUI senza-namespace — copertura COMPLETA (tutti i 17 comandi)
' ============================================================================
' Esercita ogni widget/controllo TUI: CHECKBOX, TOGGLE, LISTBOX, COMBOBOX, MENU,
' RADIO, SLIDER, SPINNER, TAB, GRID, TREEVIEW, TBROWSE, STATUSBAR,
' FILEDIALOG, PRINTERDIALOG, DATEPICKER, COLORPICKER.
'
' Sono MODALI: in headless si guidano con GMB_TUI_SCRIPT (ESC-injection, come il
' gate emitter C#) o con SENDKEY CHR(13); il sample verifica EMISSIONE + COMPILE +
' LINK su tutti e 5 i backend (IL/PCode/VM-C/C99Native). L'esecuzione
' interattiva richiede una sessione TUI.
'
' Arità canoniche: Parser.Stmt.Controls.cs (NON la guida).
' ============================================================================
IMPORT CONTROLS

SUB Main()
  DIM s AS STRING
  DIM sd$ AS STRING
  DIM sf$ AS STRING
  DIM ps AS STRING
  DIM sel AS STRING
  DIM n AS LONG
  DIM cp AS LONG
  DIM dx AS LONG

  PRINT "=== Widget TUI — copertura completa ==="

  ' --- 1. Checkbox / Toggle (booleani) ---
  CHECKBOX 1, 1, "Abilita" INTO n
  PRINT "checkbox="; n
  TOGGLE 1, 2, "Modalita" INTO n
  PRINT "toggle="; n

  ' --- 2. Liste di selezione (valore/etichetta stringa) ---
  LISTBOX 3, 1, 20, 5, "Uno,Due,Tre" INTO s
  PRINT "listbox="; s
  COMBOBOX 9, 1, 20, "Rosso,Verde,Blu" INTO s
  PRINT "combobox="; s
  MENU 10, 1, "File,Modifica,Help" INTO s
  PRINT "menu="; s
  RADIO 11, 1, "A,B,C" INTO s
  PRINT "radiogroup="; s

  ' --- 3. Slider / Spinner (valori numerici) ---
  SLIDER 12, 1, 30, 0, 100, 5 INTO n
  PRINT "slider="; n
  SPINNER 13, 1, "lun,mar,mer" INTO s
  PRINT "spinner="; s

  ' --- 4. Tab / Grid / TreeView / TBrowse (strutture) ---
  TAB 14, 1, 30, 5, "Generale,Avanzate" INTO n
  PRINT "tab="; n
  GRID 15, 1, 30, 5, "Nome,Valore", "a,1,b,2" INTO n
  PRINT "grid="; n
  TREEVIEW 16, 1, 30, 5, " radice, figlio1, figlio2" INTO s
  PRINT "treeview="; s

  ' --- 5. Barra di stato (nessun INTO) ---
  STATUSBAR "Pronto"

  ' --- 6. Dialoghi ---
  FILEDIALOG "*.txt", "Apri file" INTO sf$
  PRINT "filedialog="; sf$
  PRINTERDIALOG INTO ps, cp, dx
  PRINT "printer="; ps; " copie="; cp; " duplex="; dx
  DATEPICKER 17, 1 INTO sd$
  PRINT "datepicker="; sd$
  COLORPICKER 18, 1, 8 INTO n
  PRINT "colorpicker="; n

  DIM cols(1) AS STRING
  cols(0) = "Nome"
  cols(1) = "Valore"
  DIM dat(1) AS STRING
  dat(0) = "a"
  dat(1) = "1"
  TBROWSE 19, 1, 30, 5, cols, dat INTO sel
  PRINT "tbrowse="; sel

  PRINT "=== Widget completato ==="
END SUB