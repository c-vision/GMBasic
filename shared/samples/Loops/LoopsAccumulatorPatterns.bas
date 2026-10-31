' LoopsAccumulatorPatterns.bas — pattern di ACCUMULAZIONE nei cicli FOR, verificati su
' tutti e 5 i backend (IL, PCodeVM, VM/C, C99Native).
'
' Copre le forme che il compilatore PCode/VM-C puo' FONDERE in un solo opcode nativo
' (OP_NATIVE_LOOP_ACC per i numeri, OP_NATIVE_STRING_APPEND_LOOP per le stringhe) e i
' casi in cui la fusione NON deve avvenire: l'assegnazione deve essere l'UNICO statement
' del corpo, perche' l'opcode fused gira l'intero range e consuma lo stato del FOR.
' Ogni riga stampata deve dire OK: un KO segnala una divergenza tra backend.
SUB Main()
    DIM i AS INTEGER
    DIM acc AS DOUBLE

    PRINT "== 1. forme numeriche fuse (corpo = solo l'accumulatore) =="
    acc = 0
    FOR i = 1 TO 10
        acc = acc + i                    ' acc + I
    NEXT
    PRINT "  somma I          "; CHK(acc, 55)

    acc = 0
    FOR i = 1 TO 4
        acc = acc + i * 3                ' acc + I*m
    NEXT
    PRINT "  somma I*3        "; CHK(acc, 30)

    acc = 0
    FOR i = 1 TO 5
        acc = acc + i * i                ' acc + I*I
    NEXT
    PRINT "  somma I*I        "; CHK(acc, 55)

    acc = 0
    FOR i = 1 TO 4
        acc = acc + ROUND(i)             ' acc + f(I)
    NEXT
    PRINT "  somma ROUND(I)   "; CHK(acc, 10)

    acc = 0
    FOR i = 1 TO 4
        acc = acc + ROUND(i) * 2         ' acc + f(I)*m
    NEXT
    PRINT "  somma ROUND(I)*2 "; CHK(acc, 20)

    acc = 0
    FOR i = 1 TO 4
        acc = acc + ABS(i) + ROUND(i)    ' acc + f(I)+g(I)
    NEXT
    PRINT "  somma ABS+ROUND  "; CHK(acc, 20)

    PRINT "== 2. operatori di accumulazione =="
    acc = 0
    FOR i = 1 TO 4
        acc = acc - i
    NEXT
    PRINT "  sottrazione      "; CHK(acc, -10)

    acc = 1
    FOR i = 1 TO 5
        acc = acc * i
    NEXT
    PRINT "  prodotto (5!)    "; CHK(acc, 120)

    acc = 720
    FOR i = 1 TO 4
        acc = acc / i
    NEXT
    PRINT "  quoziente in cascata "; CHK(acc, 30)

    PRINT "== 3. range del ciclo =="
    acc = 0
    FOR i = 10 TO 1 STEP -1
        acc = acc + i
    NEXT
    PRINT "  STEP negativo    "; CHK(acc, 55)

    acc = 7
    FOR i = 5 TO 1                       ' bound invertiti: zero iterazioni
        acc = acc + 1
    NEXT
    PRINT "  zero iterazioni  "; CHK(acc, 7)

    DIM n AS INTEGER
    n = 4
    acc = 0
    FOR i = 1 TO n                       ' bound valutato una volta sola
        acc = acc + i
    NEXT
    PRINT "  bound variabile  "; CHK(acc, 10)

    PRINT "== 4. casi NON fusi (corpo con piu' statement) =="
    DIM cnt AS INTEGER
    cnt = 0
    acc = 0
    FOR i = 1 TO 4
        cnt = cnt + 1                    ' corpo misto: PRINT + 2 assegnazioni
        acc = acc + i
        PRINT "   iter"; i; "cnt="; cnt
    NEXT
    PRINT "  corpo misto acc  "; CHK(acc, 10)
    PRINT "  corpo misto cnt  "; CHK(cnt, 4)

    acc = 0
    FOR i = 1 TO 3
        acc = acc + i + i                ' chain a 3 operandi: forma non ammessa
    NEXT
    PRINT "  chain acc+I+I    "; CHK(acc, 12)

    DIM bump AS INTEGER
    bump = 0
    FOR i = 1 TO 5
        bump = bump + 1                  ' contatore modificato nel corpo: i=1->3, NEXT
        i = i + 2                        ' porta a 4; secondo giro i=6, NEXT 7 > 5 -> stop
    NEXT
    PRINT "  contatore modif. "; CHK(bump, 2)

    PRINT "== 5. accumulazione di stringhe =="
    DIM s AS STRING
    s = ""
    FOR i = 1 TO 4
        s = s + "ab"                     ' append di letterale (forma fuse)
    NEXT
    PRINT "  append lit       "; CHKSTR(s, "abababab")
    PRINT "  lunghezza        "; CHK(LEN(s), 8)

    s = ""
    FOR i = 1 TO 3
        s = s + STR$(i)                  ' append di STR$(contatore) (forma fuse)
    NEXT
    PRINT "  append STR$      "; CHKSTR(s, " 1 2 3")

    s = ""
    DIM t AS STRING
    t = "-"
    FOR i = 1 TO 3
        s = s + t                        ' termine variabile: non fuse
    NEXT
    PRINT "  append variabile "; CHKSTR(s, "---")

    s = ""
    FOR i = 1 TO 3
        s = ">" + s                      ' prefisso: non fuse
    NEXT
    PRINT "  prepend          "; CHKSTR(s, ">>>")

    s = ""
    FOR i = 1 TO 2
        s = s + "a" + "b"                ' chain: due append, non fuse
    NEXT
    PRINT "  chain due append "; CHKSTR(s, "abab")


    PRINT "== 6. cicli annidati =="
    DIM rows AS STRING
    DIM r AS INTEGER, c AS INTEGER, cell AS STRING
    rows = ""
    FOR r = 1 TO 3
        cell = ""
        FOR c = 1 TO 3
            cell = cell + "#"            ' interno fuse
        NEXT
        rows = rows + cell + ";"         ' esterno: due append, non fuse
    NEXT
    PRINT "  griglia 3x3      "; CHKSTR(rows, "###;###;###;")

    PRINT "== 7. dentro FUNCTION (scenario del bug) =="
    PRINT "  somma con IF     "; CHK(LoopSum(4), 10)
    PRINT "  log con append   "; CHKSTR(BuildLog("L"), "L 1L 2L 3")
    PRINT "== 8. EXIT FOR e CONTINUE FOR con accumulatore =="
    acc = 0
    FOR i = 1 TO 10
        IF i > 6 THEN
            EXIT FOR
        END IF
        acc = acc + i
    NEXT
    PRINT "  exit for a 6   "; CHK(acc, 21)

    acc = 0
    FOR i = 1 TO 6
        IF i / 2 = INT(i / 2) THEN
            CONTINUE FOR
        END IF
        acc = acc + i
    NEXT
    PRINT "  solo dispari   "; CHK(acc, 9)


    PRINT "== fine =="
