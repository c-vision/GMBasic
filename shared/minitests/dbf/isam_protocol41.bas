' MiniTest ISAM — protocollo §4.1: parità dei comportamenti dei comandi ISAM,
' CONVERSIONS.CSTR/STR$ e truthiness di IF su stringa. Deterministico, ESC-free.
' Copre: OPEN multi-indice, GET (CURRENCY/BIT), SEEK numerico + secondario (USING),
' SET ORDER (alias + indice runtime), SET FILTER (legacy + lambda), CREATE INDEX,
' RECNO/EOF/BOF/FOUND, SELECTED/RECSIZE, RLOCK/FLOCK.
' EXPECT: CSTRstr:[Alfa]|CSTRnum:[ 42]|CSTRbool:[True]|IFstr:TRUE|GET1:  1 Alfa  12.34 True|SEEKnum: Beta|SEEKsec:  1|ORDERsaldo: Beta|FILTlegacy: Alfa|FILTlambda: Alfa|RECNO:  1|BOF: -1 EOF:  0|SEL:  1|SIZE:  66|RLOCK:  1|FLOCK:  1
IMPORT ISAM

TYPE Tcliente
    ID AS LONG
    Nome AS STRING * 20
    Saldo AS CURRENCY
    Attivo AS BIT
END TYPE

SUB Main()
    DIM r AS Tcliente
    DIM s AS STRING
    ON ERROR RESUME NEXT
    KILL "/tmp/gmb_mt_isam41.isdb"
    ON ERROR GOTO 0
    ISAM OPEN "/tmp/gmb_mt_isam41.isdb" OF Tcliente ALIAS C INDEXED BY ID, Nome
    r.ID = 1 : r.Nome = "Alfa" : r.Saldo = 12.34 : r.Attivo = TRUE
    ISAM APPEND C, r
    r.ID = 2 : r.Nome = "Beta" : r.Saldo = -5.5 : r.Attivo = FALSE
    ISAM APPEND C, r

    PRINT "CSTRstr:[" + CONVERSIONS.CSTR("Alfa") + "]"
    PRINT "CSTRnum:[" + CONVERSIONS.CSTR(42) + "]"
    PRINT "CSTRbool:[" + CONVERSIONS.CSTR(TRUE) + "]"
    s = CONVERSIONS.CSTR(TRUE)
    IF s THEN
        PRINT "IFstr:TRUE"
    ELSE
        PRINT "IFstr:FALSE"
    END IF

    ISAM GO C, TOP
    ISAM GET C, r
    PRINT "GET1: " + CONVERSIONS.CSTR(r.ID) + " " + r.Nome + " " + CONVERSIONS.CSTR(r.Saldo) + " " + CONVERSIONS.CSTR(r.Attivo)

    ISAM SEEK C, 2
    IF ISAM.FOUND(C) THEN ISAM GET C, r
    PRINT "SEEKnum: " + r.Nome

    ISAM SET ORDER TO "Nome" ON C
    ISAM SEEK -1, "Alfa" USING "Nome"
    IF ISAM.FOUND(C) THEN ISAM GET C, r
    PRINT "SEEKsec: " + CONVERSIONS.CSTR(r.ID)
    ISAM SET ORDER TO "ID" ON C

    ISAM CREATE INDEX "BySaldo" ON C KEY (Saldo) LEN 8
    ISAM SET ORDER TO "BySaldo" ON C
    ISAM GO C, TOP
    ISAM GET C, r
    PRINT "ORDERsaldo: " + r.Nome
    ISAM SET ORDER TO "ID" ON C

    ISAM SET FILTER TO Saldo > 0 ON C
    ISAM GO C, TOP
    ISAM GET C, r
    PRINT "FILTlegacy: " + r.Nome
    ISAM SET FILTER TO FN(x) => x.Attivo = TRUE ON C
    ISAM GO C, TOP
    ISAM GET C, r
    PRINT "FILTlambda: " + r.Nome
    ISAM SET FILTER TO "" ON C

    ISAM GO C, TOP
    PRINT "RECNO: " + CONVERSIONS.CSTR(ISAM.RECNO(C))
    ISAM SKIP C, -1
    PRINT "BOF: " + CONVERSIONS.CSTR(ISAM.BOF(C)) + " EOF: " + CONVERSIONS.CSTR(ISAM.EOF(C))
    PRINT "SEL: " + CONVERSIONS.CSTR(ISAM.SELECTED(C))
    PRINT "SIZE: " + CONVERSIONS.CSTR(ISAM.RECSIZE(C))

    ISAM GO C, TOP
    PRINT "RLOCK: " + CONVERSIONS.CSTR(ISAM.RLOCK(C))
    PRINT "FLOCK: " + CONVERSIONS.CSTR(ISAM.FLOCK(C))
    ISAM UNLOCK C

    ISAM CLOSE C
    KILL "/tmp/gmb_mt_isam41.isdb"
END SUB
