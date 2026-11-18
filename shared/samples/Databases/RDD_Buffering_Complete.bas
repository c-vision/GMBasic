' =====================================================================
' RDD_Buffering_Complete.bas — Table buffering RDD (Clipper) su 4 backend
' ---------------------------------------------------------------------
' Copre TUTTA la superficie del row-buffer: RDD.BUFFER (mode 2 pessimistico,
' 3 ottimistico, 1/0 = off), RDD SET FIELD (campo numerico), RDD REPLACE
' (record corrente, per nome), RDD.GETFIELD / RDD.OLDVAL / RDD.GETFLDSTATE
' (forme con INDICE 1-based e con NOME), RDD.TABLEUPDATE / RDD.TABLEREVERT,
' e il comando SET DELETED (senza buffer attivo).
' Aree RDD pure (DBF via RDD USE): i campi sono celle testuali lette/toccate
' dal motore RDD; il buffer devia le scritture finché TABLEUPDATE non applica.
' Indici campo = ordinale Clipper 1-based (1..n); OLDVAL = snapshot.
' Semantica canonica: Decisions/ADR-RowBuffer-RDD-Canonica.md.
' Parità verificata: il, pcode, vmc, c99, c99n (tools/parity4 --legs all).
' =====================================================================
IMPORT RDD
IMPORT DBF
IMPORT IO

TYPE TAddetti
    CODICE AS STRING * 8
    NOME   AS STRING * 20
    STIPENDIO AS DOUBLE
    UFFICIO AS STRING * 8
END TYPE

SUB Main()
    DIM p AS STRING
    p = PROJECTFOLDER() + "/work"
    IF NOT IO.DIREXISTS(p) THEN MKDIR p
    p = p + "/buf_complete.dbf"
    KILL p

    DIM a AS TAddetti
    DBF CREATE p FROM TYPE TAddetti
    DBF APPEND RECORD {CODICE: "D001", NOME: "ROSSI", STIPENDIO: 35000, UFFICIO: "AMM"}
    DBF APPEND RECORD {CODICE: "D002", NOME: "NERI",  STIPENDIO: 42000, UFFICIO: "TEC"}
    DBF CLOSE

    RDD SET DEFAULT "DBF"
    RDD USE p ALIAS ADDETTI
    PRINT "valtype: "; RDD.VALTYPE(1)
    PRINT "area: "; RDD.GETFIELD(1, 2)

    ' --- 1. BUFFER pessimistico (mode 2) + RDD SET FIELD (campo 3) ---
    PRINT "buf2: "; RDD.BUFFER(1, 2)
    RDD SET FIELD 1, 3, 99000
    PRINT "gf-num: "; RDD.GETFIELD(1, 3)
    PRINT "gf-name: "; RDD.GETFIELD(1, "NOME")
    PRINT "old-num: "; RDD.OLDVAL(1, 3)
    PRINT "old-name: "; RDD.OLDVAL(1, "NOME")
    PRINT "state-mod: "; RDD.GETFLDSTATE(1, 3)
    PRINT "state-clean: "; RDD.GETFLDSTATE(1, 2)
    PRINT "revert: "; RDD.TABLEREVERT(1)
    PRINT "post-revert: "; RDD.GETFIELD(1, 3)

    ' --- 2. BUFFER ottimistico (mode 3) + RDD REPLACE (per nome) + commit ---
    PRINT "buf3: "; RDD.BUFFER(1, 3)
    RDD SET FIELD 1, 2, "BIANCHI"
    RDD REPLACE NOME WITH "VERDI", UFFICIO WITH "DIR"
    PRINT "gf-dopo-replace: "; RDD.GETFIELD(1, 2)
    PRINT "state-replaced: "; RDD.GETFLDSTATE(1, 2)
    PRINT "update: "; RDD.TABLEUPDATE(1)
    PRINT "post-update: "; RDD.GETFIELD(1, 2)
    PRINT "ufficio: "; RDD.GETFIELD(1, 4)

    ' --- 3. BUFFER(…,1) = OFF: le modifiche pending si PERDONO ---
    PRINT "buf-riapri: "; RDD.BUFFER(1, 2)
    RDD SET FIELD 1, 2, "PENDENTE"
    PRINT "off: "; RDD.BUFFER(1, 1)
    PRINT "dopo-off: "; RDD.GETFIELD(1, 2)
    PRINT "state-senza-buffer: "; RDD.GETFLDSTATE(1, 2)
    PRINT "oldval-senza-buffer: ["; RDD.OLDVAL(1, 2); "]"

    ' --- nome sul buffer attivo (campo per NOME: chiave reale del driver) ---
    PRINT "buf-nome: "; RDD.BUFFER(1, 2)
    RDD SET FIELD 1, "NOME", "DI_NOME"
    PRINT "gf-nome: "; RDD.GETFIELD(1, "NOME")
    PRINT "old-nome: "; RDD.OLDVAL(1, "NOME")
    PRINT "st-nome: "; RDD.GETFLDSTATE(1, "NOME")
    PRINT "upd-nome: "; RDD.TABLEUPDATE(1)

    ' RLOCK/RECNO NON qui: divergenze PREESISTENTI fuori perimetro row-buffer
    ' (lock dipende da apertura esclusiva del file; RDD.RECNO su VM/C legge il ramo
    ' DBF con handle residuo dopo CLOSE). Il buffering non le tocca.

    ' --- 4. SET DELETED (comando d'area, senza buffer attivo) ---
    RDD SET DELETED ON
    PRINT "setdeleted-ok"
    RDD SET DELETED OFF
    PRINT "revert-senza-buffer: "; RDD.TABLEREVERT(1)
    RDD CLOSE
    KILL p
    PRINT "=== Buffering Complete OK ==="
END SUB
