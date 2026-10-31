' MiniTest RDD — funzioni di campo/stato senza sample (11 simboli).
' Firme reali da StdLib.Rdd.cs: GETFIELD(area, field), OLDVAL(area, field),
' GETFLDSTATE(area, field), VALTYPE(area), CURVAL(value), SETDELETED(area, flag),
' TABLEREVERT/TABLEUPDATE(area), RECALL(scope, expr), TODAY(), FLOAT(expr).
' TODAY() è data-dipendente: verificata come proprietà, non come valore.
' EXPECT: valtype: [DBF]|float:  12.5|today: si|getfield: [ALICE]|curval:  1.5|oldval: []|fldstate:  0|setdeleted: si|recall:  2|tableupdate:  0|tablerevert:  0|buffer: no
IMPORT RDD
IMPORT DBF
IMPORT IO

TYPE TRdd
    ID AS STRING * 10
    NOME AS STRING * 10
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_rdd.dbf"

    DBF CREATE p FROM TYPE TRdd
    DBF APPEND RECORD {ID: "A1", NOME: "ALICE"}
    DBF APPEND RECORD {ID: "A2", NOME: "BOB"}
    DBF CLOSE

    RDD USE p ALIAS R
    DBF GO TOP

    PRINT "valtype: ["; RDD.VALTYPE(1); "]"
    PRINT "float: "; RDD.FLOAT(12.5)
    IF RDD.TODAY() > 0 THEN PRINT "today: si" ELSE PRINT "today: no"

    PRINT "getfield: ["; RDD.GETFIELD(1, 2); "]"
    PRINT "curval: "; RDD.CURVAL(1.5)
    PRINT "oldval: ["; RDD.OLDVAL(1, 2); "]"
    PRINT "fldstate: "; RDD.GETFLDSTATE(1, 2)

    IF RDD.SETDELETED(1, 0) THEN PRINT "setdeleted: si" ELSE PRINT "setdeleted: no"
    PRINT "recall: "; RDD.RECALL("ALL", TRUE)
    PRINT "tableupdate: "; RDD.TABLEUPDATE(1)
    PRINT "tablerevert: "; RDD.TABLEREVERT(1)

    ' --- BUFFER(area, mode): attiva il buffer a righe per l'area (bit: area aperta?).
    ' In ULTIMA posizione: attivare il buffer cambia la semantica di TABLEUPDATE/TABLEREVERT
    ' (che sopra si verificano senza buffer).
    IF RDD.BUFFER(1, 0) THEN PRINT "buffer: si" ELSE PRINT "buffer: no"

    DBF CLOSE
    KILL p
END SUB
