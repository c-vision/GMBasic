IMPORT CONTROLS
' FILE: Console/ConsoleUiControls.bas
' =================================================================================================
' TEST UNITARIO: CONSOLE - CONTROLLI UI STANDALONE (LEGACY GLOBAL)
' =================================================================================================
' Questo test copre i controlli UI standalone stile Clipper: CHECKBOX, LISTBOX, COMBOBOX,
' BUTTON, MENU, STATUSBAR, TOGGLE, RADIO, SLIDER, GRID, DATEPICKER, COLORPICKER,
' TREEVIEW, SPINNER, TBROWSE.
'
' IMPORTANTE: questi controlli sono INTERATTIVI — in headless bloccano in attesa di input
' (ogni controllo mostra il widget e resta in attesa). Il test COMPILA ma va escluso dal
' runner test_pcode (skip) se non viene iniettata la tastiera. I comandi CONSOLE non-bloccanti
' stanno in Console/ConsoleControls.bas (quello eseguibile in headless).
'
' Sintassi standalone (legacy global, come in Clipper):
'   CHECKBOX var, row, col, caption
'   LISTBOX  var, row, col, width, height, items
'   COMBOBOX var, row, col, width, items
'   BUTTON   row, col, caption CALL sub
'   MENU     var, row, col, items
'   STATUSBAR text
'   TOGGLE   var, row, col, caption
'   RADIO var, row, col, items
'   SLIDER   var, row, col, length, min, max, step
'   GRID     var, row, col, width, height, headers, data
'   DATEPICKER var, row, col
'   COLORPICKER var, row, col, columns
'   TREEVIEW var, row, col, width, height, tree
'   SPINNER  row, col, preset
'   TBROWSE  var, row, col, width, height, cols, data
' =================================================================================================

SUB SetClicked(v)
    PRINT "BUTTON click: "; v
END SUB

SUB Main()
    CLS
    PRINT "============================================================"
    PRINT "CONSOLE - Controlli UI standalone (interattivi)"
    PRINT "============================================================"
    PRINT

    ' Dati condivisi
    DIM colori(2) AS STRING
    colori(0) = "Rosso" : colori(1) = "Verde" : colori(2) = "Blu"

    DIM voci(2) AS STRING
    voci(0) = "File" : voci(1) = "Edit" : voci(2) = "View"

    DIM radioItems(2) AS STRING
    radioItems(0) = "A" : radioItems(1) = "B" : radioItems(2) = "C"

    ' --- CHECKBOX ---
    DIM ok AS STRING
    CHECKBOX  3, 3, "Accetto i termini" INTO ok

    ' --- LISTBOX ---
    DIM colore AS STRING
    LISTBOX  6, 3, 15, 5, colori INTO colore

    ' --- COMBOBOX ---
    DIM combo AS STRING
    COMBOBOX  8, 3, 15, colori INTO combo

    ' --- MENU ---
    DIM menuSel AS STRING
    MENU  12, 3, voci INTO menuSel

    ' --- STATUSBAR ---
    STATUSBAR "Pronto"

    ' --- TOGGLE ---
    DIM tog AS STRING
    TOGGLE  16, 3, "Opzione" INTO tog

    ' --- RADIO ---
    DIM radio AS STRING
    RADIO  18, 3, radioItems INTO radio

    ' --- SLIDER (var, row, col, length, min, max, step) ---
    DIM sl AS DOUBLE
    SLIDER  20, 3, 20, 0, 100, 5 INTO sl

    ' --- GRID (headers piatti + dati piatti) ---
    DIM gridHeaders(1) AS STRING
    gridHeaders(0) = "ID" : gridHeaders(1) = "Name"
    DIM gridData(3) AS STRING
    gridData(0) = "1" : gridData(1) = "Uno" : gridData(2) = "2" : gridData(3) = "Due"
    DIM gridSel AS STRING
    GRID  22, 3, 60, 5, gridHeaders, gridData INTO gridSel

    ' --- DATEPICKER ---
    DIM dataSel AS STRING
    DATEPICKER  26, 3 INTO dataSel

    ' --- COLORPICKER ---
    DIM colPick AS STRING
    COLORPICKER  28, 3, 3 INTO colPick

    ' --- TREEVIEW ---
    DIM treeItems(1) AS STRING
    treeItems(0) = "Root +" : treeItems(1) = "  Child"
    DIM treeSel AS STRING
    TREEVIEW  30, 3, 30, 10, treeItems INTO treeSel

    ' --- SPINNER (row, col, preset) ---
    SPINNER 32, 3, "dots"

    ' --- TBROWSE (cols piatti + data piatta) ---
    DIM tbCols(1) AS STRING
    tbCols(0) = "Name" : tbCols(1) = "Age"
    DIM tbData(3) AS STRING
    tbData(0) = "Alice" : tbData(1) = "30" : tbData(2) = "Bob" : tbData(3) = "25"
    DIM tbSel AS STRING
    TBROWSE  34, 2, 70, 15, tbCols, tbData INTO tbSel

    PRINT "Controlli UI creati."
    PRINT "Test Completato."
END SUB
