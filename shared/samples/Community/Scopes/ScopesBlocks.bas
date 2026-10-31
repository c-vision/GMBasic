' FILE: Scopes/ScopesBlocks.bas
' Test: Block scope isolation per specifica VARIABLES #3
' - DIM dentro IF → variabile muore su END IF
' - DIM dentro FOR → variabile muore su NEXT (contatore locale al ciclo)
' - DIM dentro TRY/CATCH → variabile muore alla fine del blocco
' - LET dentro blocco → crea/shadows nello scope corrente

IMPORT SYSTEM

SUB TestIfBlock()
    DIM outer = 10
    PRINT "Prima IF: outer ="; outer

    IF outer > 0 THEN
        DIM inner = 20
        LET outer = 30   ' LET crea nuovo locale? No — LET crea in scope corrente = blocco IF
        PRINT "Dentro IF: inner ="; inner; " outer ="; outer
    END IF

    ' inner non esiste piu' (era nel blocco IF)
    ' outer esiste ancora (era nello scope procedura)
    PRINT "Dopo IF: outer ="; outer
    PRINT
END SUB

SUB TestForBlock()
    DIM i AS INTEGER
    DIM found = "no"

    FOR i = 1 TO 3
        DIM loop_var = i * 10
        PRINT "Dentro FOR: i ="; i; " loop_var ="; loop_var
        found = "yes"
    NEXT i

    ' loop_var NON esiste qui — era locale al FOR
    PRINT "Dopo FOR: i ="; i; " found ="; found
    PRINT
END SUB

SUB TestTryBlock()
    DIM beforeTry = 100

    TRY
        DIM insideTry = 200
        PRINT "Dentro TRY: insideTry ="; insideTry
    CATCH e AS ERR
        ' insideTry NON esiste qui
        PRINT "CATCH: errore numero"; e.Number
    END TRY

    ' insideTry NON esiste piu' (era nel blocco TRY)
    PRINT "Dopo TRY: beforeTry ="; beforeTry
    PRINT
END SUB

SUB TestLetInBlockShadows()
    DIM x = 1000

    IF TRUE THEN
        ' LET crea NUOVO locale (shadowing), non modifica l'outer x
        LET x = 2000
        PRINT "Dentro IF con LET: x ="; x
    END IF

    ' Il LET era locale al blocco — x e' tornata al valore originale
    PRINT "Dopo IF: x ="; x
    PRINT
END SUB

SUB TestImplicitInBlockModifies()
    DIM y = 5000

    IF TRUE THEN
        ' Assegnazione implicita MODIFICA la y esterna (cerca su, modifica se trovato)
        y = 6000
        PRINT "Dentro IF con implicita: y ="; y
    END IF

    PRINT "Dopo IF: y ="; y  ' y e' stata MODIFICATA (6000)
    PRINT
END SUB

SUB Main()
    CLS
    PRINT "=== Test Block Scope Isolation ==="
    PRINT

    PRINT "--- Test IF block scope ---"
    TestIfBlock()

    PRINT "--- Test FOR block scope ---"
    TestForBlock()

    PRINT "--- Test TRY block scope ---"
    TestTryBlock()

    PRINT "--- Test LET shadowing in block ---"
    TestLetInBlockShadows()

    PRINT "--- Test Implicit modifies outer in block ---"
    TestImplicitInBlockModifies()

    PRINT "=== Fine test ==="
END SUB
