' FILE: Math/MathAtnVectorAngle.bas
' =================================================================================================
'       SAMPLE: ATN con 1 e 2 argomenti (2° argomento OPZIONALE)
' =================================================================================================
' ATN(y)    = arctan(y): risultato in -90°..+90° (il quadrante NON è deducibile dal rapporto)
' ATN(y, x) = atan2(y, x): angolo del vettore (x, y) in -180°..+180° — è la forma da usare per
'             calcolare rotta/bearing tra due punti, perché conserva il quadrante.
' Non esistono ATN2/ATAN2: la forma a due argomenti è ATN stessa (un solo token/una sola
' funzione, con il secondo parametro opzionale).
'
' Il sample copre anche le altre grafie toccate dalla stessa passata sulla superficie:
'   - UCASE / LCASE (alias di UPPER / LOWER, §5)
'   - LOG (la grafia Clipper LN è stata eliminata, §6)
'   - TRYLOCK (la grafia MutexTryLock non è accettata, §7)
'   - GETTEMPDIR / GETTEMPFILE (sostituiscono GetTempDirFunc / GetTempFileFunc, §8)
'
' MiniTest di riferimento: minitests/math/math_atn.bas, minitests/strings/case_alias_parity.bas,
' minitests/mutex/trylock.bas, minitests/io/io_handles.bas, minitests/misc/log.bas
' =================================================================================================

IMPORT MATH
IMPORT STRINGS
IMPORT MUTEX
IMPORT IO

SUB Main()

    PRINT "== 1. ATN con un argomento (arctan) =="
    PRINT "ATN(1)  = "; ROUND(MATH.RAD2DEG(MATH.ATN(1)), 6); " deg"
    PRINT "ATN(-1) = "; ROUND(MATH.RAD2DEG(MATH.ATN(-1)), 6); " deg"
    PRINT

    PRINT "== 2. ATN con due argomenti (atan2): i quattro quadranti =="
    PRINT "ATN(1, 1)   = "; ROUND(MATH.RAD2DEG(MATH.ATN(1, 1)), 6); " deg (NE)"
    PRINT "ATN(1, -1)  = "; ROUND(MATH.RAD2DEG(MATH.ATN(1, -1)), 6); " deg (NO)"
    PRINT "ATN(-1, -1) = "; ROUND(MATH.RAD2DEG(MATH.ATN(-1, -1)), 6); " deg (SO)"
    PRINT "ATN(-1, 1)  = "; ROUND(MATH.RAD2DEG(MATH.ATN(-1, 1)), 6); " deg (SE)"
    PRINT "ATN(0, -1)  = "; ROUND(MATH.RAD2DEG(MATH.ATN(0, -1)), 6); " deg (Ovest)"
    PRINT

    PRINT "== 3. Rotta tra due punti (uso reale) =="
    DIM x1 = 3.0
    DIM y1 = 1.0
    DIM x2 = -2.0
    DIM y2 = 5.0
    DIM dx = x2 - x1
    DIM dy = y2 - y1
    PRINT "da (3, 1) a (-2, 5): dx = "; dx; "  dy = "; dy
    PRINT "rotta ATN(dy, dx)   = "; ROUND(MATH.RAD2DEG(MATH.ATN(dy, dx)), 6); " deg"
    PRINT "ATN(dy / dx) 1 arg  = "; ROUND(MATH.RAD2DEG(MATH.ATN(dy / dx)), 6); " deg  <- quadrante perso"
    PRINT

    PRINT "== 4. Distanza e rotta normalizzata 0..360 =="
    PRINT "distanza              = "; ROUND(MATH.SQR(dx * dx + dy * dy), 6)
    ' Rotta di ritorno: ATN(-dy, -dx) e' negativa → +360 per la rotta 0..360 (con ATN a un
    ' argomento questa normalizzazione non e' ricavabile: il quadrante e' perso).
    DIM rotta_ritorno = MATH.RAD2DEG(MATH.ATN(-dy, -dx))
    PRINT "ritorno ATN(-dy,-dx)  = "; ROUND(rotta_ritorno, 6); " deg"
    IF rotta_ritorno < 0 THEN
        rotta_ritorno = rotta_ritorno + 360
    END IF
    PRINT "ritorno (0..360)      = "; ROUND(rotta_ritorno, 6); " deg"
    PRINT

    PRINT "== 5. Registro della rotta (UCASE / LCASE) =="
    ' UCASE/LCASE sono ALIAS di UPPER/LOWER: stessa funzione, grafia Clipper. Nessun token
    ' proprio: qui si usa la forma qualificata su STRINGS per la stessa procedura.
    DIM etichetta = STRINGS.UCASE("rotta-")
    DIM nota = STRINGS.LCASE("NORD-OVEST")
    PRINT "etichetta = "; etichetta
    PRINT "nota      = "; nota
    PRINT

    PRINT "== 6. Logaritmo naturale (LOG) =="
    ' Il logaritmo naturale si chiama SOLO LOG: la grafia Clipper LN non esiste piu'.
    PRINT "LOG(e)    = "; ROUND(MATH.LOG(MATH.EXP(1)), 6)
    PRINT "LOG(1)    = "; ROUND(MATH.LOG(1), 6)
    PRINT

    PRINT "== 7. Sezione critica non bloccante (TRYLOCK) =="
    DIM mu AS MUTEX
    DIM preso = MUTEX.TRYLOCK(mu)
    DIM ripreso = MUTEX.TRYLOCK(mu)
    MUTEX UNLOCK mu
    PRINT "primo TRYLOCK   = "; preso
    PRINT "secondo TRYLOCK = "; ripreso
    PRINT

    PRINT "== 8. Report su file temporaneo (GETTEMPDIR / GETTEMPFILE) =="
    DIM dir = IO.GETTEMPDIR()
    DIM tmp = IO.GETTEMPFILE()
    IF LEN(dir) > 0 THEN PRINT "GETTEMPDIR ok" ELSE PRINT "GETTEMPDIR FAIL"
    IF LEN(tmp) > 0 THEN PRINT "GETTEMPFILE ok" ELSE PRINT "GETTEMPFILE FAIL"
    DIM h = IO.FCREATE(tmp)
    IO.FWRITE(h, etichetta + nota)
    IO.FCLOSE(h)
    DIM r = IO.FOPEN(tmp)
    PRINT "riletto         = "; IO.FREAD(r)
    IO.FCLOSE(r)
    KILL tmp
    IF IO.FILEEXISTS(tmp) = 0 THEN PRINT "report rimosso" ELSE PRINT "report ancora presente"
    PRINT
    PRINT "Fine."

END SUB
