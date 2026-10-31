' FILE: Clipper/ClipperMacroStaticError.bas
' =================================================================================================
'   CANARINO — STATIC in EVAL = errore esplicito (semantica S_eval vincolante 2026-09-09:
'   lo scope eval è transiente, STATIC richiederebbe persistenza → mai no-op silenzioso).
'   Comportamento PER-BACKEND (divergenza nota e voluta, non un gap):
'   - IL/PCodeVM (C#): l'errore torna come VALORE "MACRO ERROR: ..." (mai abort) → caso -ok.
'   - VM/C (C): l'errore abortisce l'EVAL con "[VM ERROR] MACRO (&) eval failed", exit 1.
'     I casi 1-7 prima di questo restano verdi; l'abort è l'asserzione su VM/C.
'   - C99 (transpiled): l'errore resta silenzioso (nil) — l'emitter non controlla il
'     codice di ritorno di gmb_expr (come prima di S_eval); caso informativo.
' =================================================================================================
GLOBAL g AS INTEGER

SUB Main()
    g = 1
    PRINT "static-err-begin"
    DIM m AS STRING
    m = EVAL("STATIC st AS INTEGER")
    PRINT "static-err-or-abort:"; m
    IF m = "MACRO ERROR: STATIC is not allowed inside EVAL/macro: eval scope is transient." THEN PRINT "static-err-ok" ELSE PRINT "static-err-FAIL"
END SUB
