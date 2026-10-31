' FILE: samples/Community/Databases/RddCompleteCoverage.bas
' Sample COMPLETO RDD — usa tutte e 22 le funzioni del namespace
' (BOF BUFFER CTOD CURVAL DELETE DTOC EOF FLOAT FLOCK FOUND GETFIELD GETFLDSTATE
'  OLDVAL RECALL RECNO REPLACE RLOCK SETDELETED TABLEREVERT TABLEUPDATE TODAY VALTYPE).
' Deterministico (TODAY verificata come proprietà, non come valore).
IMPORT RDD
IMPORT DBF
IMPORT IO

TYPE TRddCov
    ID AS STRING * 10
    NOME AS STRING * 10
    VAL AS DOUBLE
END TYPE

SUB Main()
    DIM p AS STRING
    p = "work/rdd_cov.dbf"
    MKDIR "work"
    ON ERROR RESUME NEXT
    KILL p
    ON ERROR GOTO 0

    DBF CREATE p FROM TYPE TRddCov
    DBF APPEND RECORD {ID: "A1", NOME: "ALICE", VAL: 1.5}
    DBF APPEND RECORD {ID: "A2", NOME: "BOB", VAL: 2.5}
    DBF CLOSE

    RDD USE p ALIAS RC
    DBF GO TOP

    ' --- Cursore (token condivisi DBF/ISAM/RDD) ---
    PRINT "bof: "; RDD.BOF()
    PRINT "eof: "; RDD.EOF()
    PRINT "found: "; RDD.FOUND()
    PRINT "recno: "; RDD.RECNO()

    ' --- Campo e stato di modifica ---
    PRINT "valtype: ["; RDD.VALTYPE(1); "]"
    PRINT "getfield: ["; RDD.GETFIELD(1, 2); "]"
    PRINT "getfldstate: "; RDD.GETFLDSTATE(1, 2)
    PRINT "oldval: ["; RDD.OLDVAL(1, 2); "]"
    PRINT "curval: "; RDD.CURVAL(1.5)

    ' --- Conversioni ---
    DIM d = RDD.CTOD("2026-01-15")
    PRINT "ctod->dtoc: ["; RDD.DTOC(d); "]"
    IF RDD.TODAY() > 0 THEN PRINT "today: si" ELSE PRINT "today: no"
    PRINT "float: "; RDD.FLOAT(12.5)

    ' --- Scrittura e cancellazione scoped (scope CURRENT = record corrente: semantica
    '     ben definita e identica sui 4 backend). PRIMA dei lock: su IL RLOCK/FLOCK
    '     bloccano l'area e azzerano le scoped ops successive.
    PRINT "replace: "; RDD.REPLACE("CURRENT", "NOME", "X", TRUE)
    PRINT "delete: "; RDD.DELETE("CURRENT", TRUE)
    PRINT "recall: "; RDD.RECALL("CURRENT", TRUE)

    ' --- Lock ---
    IF RDD.RLOCK(1) THEN PRINT "rlock: si" ELSE PRINT "rlock: no"
    IF RDD.FLOCK(1) THEN PRINT "flock: si" ELSE PRINT "flock: no"
    IF RDD.SETDELETED(1, 0) THEN PRINT "setdeleted: si" ELSE PRINT "setdeleted: no"

    ' --- Buffer (in ULTIMA posizione: cambia la semantica di TABLEUPDATE/TABLEREVERT) ---
    IF RDD.BUFFER(1, 0) THEN PRINT "buffer: si" ELSE PRINT "buffer: no"
    PRINT "tableupdate: "; RDD.TABLEUPDATE(1)
    PRINT "tablerevert: "; RDD.TABLEREVERT(1)

    DBF CLOSE
    KILL p
    PRINT "RDD complete coverage OK"
END SUB
