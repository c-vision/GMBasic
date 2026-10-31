' FILE: Modules/ConstStructuralCanary.bas
' =================================================================================================
'   CANARINO FASE C (audit Fase A/D — Linker-Architecture-IL-VM-Parity-TODO.md)
'
'   Costante PowerBASIC-style (%K, %NOME) in OGNI posizione strutturale. Prima dei fix:
'   - i ConstStmt non venivano MAI registrati in _constants (6 siti 'or ConstStmt continue')
'     -> il fallback del compilatore era morto, %NOME cascava a NIL/0 nelle posizioni NON
'     riscritte dal Linker (es. MATCH %K2 -> default);
'   - gli statement con figli Expr non riscritti (MatchStmt, tuple, ...) lasciavano le
'     chiamate a FUNCTION utente come ArrayAccessExpr non risolti -> OP_PUSH_NIL.
'
'   Posizioni coperte: PRINT, IF, SELECT CASE, MATCH (soggetto), MATCH con UDF, tuple
'   literal, indice di array, argomento di FUNCTION utente.
' =================================================================================================

CONST %K = 2
CONST %NOME = "Mario"

FUNCTION Doppio(x AS INTEGER) AS INTEGER
    RETURN x * 2
END FUNCTION

SUB Main()
    ' 1. PRINT
    PRINT "K="; %K
    PRINT "nome="; %NOME

    ' 2. IF
    IF %K = 2 THEN PRINT "if-ok" ELSE PRINT "if-FAIL"

    ' 3. SELECT CASE
    SELECT CASE %K
        CASE 1
            PRINT "select-FAIL"
        CASE 2
            PRINT "select-ok"
        CASE ELSE
            PRINT "select-FAIL"
    END SELECT

    ' 4. MATCH (costante come soggetto — prima cascava a default)
    MATCH %K
        CASE 1 =>
    PRINT "match-FAIL"
        CASE 2 =>
    PRINT "match-ok"
        CASE _ =>
    PRINT "match-FAIL"
    END MATCH

    ' 5. MATCH con UDF (posizione NON riscritta prima della Fase D -> OP_PUSH_NIL)
    MATCH Doppio(2)
        CASE 1 =>
    PRINT "udf-FAIL"
        CASE 4 =>
    PRINT "udf-ok"
        CASE _ =>
    PRINT "udf-FAIL"
    END MATCH

    ' 6. Tuple literal con costante
    DIM t = (%K, %K + 1)
    PRINT "t0="; t(0); " t1="; t(1)

    ' 7. Indice di array con costante
    DIM arr(3) AS INTEGER
    arr(%K) = 99
    PRINT "arr="; arr(2)

    ' 8. Costante come argomento di FUNCTION utente
    PRINT "doppio="; Doppio(%K)
END SUB
