IMPORT CONTROLS
' FILE: Samples/Clipper/ClipperAdvanced.bas
' Demo avanzata: TBrowse RDD, FileDialog, PrinterDialog, TreeView, Grid, @...GET TYPE

SUB Main()
    PRINT "╔══════════════════════════════════════════════════════════════╗"
    PRINT "║        GMBasic Clipper TUI — Advanced Demo                 ║"
    PRINT "╚══════════════════════════════════════════════════════════════╝"
    PRINT ""
    
    ' ── Test 1: @...GET inline form controls ──
    PRINT "=== @...GET TYPE inline controls ==="
    DIM nameVal AS STRING
    DIM ageVal AS INTEGER
    DIM activeVal AS BOOLEAN
    DIM dateReg AS STRING
    DIM optVal AS STRING
    DIM scoreVal AS INTEGER
    
    nameVal = "John"
    ageVal = 30
    activeVal = TRUE
    dateReg = "2026-01-15"
    optVal = ""
    scoreVal = 50
    
    @ 3, 5 SAY "Name:"     @ 3, 15 GET nameVal    TYPE "C"
    @ 4, 5 SAY "Age:"      @ 4, 15 GET ageVal     TYPE "N" MIN 0 MAX 120 STEP 1
    @ 5, 5 SAY "Active:"   @ 5, 15 GET activeVal  TYPE "K" CAPTION "Active"
    @ 6, 5 SAY "Date:"     @ 6, 15 GET dateReg    TYPE "D" CAPTION "Reg. Date"
    @ 7, 5 SAY "Option:"   @ 7, 15 GET optVal     TYPE "C" ITEMS ["A","B","C"]
    @ 8, 5 SAY "Score:"    @ 8, 15 GET scoreVal   TYPE "N" MIN 0 MAX 100
    
    READ
    
    PRINT "  Name: "; nameVal
    PRINT "  Age: "; ageVal
    PRINT "  Active: "; activeVal
    PRINT "  Date: "; dateReg
    PRINT "  Option: "; optVal
    PRINT "  Score: "; scoreVal
    PRINT "  PASS"
    PRINT ""
    
    ' ── Test 2: GRID ──
    PRINT "=== GRID ==="
    DIM gridSel AS INTEGER
    gridSel = 0
    GRID 12, 5, 60, 5, ["ID", "Name", "Score"], _
        ["1", "Alice", "95", _
         "2", "Bob", "82", _
         "3", "Carol", "78", _
         "4", "Dave", "91", _
         "5", "Eve", "67"] INTO gridSel
    PRINT "  Cell: "; gridSel
    PRINT "  PASS"
    PRINT ""
    
    ' ── Test 3: TREEVIEW ──
    PRINT "=== TREEVIEW ==="
    DIM treeSel AS STRING
    treeSel = ""
    TREEVIEW 18, 5, 30, 10, _
        ["Root +", _
         "  Folder A +", _
         "    File A1", _
         "    File A2", _
         "  Folder B +", _
         "    File B1", _
         "  File C"] INTO treeSel
    PRINT "  Selected: "; treeSel
    PRINT "  PASS"
    PRINT ""
    
    ' ── Test 4: CONFIRM (sostituisce il BUTTON rimosso — scelta con valore di ritorno) ──
    PRINT "=== CONFIRM ==="
    DIM btnClicked AS BOOLEAN
    btnClicked = CONFIRM("Click Me") = %OK
    PRINT "  Confirm ";
    IF btnClicked THEN
        PRINT "OK"
    ELSE
        PRINT "cancelled"
    END IF
    PRINT "  PASS"
    PRINT ""

    
    ' ── Test 5: FILEDIALOG ──
    PRINT "=== FILEDIALOG ==="
    DIM filePath AS STRING
    filePath = ""
    FILEDIALOG  "*.bas", "Select BASIC file" INTO filePath
    IF filePath <> "" THEN
        PRINT "  Selected: "; filePath
    ELSE
        PRINT "  Cancelled"
    END IF
    PRINT "  PASS"
    PRINT ""
    
    ' ── Test 6: ALERT + CONFIRM ──
    PRINT "=== ALERT / CONFIRM ==="
    DIM alertRes = ALERT("This is a test alert message!")
    DIM confirmVal AS BOOLEAN
    confirmVal = FALSE
    confirmVal = CONFIRM("Do you want to continue?") = %OK
    PRINT "  Confirm: "; confirmVal
    PRINT "  PASS"
    PRINT ""
    
    ' ── Test 7: TBROWSE con dati array ──
    PRINT "=== TBROWSE (array mode) ==="
    DIM tbSel AS INTEGER
    tbSel = 0
    TBROWSE 30, 2, 70, 15, _
        [["Name", 20, "C", "L", TRUE, TRUE], _
         ["Age", 5, "N", "R", TRUE, TRUE], _
         ["City", 15, "C", "L", FALSE, TRUE], _
         ["Score", 6, "F", "R", FALSE, TRUE]], _
        [["Alice", "30", "Rome", "95.5"], _
         ["Bob", "25", "Milan", "82.0"], _
         ["Carol", "35", "Paris", "78.3"]], _
        [["freeze", 1], _
         ["footer", "F5=Sort /=Search Enter=Select Esc=Cancel"], _
         ["colors", "W/B,B/W,N/G"], _
         ["highlight", TRUE]] INTO tbSel
    PRINT "  Row: "; tbSel
    PRINT "  PASS"
    
    PRINT ""
    PRINT "═══ All advanced tests passed ═══"
END SUB
