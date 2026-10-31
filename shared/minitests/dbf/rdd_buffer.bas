' MiniTest RDD — row-buffer (table buffering Clipper) su area ISAM con UDT.
' Copre il caso bufferizzato (chiusura del residuo dichiarato in
' Todo/(open) - RDD-Row-Buffer-Motore-C e ADR-RowBuffer-RDD-Canonica):
' BUFFER(mode 2/3), deviazione delle scritture nel buffer, GETFIELD/OLDVAL/GETFLDSTATE
' letti dal buffer, TABLEREVERT (scarta senza toccare il motore), TABLEUPDATE (applica
' al motore con una scrittura). Indici campo = ordinale Clipper 1-based.
' IMPORT IO ISAM RDD
' EXPECT: buf: -1|gf: [ 99999 ]|old: [ 45000 ]|st:  1|rev: -1|post-rev: [ 45000 ]|buf2: -1|gf2: [R&D]|up: -1|eng: [R&D]|st0:  0|off:  0|buf3: -1|gfn: [NAMEMODE]|stn:  1|oldn: [R&D]|stc:  0|rev2: -1
IMPORT IO
IMPORT ISAM
IMPORT RDD

TYPE TBuf
    ID AS STRING * 8
    Nome AS STRING * 32
    Sal AS DOUBLE
    Dip AS STRING * 16
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_rddbuf.isdb"
    KILL p
    DIM e AS TBuf
    ISAM OPEN p OF TBuf ALIAS EMP INDEXED BY ID
    e.ID = "E1" : e.Nome = "MARIO" : e.Sal = 45000 : e.Dip = "IT"
    ISAM APPEND EMP, e
    ISAM GO EMP, TOP
    ISAM GET EMP, e

    ' --- pessimistico (mode 2): modifica, lettura dal buffer, revert ---
    PRINT "buf: "; RDD.BUFFER(1, 2)
    RDD SET FIELD 1, 3, 99999
    PRINT "gf: ["; RDD.GETFIELD(1, 3); "]"
    PRINT "old: ["; RDD.OLDVAL(1, 3); "]"
    PRINT "st: "; RDD.GETFLDSTATE(1, 3)
    PRINT "rev: "; RDD.TABLEREVERT(1)
    PRINT "post-rev: ["; RDD.GETFIELD(1, 3); "]"

    ' --- ottimistico (mode 3): due campi, commit, motore aggiornato ---
    PRINT "buf2: "; RDD.BUFFER(1, 3)
    RDD SET FIELD 1, 3, 77000
    RDD SET FIELD 1, 4, "R&D"
    PRINT "gf2: ["; RDD.GETFIELD(1, 4); "]"
    PRINT "up: "; RDD.TABLEUPDATE(1)
    ISAM GO EMP, TOP
    ISAM GET EMP, e
    PRINT "eng: ["; e.Dip; "]"
    PRINT "st0: "; RDD.GETFLDSTATE(1, 3)
    PRINT "off: "; RDD.BUFFER(1, 1)

    ' --- nome: SET FIELD/GETFIELD/OLDVAL per nome con UDT (chiave = SymbolId) ---
    PRINT "buf3: "; RDD.BUFFER(1, 2)
    RDD SET FIELD 1, "Dip", "NAMEMODE"
    PRINT "gfn: ["; RDD.GETFIELD(1, "Dip"); "]"
    PRINT "stn: "; RDD.GETFLDSTATE(1, "Dip")
    PRINT "oldn: ["; RDD.OLDVAL(1, "Dip"); "]"
    PRINT "stc: "; RDD.GETFLDSTATE(1, "Nome")
    PRINT "rev2: "; RDD.TABLEREVERT(1)
    ISAM CLOSE EMP
    KILL p
END SUB
