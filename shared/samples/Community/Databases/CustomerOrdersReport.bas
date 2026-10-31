' FILE: samples/database/CustomerOrdersReport.bas
' ===================================================================
' REPORT CLIENTI/ORDINI — join annidato tra due CSV (driver testo):
'   1. genera customers.csv e orders.csv in work
'   2. apre due aree RDD (clienti area 1, ordini area 2)
'   3. per ogni cliente naviga i suoi ordini (DBF SELECT) e aggrega
'   4. scrive un report CSV (cliente, n.ordini, totale)
' Percorsi: fixtures in samples/fixtures, output in work.
' ===================================================================
IMPORT RDD
IMPORT DBF
IMPORT IO

SUB Main()
    DIM g_work AS STRING
    g_work = PROJECTFOLDER() + "/work"
    IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    PRINT "=== Customer Orders Report ==="
    PRINT

    ' --- 1. Genera i CSV ---
    PRINT "1. generazione customers.csv / orders.csv"
    OPEN g_work + "/customers.csv" FOR OUTPUT AS #1
    PRINT #1, "cid,nome"
    PRINT #1, "C1,Alice"
    PRINT #1, "C2,Bob"
    PRINT #1, "C3,Carla"
    CLOSE #1
    OPEN g_work + "/orders.csv" FOR OUTPUT AS #2
    PRINT #2, "oid,cid,importo"
    PRINT #2, "100,C1,50.00"
    PRINT #2, "101,C1,30.00"
    PRINT #2, "102,C2,120.00"
    PRINT #2, "103,C1,20.00"
    PRINT #2, "104,C3,80.00"
    CLOSE #2
    PRINT "   3 clienti, 5 ordini"
    PRINT

    ' --- 2. Apri entrambe le aree ---
    PRINT "2. apertura aree"
    RDD USE g_work + "/customers.csv" ALIAS CUST
    RDD USE g_work + "/orders.csv" ALIAS ORD
    PRINT "   clienti: "; DBF.RECNO()
    PRINT

    ' --- 3. Join annidato + aggregazione ---
    PRINT "3. join clienti-ordini (DBF SELECT)"
    DBF SELECT CUST
    DBF GO TOP
    OPEN g_work + "/orders_report.csv" FOR OUTPUT AS #3
    PRINT #3, "cliente,ordini,totale"
    DO WHILE NOT DBF.EOF()
        cid$ = RDD.GETFIELD(1, 1)
        nome$ = RDD.GETFIELD(1, 1)
        DBF SELECT ORD
        DBF GO TOP
        nOrd = 0
        tot = 0
        DO WHILE NOT DBF.EOF()
            IF RDD.GETFIELD(2, 1) = cid$ THEN
                nOrd = nOrd + 1
                tot = tot + VAL(RDD.GETFIELD(2, 2))
            END IF
            DBF SKIP 1
        LOOP
        DBF SELECT CUST
        PRINT #3, nome$; ","; nOrd; ","; tot
        PRINT "   "; nome$; ": "; nOrd; " ordini, totale "; tot
        DBF SKIP 1
    LOOP
    CLOSE #3
    PRINT

    ' --- 4. Copie ordinate ---
    PRINT "4. COPY TO ordinati"
    DBF SELECT ORD
    RDD SET ORDER 1, "importo"
    RDD COPY TO g_work + "/orders_by_amount.csv" VIA "CSV"
    DBF CLOSE
    DBF SELECT CUST
    DBF CLOSE
    PRINT

    PRINT "--- CUSTOMER ORDERS REPORT OK ---"
END SUB