END SUB

' Confronto a tolleranza: i double possono stamparsi con cifre diverse tra backend,
' quindi il valore si confronta qui e a video finisce solo OK/KO.
FUNCTION CHK(v AS DOUBLE, w AS DOUBLE) AS STRING
    IF ABS(v - w) < 0.000001 THEN RETURN "OK"
    RETURN "KO=" + LTRIM$(STR$(v)) + " atteso " + LTRIM$(STR$(w))
END FUNCTION

FUNCTION CHKSTR(v AS STRING, w AS STRING) AS STRING
    IF v = w THEN RETURN "OK"
    RETURN "KO=[" + v + "] atteso [" + w + "]"
END FUNCTION

' Corpo misto (IF + accumulatore): la fusione non si applica, il corpo deve girare
' tutte le n volte. Se l'opcode fused consumasse il ciclo il risultato sarebbe 1.
FUNCTION LoopSum(n AS INTEGER) AS DOUBLE
    DIM s AS DOUBLE
    DIM k AS INTEGER
    s = 0
    FOR k = 1 TO n
        IF k >= 1 THEN
            s = s + k
        END IF
    NEXT k
    LoopSum = s
END FUNCTION

' Append di stringa con corpo misto (PRINT + append a 3 operandi).
FUNCTION BuildLog(prefix AS STRING) AS STRING
    DIM s AS STRING
    DIM k AS INTEGER
    s = ""
    FOR k = 1 TO 3
        PRINT "   log"; k
        s = s + prefix + STR$(k)
    NEXT k
    BuildLog = s
END FUNCTION

