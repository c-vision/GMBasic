' FILE: Databases/DbVmMarshallerDbf.bas
' =====================================================================
' Test del marshaller DBF sul backend VM/C: CREATE, USE, APPEND BLANK,
' REPLACE (coppie value/fname), GO, GET, INDEX ON + TOTAL ON.
' Output: work/.
' =====================================================================
IMPORT DBF
IMPORT IO

TYPE Row
  ID AS INTEGER
  Name AS STRING * 20
  Amount AS DOUBLE
END TYPE

SUB Main()
    DIM g_work AS STRING
    g_work = PROJECTFOLDER() + "/work"
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    ON ERROR RESUME NEXT
    KILL g_work + "/vm_dbf.dbf"
    KILL g_work + "/vm_dbf_out.dbf"
    KILL g_work + "/vm_dbf.idx"
    ON ERROR GOTO 0

    DBF CREATE g_work + "/vm_dbf.dbf" FROM TYPE Row
    DBF USE g_work + "/vm_dbf.dbf" ALIAS "F" AS Row

    DBF APPEND BLANK
    DBF REPLACE ID WITH 1, Name WITH "Alice", Amount WITH 100
    DBF APPEND BLANK
    DBF REPLACE ID WITH 2, Name WITH "Bob", Amount WITH 50
    DBF APPEND BLANK
    DBF REPLACE ID WITH 3, Name WITH "Alice", Amount WITH 200
    DBF GO TOP

    DBF INDEX ON Name TO g_work + "/vm_dbf.idx"
    DBF TOTAL ON Name TO g_work + "/vm_dbf_out.dbf" FIELDS Amount
    DBF CLOSE

    PRINT "VM-DBF-MARSHALLER-OK"
END SUB