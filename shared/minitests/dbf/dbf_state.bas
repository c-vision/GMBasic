' MiniTest DBF — funzioni di STATO dell'area (il dominio DBF non aveva alcun sample:
' 2 simboli su 46 coperti). Path ASSOLUTO (/tmp): la CWD differisce fra i backend.
' ORACOLO: il MiniTest è il proprio oracolo (riga attesa qui sotto).
' EXPECT: used1: si|selected:  1|alias: D|lastrec:  2|recno:  1|recsize:  20|field1: [ID]|field2: [NOME]|deleted: no|neterr: no|dbfilter: []|dbrelation: []|indexord:  0|indexkey: []|bof_iniziale: no|recno_top:  1|recno_skip:  2|found: no
IMPORT DBF
IMPORT IO

TYPE TStato
    ID AS STRING * 10
    NOME AS STRING * 10
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_dbf_state.dbf"

    ' NB: NON si asserisce USED() a inizio sample: senza argomenti risponde sull'AREA
    ' CORRENTE, e in una run di suite (tutti i MiniTest nello stesso processo) un altro
    ' sample può averne lasciata una aperta → il valore dipende dall'ordine di esecuzione.
    ' Le asserzioni di questo sample partono dopo il DBF USE, quando l'area è quella attesa.

    DBF CREATE p FROM TYPE TStato
    DBF APPEND RECORD {ID: "A1", NOME: "ALICE"}
    DBF APPEND RECORD {ID: "A2", NOME: "BOB"}
    DBF CLOSE

    ' Ciclo realistico CREATE -> CLOSE -> USE. Il reopen e' un gap noto della VM/C
    ' (gmb_db_dbf_open non ripristina lo schema dall'header) → su VMC le funzioni di stato
    ' dell'area riaperta danno 0/-1/""; il campione resta ROSSO finche' non si chiude, come
    ' documentato in TODO/(open) - MiniTest-Oracolo-Copertura-e-Parita-5-Backends.
    DBF USE p ALIAS D
    IF DBF.USED() THEN PRINT "used1: si" ELSE PRINT "used1: no"
    PRINT "selected: "; DBF.SELECTED()
    PRINT "alias: "; DBF.ALIAS()
    PRINT "lastrec: "; DBF.LASTREC()
    PRINT "recno: "; DBF.RECNO()
    PRINT "recsize: "; DBF.RECSIZE()
    PRINT "field1: ["; DBF.FIELDNAME(1, "D"); "]"
    PRINT "field2: ["; DBF.FIELDNAME(2, "D"); "]"
    IF DBF.DELETED() THEN PRINT "deleted: si" ELSE PRINT "deleted: no"
    IF DBF.NETERR() THEN PRINT "neterr: si" ELSE PRINT "neterr: no"
    PRINT "dbfilter: ["; DBF.DBFILTER(); "]"
    PRINT "dbrelation: ["; DBF.DBRELATION(); "]"
    PRINT "indexord: "; DBF.INDEXORD()
    PRINT "indexkey: ["; DBF.INDEXKEY(); "]"

    ' --- posizionamento
    IF DBF.BOF() THEN PRINT "bof_iniziale: si" ELSE PRINT "bof_iniziale: no"
    DBF GO TOP
    PRINT "recno_top: "; DBF.RECNO()
    DBF SKIP
    PRINT "recno_skip: "; DBF.RECNO()
    IF DBF.FOUND() THEN PRINT "found: si" ELSE PRINT "found: no"

    DBF CLOSE
    KILL p
END SUB
