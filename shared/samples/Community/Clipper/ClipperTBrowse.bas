IMPORT CONTROLS
' FILE: Samples/Clipper/ClipperTBrowse.bas
' Test TBROWSE — Clipper 5.3 con sort (F5), search (/), lock, bookmark, stili

SUB Main()
    DIM selectedRow AS INTEGER
    selectedRow = 0
    
    PRINT "TBrowse demo: sort(F5), search(/), lock, bookmark, footer"
    PRINT ""
    
    TBROWSE 3, 2, 70, 15, _
        [["Name",   20, "C", "L", TRUE,  TRUE], _
         ["Age",     5, "N", "R", TRUE,  TRUE], _
         ["City",   15, "C", "L", FALSE, TRUE], _
         ["Active",  8, "L", "C", TRUE,  TRUE], _
         ["Score",   6, "F", "R", FALSE, TRUE]], _
        [["Alice",  "30", "Rome",  "Y", "95.5"], _
         ["Bob",    "25", "Milan", "N", "82.0"], _
         ["Carol",  "35", "Paris", "Y", "78.3"], _
         ["Dave",   "28", "Rome",  "Y", "91.0"], _
         ["Eve",    "42", "Milan", "N", "67.5"], _
         ["Frank",  "31", "Paris", "Y", "88.2"], _
         ["Grace",  "27", "Rome",  "N", "73.8"], _
         ["Hank",   "45", "Milan", "Y", "94.1"], _
         ["Iris",   "33", "Paris", "N", "85.7"], _
         ["Jack",   "29", "Rome",  "Y", "79.9"]], _
        [["freeze",  1], _
         ["footer",  "F5=Sort  /=Search  Enter=Select  Esc=Cancel"], _
         ["colors",  "W/B,B/W,N/G"], _
         ["highlight", TRUE], _
         ["lock", [[0, 1], [0, 2], [7, 2]]], _
         ["bookmark", 3]] INTO selectedRow
    
    IF selectedRow >= 0 THEN
        PRINT "Selected row: "; selectedRow
    ELSE
        PRINT "Cancelled"
    END IF
END SUB
