' MiniTest RDD — cursore, date, lock e scoped (funzioni non coperte da
' rdd_fields/rdd_buffer). Firme da StdLib.Rdd.cs:
'   BOF()/EOF()/FOUND()/RECNO()          → 0 arg (Bit / Long)
'   CTOD(date$) → Date, DTOC(date) → stringa ISO yyyy-MM-dd
'   RLOCK(area)/FLOCK(area)              → Bit
'   DELETE(scope, expr) / REPLACE(scope, field, value, expr) → conteggio
' Scope CURRENT = record corrente (ben definito e identico sui 4 backend).
' Lock DOPO le scoped ops: su IL RLOCK/FLOCK bloccano l'area e azzerano le
' scoped ops successive.
' EXPECT: bof:  0|eof:  0|found:  0|recno:  1|ctod->dtoc: [2026-01-15]|replace:  1|getfield: [X]|delete:  1|recall:  1|rlock: si|flock: si
IMPORT RDD
IMPORT DBF
IMPORT IO

TYPE TRddCur
    ID AS STRING * 10
    NOME AS STRING * 10
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_rdd_cursor.dbf"

    DBF CREATE p FROM TYPE TRddCur
    DBF APPEND RECORD {ID: "A1", NOME: "ALICE"}
    DBF APPEND RECORD {ID: "A2", NOME: "BOB"}
    DBF CLOSE

    RDD USE p ALIAS RC
    DBF GO TOP

    PRINT "bof: "; RDD.BOF()
    PRINT "eof: "; RDD.EOF()
    PRINT "found: "; RDD.FOUND()
    PRINT "recno: "; RDD.RECNO()

    PRINT "ctod->dtoc: ["; RDD.DTOC(RDD.CTOD("2026-01-15")); "]"

    PRINT "replace: "; RDD.REPLACE("CURRENT", "NOME", "X", TRUE)
    PRINT "getfield: ["; RDD.GETFIELD(1, 2); "]"
    PRINT "delete: "; RDD.DELETE("CURRENT", TRUE)
    PRINT "recall: "; RDD.RECALL("CURRENT", TRUE)

    IF RDD.RLOCK(1) THEN PRINT "rlock: si" ELSE PRINT "rlock: no"
    IF RDD.FLOCK(1) THEN PRINT "flock: si" ELSE PRINT "flock: no"

    DBF CLOSE
    KILL p
END SUB
