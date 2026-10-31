'==============================================================================
' Sample — Operatori bitwise/logici e comando SWAP
' Dimostra su TUTTI i backend (IL, PCodeVM, VM/C, C99Native):
'   - XOR bitwise (cast a intero)
'   - AND / OR logici SHORT-CIRCUIT (risultato: AND -> 0 se sinistra falsa
'     altrimenti valore destro; OR -> -1 se sinistra vera altrimenti valore destro)
'   - precedenza: OR < XOR < AND
'   - SWAP di variabili numeriche e stringa
' Semantica condivisa e verificata con i MiniTest:
'   minitests/constructs/expr_operators_xor.bas, expr_operators_logical.bas,
'   statements_swap.bas
'==============================================================================

DIM ContatoreChiamate AS LONG

' Effetto collaterale: serve a provare che AND/OR NON lo invocano quando il
' lato sinistro decide gia' il risultato (short-circuit).
FUNCTION Costoso() AS DOUBLE
    ContatoreChiamate = ContatoreChiamate + 1
    Costoso = 1
END FUNCTION

' Media bit-packing semplice con XOR (checksum) su un array.
FUNCTION ChecksumXor(Valori() AS DOUBLE, N AS LONG) AS DOUBLE
    DIM i AS LONG
    DIM acc AS LONG
    acc = 0
    FOR i = 0 TO N - 1
        acc = acc XOR CLNG(Valori(i))
    NEXT
    ChecksumXor = acc
END FUNCTION

SUB Main()
    ' --- XOR bitwise ---
    PRINT "xor="; 6 XOR 3            ' 5
    PRINT "xor_neg="; 5 XOR -1       ' -6

    ' --- Precedenza OR < XOR < AND ---
    PRINT "prec1="; 6 AND 3 XOR 1    ' AND prima: (6 AND 3)=3 -> 3 XOR 1 = 2
    PRINT "prec2="; 8 XOR 1 AND 3    ' AND prima: (1 AND 3)=3 -> 8 XOR 3 = 11
    PRINT "prec3="; 1 OR 2 XOR 3     ' XOR prima: (2 XOR 3)=1 -> 1 OR 1 = -1

    ' --- SWAP numerico ---
    DIM a AS LONG
    DIM b AS LONG
    a = 10 : b = 27
    SWAP a, b
    PRINT "swap_num="; a; ","; b     ' 27,10

    ' --- SWAP stringa ---
    DIM s1 AS STRING
    DIM s2 AS STRING
    s1 = "primo" : s2 = "secondo"
    SWAP s1, s2
    PRINT "swap_str="; s1; ","; s2   ' secondo,primo

    ' --- AND/OR short-circuit: il ramo destro NON viene valutato ---
    ContatoreChiamate = 0
    DIM r1 AS LONG
    r1 = 0 AND Costoso()             ' sinistra falsa -> 0, Costoso() non chiamata
    DIM r2 AS LONG
    r2 = 1 OR Costoso()              ' sinistra vera  -> -1, Costoso() non chiamata
    PRINT "sc="; r1; ","; r2; ",chiamate="; ContatoreChiamate   ' 0,-1,chiamate=0

    ' --- Short-circuit che protegge una divisione ---
    DIM den AS LONG
    den = 0
    IF den <> 0 AND (100 / den) > 1 THEN
        PRINT "div=non-zero"
    ELSE
        PRINT "div=protetta"         ' nessun crash: il ramo destro non valutato
    END IF

    ' --- Uso reale: checksum XOR su array ---
    DIM v(4) AS DOUBLE
    v(0) = 1 : v(1) = 2 : v(2) = 4 : v(3) = 8
    PRINT "checksum="; ChecksumXor(v, 4)   ' 1^2^4^8 = 15
END SUB
