' MiniTest DBF — lock, durabilità, group-commit, filtro, LOOKUP (simboli senza sample).
' Ciclo CREATE -> USE diretto (il reopen è coperto da dbf_state.bas).
' ORACOLO: la riga attesa qui sotto è dichiarata solo quando i backend concordano.
' EXPECT: rlock: si|flock: si|neterr: no|setdur: [-1 ]|flushdur: [-1 ]|groupbegin: -1|groupend: -1|filter0: []|filter1: [ID = 'A2']|lookup: [BOB]
IMPORT DBF
IMPORT IO

TYPE TMisc
    ID AS STRING * 10
    NOME AS STRING * 10
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_dbf_misc.dbf"

    DBF CREATE p FROM TYPE TMisc
    DBF APPEND RECORD {ID: "A1", NOME: "ALICE"}
    DBF APPEND RECORD {ID: "A2", NOME: "BOB"}
    DBF USE p ALIAS M
    DBF GO TOP

    ' --- lock
    IF DBF.RLOCK() THEN PRINT "rlock: si" ELSE PRINT "rlock: no"
    IF DBF.FLOCK() THEN PRINT "flock: si" ELSE PRINT "flock: no"

    ' --- errori e durabilità
    IF DBF.NETERR() THEN PRINT "neterr: si" ELSE PRINT "neterr: no"
    PRINT "setdur: ["; DBF.SETDURABILITY("full"); "]"
    PRINT "flushdur: ["; DBF.FLUSHDURABILITY(); "]"

    ' --- group commit
    PRINT "groupbegin: "; DBF.BEGINGROUPCOMMIT()
    PRINT "groupend: "; DBF.ENDGROUPCOMMIT()

    ' --- filtro: vuoto, poi impostato via comando, poi riletto
    PRINT "filter0: ["; DBF.DBFILTER(); "]"
    DBF SET FILTER TO "ID = 'A2'"
    PRINT "filter1: ["; DBF.DBFILTER(); "]"

    ' --- filtro AZZERATO prima del LOOKUP: i backend gestionali memorizzano il filtro ma non
    ' lo applicano alla scansione, mentre il motore C lo applica → senza azzerarlo il LOOKUP
    ' sul C non troverebbe nulla e i cinque divergerebbero.
    DBF SET FILTER TO ""

    ' --- LOOKUP: valore di un campo cercando su un altro
    PRINT "lookup: ["; DBF.LOOKUP("NOME", "A2", "ID", "M"); "]"

    DBF CLOSE
    KILL p
END SUB
