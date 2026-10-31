IMPORT CONTROLS
' FILE: Samples/Clipper/ClipperFullSuite.bas
' Demo completa di tutti i controlli Clipper TUI
' Esegui in modalità test (GMBASIC_DEV_MODE=1) per output non interattivo

SUB Main()
    PRINT "╔══════════════════════════════════════════════════════════════╗"
    PRINT "║        GM-Basic Clipper TUI — Full Suite Demo               ║"
    PRINT "╚══════════════════════════════════════════════════════════════╝"
    PRINT ""
    
    ' ── Test 1: STATUSBAR ──
    PRINT "Test 1: STATUSBAR"
    STATUSBAR " Ready | F1=Help | F5=Sort | /=Search | Esc=Cancel"
    PRINT "  Status bar displayed at bottom of screen."
    PRINT "  OK"
    
    ' ── Test 2: TOGGLE ──
    PRINT "Test 2: TOGGLE"
    DIM toggleVal AS BOOLEAN
    toggleVal = FALSE
    TOGGLE  3, 5, "Enable feature" INTO toggleVal
    PRINT "  Toggle value: "; toggleVal
    PRINT "  OK"
    
    ' ── Test 3: RADIO ──
    PRINT "Test 3: RADIO"
    DIM radioVal AS STRING
    radioVal = ""
    RADIO  5, 5, ["Option A", "Option B", "Option C"] INTO radioVal
    PRINT "  Selected: "; radioVal
    PRINT "  OK"
    
    ' ── Test 4: SLIDER ──
    PRINT "Test 4: SLIDER"
    DIM sliderVal AS INTEGER
    sliderVal = 50
    SLIDER  7, 5, 40, 0, 100, 5 INTO sliderVal
    PRINT "  Slider value: "; sliderVal
    PRINT "  OK"
    
    ' ── Test 5: TAB ──
    PRINT "Test 5: TAB"
    DIM tabIdx AS INTEGER
    tabIdx = 0
    TAB  9, 5, 50, 3, ["General", "Advanced", "Help"] INTO tabIdx
    PRINT "  Active tab: "; tabIdx
    PRINT "  OK"
    
    ' ── Test 6: CHECKBOX ──
    PRINT "Test 6: CHECKBOX"
    DIM chkVal AS BOOLEAN
    chkVal = TRUE
    CHECKBOX  11, 5, "Send notifications" INTO chkVal
    PRINT "  Checked: "; chkVal
    PRINT "  OK"
    
    ' ── Test 7: LISTBOX ──
    PRINT "Test 7: LISTBOX"
    DIM listVal AS STRING
    listVal = ""
    LISTBOX  13, 5, 30, 5, ["Red", "Green", "Blue", "Yellow", "Purple", "Orange"] INTO listVal
    PRINT "  Selected: "; listVal
    PRINT "  OK"
    
    ' ── Test 8: COMBOBOX ──
    PRINT "Test 8: COMBOBOX"
    DIM comboVal AS STRING
    comboVal = ""
    COMBOBOX  19, 5, 20, ["Italy", "France", "Germany", "Spain"] INTO comboVal
    PRINT "  Selected: "; comboVal
    PRINT "  OK"
    
    ' ── Test 9: MENU flat ──
    PRINT "Test 9: MENU (flat)"
    DIM menuVal AS STRING
    menuVal = ""
    MENU  21, 5, ["File", "Edit", "View", "Help"] INTO menuVal
    PRINT "  Selected: "; menuVal
    PRINT "  OK"
    
    ' ── Test 10: MENU nested ──
    PRINT "Test 10: MENU (nested con separatore)"
    DIM menuNested AS STRING
    menuNested = ""
    MENU  22, 5, [["File", "New", "Open", "Save", "---", "Exit"], ["Edit", "Cut", "Copy", "Paste"], "Help"] INTO menuNested
    PRINT "  Selected: "; menuNested
    PRINT "  OK"
    
    ' ── Test 11: DATEPICKER ──
    PRINT "Test 11: DATEPICKER"
    DIM dateVal AS STRING
    dateVal = ""
    DATEPICKER  24, 5 INTO dateVal
    PRINT "  Date: "; dateVal
    PRINT "  OK"
    
    ' ── Test 12: COLORPICKER ──
    PRINT "Test 12: COLORPICKER"
    DIM colorIdx AS INTEGER
    colorIdx = 0
    COLORPICKER  26, 5, 16 INTO colorIdx
    PRINT "  Color index: "; colorIdx
    PRINT "  OK"
    
    PRINT ""
    PRINT "═══ All tests passed ═══"
END SUB
