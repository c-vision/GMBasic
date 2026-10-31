' FILE: samples/Community/Arrays/ArrayLiteralBrackets.bas
' ======================================================================================================================
' Inizializzatore di array con la forma a PARENTESI QUADRE `[ ... ]`
' ======================================================================================================================
' Stessa semantica del letterale graffa `{ ... }`: un `[ ... ]` usato come inizializzatore di
' un DIM che dichiara un array riempie l'array elemento per elemento. Copre:
'   - array implicito (nessuna dimensione): DIM a() AS T = [ ... ]   -> size = n. elementi
'   - array dimensionato:                   DIM b(n) AS T = [ ... ]  -> i primi elementi dai valori
'   - tipi DOUBLE / INTEGER / STRING
' Verificato in parita' sui 4 backend (IL, P-Code, VM/C, C99Native).
SUB Main()
    PRINT "=== ArrayLiteralBrackets: DIM ... = [ ... ] ==="

    DIM a() AS DOUBLE = [1.5, 2.5, 3.5]
    PRINT "a="; a(0); a(1); a(2)

    DIM b(4) AS DOUBLE = [10.0, 20.0, 30.0]
    PRINT "b="; b(0); b(1); b(2); b(3); b(4)

    DIM c() AS INTEGER = [1, 2, 3, 4]
    PRINT "c="; c(0); c(1); c(2); c(3)

    DIM s() AS STRING = ["aa", "bb", "cc"]
    PRINT "s="; s(0); s(1); s(2)

    ' Somma con un ciclo (verifica che i valori siano davvero nell'array).
    DIM tot AS DOUBLE = 0
    FOR i = 0 TO 2
        tot = tot + a(i)
    NEXT i
    PRINT "tot="; tot

    ' --- Matrice 2-D con letterale annidato `[[..],[..]]` (come `{{..},{..}}`) ---
    DIM mx(1, 1) AS INTEGER = [[1, 2], [3, 4]]
    PRINT "mx="; mx(0,0); mx(0,1); mx(1,0); mx(1,1)

    DIM md(2, 1) AS DOUBLE = [[1.5, 2.5], [3.5, 4.5], [5.5, 6.5]]
    PRINT "md(0,0)="; md(0,0); " md(2,1)="; md(2,1)

    PRINT "ArrayLiteralBrackets completato."
END SUB
