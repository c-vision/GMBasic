' EXPECT: q= 3|rem= 2
' BYREF scalare elemento LONG (2026-09-28, §1bis): il parametro viaggia come puntatore
' nativo a un SINGOLO valore (non un array) — pattern C classico div/mod, quoziente
' come ritorno, resto come OUT.
DECLARE FUNCTION VecDivMod LIB "libvec_module" ALIAS "vec_divmod" (a AS LONG, b AS LONG, BYREF r AS LONG) AS LONG

SUB Main()
    DIM remainder AS LONG
    DIM q AS LONG
    q = VecDivMod(17, 5, remainder)
    PRINT "q="; q
    PRINT "rem="; remainder
END SUB
