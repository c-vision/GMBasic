' FILE: Databases/DbfRelationJoin.bas
' SET RELATION TO ... INTO ... riposiziona il figlio allo SKIP del padre.
' JOIN WITH crea il dest con le coppie che passano FOR.

IMPORT DBF WITH ALIAS
IMPORT IO WITH ALIAS

TYPE Cust
    ID AS STRING * 10
    NAME AS STRING * 10
END TYPE

TYPE Ord
    CID AS STRING * 10
    ITEM AS STRING * 10
END TYPE

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    ON ERROR RESUME NEXT
    KILL g_work + "/rel_cust.dbf"
    KILL g_work + "/rel_ord.dbf"
    KILL g_work + "/rel_ord.ntx"
    KILL g_work + "/rel_join.dbf"
    ON ERROR GOTO 0
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work

    DBF CREATE g_work + "/rel_cust.dbf" FROM TYPE Cust
    DBF APPEND RECORD {ID: "A", NAME: "ALICE"}
    DBF APPEND RECORD {ID: "B", NAME: "BOB"}
    DBF CLOSE

    DBF CREATE g_work + "/rel_ord.dbf" FROM TYPE Ord
    DBF APPEND RECORD {CID: "A", ITEM: "X"}
    DBF APPEND RECORD {CID: "A", ITEM: "Y"}
    DBF APPEND RECORD {CID: "B", ITEM: "Z"}
    DBF CLOSE

    DBF USE g_work + "/rel_cust.dbf" ALIAS CUST
    DBF USE g_work + "/rel_ord.dbf" ALIAS ORD
    DBF SELECT ORD
    DBF INDEX ON CID TO g_work + "/rel_ord.ntx"
    DBF SET ORDER TO 1

    DBF SELECT CUST
    DBF SET RELATION TO ID INTO ORD
    DBF GO TOP
    DBF SELECT ORD
    PRINT "Figlio su ALICE: "; ITEM
    DBF SELECT CUST
    DBF SKIP
    DBF SELECT ORD
    PRINT "Figlio su BOB: "; ITEM

    DBF SELECT CUST
    DBF JOIN WITH ORD TO g_work + "/rel_join.dbf" FOR ID = CID
    DBF CLOSE DATABASES

    DBF USE g_work + "/rel_join.dbf" ALIAS J
    PRINT "JOIN recs: "; DBF.LASTREC()
    DBF CLOSE
    PRINT "=== OK ==="
END SUB
