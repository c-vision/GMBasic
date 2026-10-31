' MiniTest RDD — row-buffer con RDD REPLACE (per nome) e getter per nome/indice,
' su area RDD pura (DBF via RDD USE). Copre la deviazione REPLACE→buffer (ADR §6),
' OLDVAL/GETFIELD/GETFLDSTATE con chiave numerica 1-based E nome, BUFFER(…,1)=off
' con pending perso, TABLEREVERT. Verde su il/pcode/vmc/c99/c99n.
' EXPECT: gf-num: ALICE|gf-name: ALICE|buf: -1|gf2: ZARA|old2: ALICE|old3:  100|st2:  1|st3:  1|off:  0|dopo-off: ALICE|buf3: -1|rev: -1|dopo-rev: ALICE|up:  0|finale: FINE|buf4: -1|gf4: [PER_NOME]|old4: [FINE]|st4:  1|st4b:  1|up4: -1|finale4: PER_NOME
IMPORT RDD
IMPORT DBF
TYPE TT2
    ID AS STRING * 10
    NOME AS STRING * 10
    SAL AS DOUBLE
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_rddbuf_repl.dbf"
    KILL p
    DBF CREATE p FROM TYPE TT2
    DBF APPEND RECORD {ID: "A1", NOME: "ALICE", SAL: 100}
    DBF CLOSE
    RDD USE p ALIAS R
    PRINT "gf-num: "; RDD.GETFIELD(1, 2)
    PRINT "gf-name: "; RDD.GETFIELD(1, "NOME")
    PRINT "buf: "; RDD.BUFFER(1, 2)
    RDD REPLACE NOME WITH "ZARA", SAL WITH 500
    PRINT "gf2: "; RDD.GETFIELD(1, 2)
    PRINT "old2: "; RDD.OLDVAL(1, "NOME")
    PRINT "old3: "; RDD.OLDVAL(1, 3)
    PRINT "st2: "; RDD.GETFLDSTATE(1, "NOME")
    PRINT "st3: "; RDD.GETFLDSTATE(1, 3)
    PRINT "off: "; RDD.BUFFER(1, 1)
    PRINT "dopo-off: "; RDD.GETFIELD(1, 2)
    PRINT "buf3: "; RDD.BUFFER(1, 3)
    RDD SET FIELD 1, 2, "TEMP"
    PRINT "rev: "; RDD.TABLEREVERT(1)
    PRINT "dopo-rev: "; RDD.GETFIELD(1, 2)
    RDD REPLACE NOME WITH "FINE"
    PRINT "up: "; RDD.TABLEUPDATE(1)
    PRINT "finale: "; RDD.GETFIELD(1, 2)
    PRINT "buf4: "; RDD.BUFFER(1, 2)
    RDD SET FIELD 1, "NOME", "PER_NOME"
    PRINT "gf4: ["; RDD.GETFIELD(1, "NOME"); "]"
    PRINT "old4: ["; RDD.OLDVAL(1, "NOME"); "]"
    PRINT "st4: "; RDD.GETFLDSTATE(1, "NOME")
    PRINT "st4b: "; RDD.GETFLDSTATE(1, 2)
    PRINT "up4: "; RDD.TABLEUPDATE(1)
    PRINT "finale4: "; RDD.GETFIELD(1, 2)
    RDD CLOSE
    KILL p
END SUB
