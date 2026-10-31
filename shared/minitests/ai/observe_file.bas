' R1 AI OBSERVE — OBSERVEFILE/LOG/SCREEN + OBSERVATION* letture.
' Usa fixture assoluta (parità CWD tra backend). Expect FNV-1a64 del fixture.
' EXPECT: delta= 1|hash=98b20a00a591cf91|latest=si|inlist=si|file=si|count= 1
IMPORT AI

SUB Main()
    DIM path AS STRING
    path = "../../samples/Fixtures/ai/fattura_it.txt"
    c0 = AI.OBSERVATIONCOUNT()
    AI OBSERVE FILE path INTO id
    c1 = AI.OBSERVATIONCOUNT()
    PRINT "delta="; STR$(c1 - c0)
    PRINT "hash="; AI.OBSERVATIONHASH$(id)
    IF AI.OBSERVATIONLATEST$() = LTRIM$(STR$(id)) THEN PRINT "latest=si" ELSE PRINT "latest=no"
    l$ = AI.OBSERVATIONLIST$()
    IF INSTR(l$, LTRIM$(STR$(id))) > 0 THEN PRINT "inlist=si" ELSE PRINT "inlist=no"
    IF AI.OBSERVATIONFILE$(id) = path THEN PRINT "file=si" ELSE PRINT "file=no"
    PRINT "count="; STR$(AI.OBSERVATIONCOUNT())
END SUB