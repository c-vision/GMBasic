' EXPECT: i 5|l-7|xy|m 5.5|neg-3|z 0|a             b|s1 1  2  3|cmp-1|big 1000000|big15 123456789012345|big16 1.23456789012346E+15|exp 1.234E-06|exp2 1.23456789012346E+18|prec 3.14159265358979|half 0.5|nz 0|snum5|sqa 5 b|5  6  7|mix 5          6|tailnext
' MiniTest PRINT.SPAZIO — semantica di spaziatura del PRINT (parità 4 backend).
' Regole canoniche (oracolo IL/PCode/VM-C, replicate da C99Native):
'  - numero (double) → spazio iniziale se >=0 + G15 + spazio finale
'  - stringa → niente padding
'  - ';' tra argomenti → nessuno spazio aggiuntivo (solo il padding del valore)
'  - ',' → zona di 14 colonne (padding di SPAZI fino alla colonna successiva, NON un TAB)
'  - interi grandi (>=1e15) → G15 scientifico (1.23456789012346E+15)
'  - PRINT nudo → riga vuota; PRINT che termina con ';' → niente newline
' Oracolo: backfill IL (GMB_MINI_WRITE_EXPECT=1).
SUB Main()
    PRINT "i"; 5
    PRINT "l"; -7
    PRINT "x"; "y"
    PRINT "m"; 5.5
    PRINT "neg"; -3
    PRINT "z"; 0
    PRINT "a", "b"
    PRINT "s1"; 1; 2; 3
    PRINT "cmp"; (1 < 2)
    PRINT "big"; 1000000
    PRINT "big15"; 123456789012345
    PRINT "big16"; 1234567890123456
    PRINT "exp"; 0.000001234
    PRINT "exp2"; 1234567890123456789
    PRINT "prec"; 3.141592653589793
    PRINT "half"; 0.5
    PRINT "nz"; -0.0
    PRINT "snum"; "5"
    PRINT "sq"; "a"; 5; "b"
    PRINT 5; 6; 7
    PRINT "mix"; 5, 6
    PRINT "tail";
    PRINT "next"
END SUB
