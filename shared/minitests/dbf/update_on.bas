' MiniTest DBF — UPDATE ON <key> FROM <alias> REPLACE … (join-update per chiave).
' Path ASSOLUTI (/tmp): la CWD differisce fra i backend. Il sample è il proprio oracolo.
' EXPECT: r= 1|B1| 110|r= 2|A2| 200|r= 3|B3| 330
IMPORT DBF WITH ALIAS
IMPORT IO

TYPE TUpd
    ID AS INTEGER
    Nome AS STRING * 20
    Saldo AS CURRENCY
END TYPE

SUB Main()
    DIM pa AS STRING
    DIM pb AS STRING
    pa = "/tmp/gmb_mt_update_on_a.dbf"
    pb = "/tmp/gmb_mt_update_on_b.dbf"
    KILL pa
    KILL pb

    ' Destinazione: ID 1,2,3
    DBF CREATE pa FROM TYPE TUpd
    DBF USE pa ALIAS AA
    DBF APPEND BLANK
    DBF REPLACE ID WITH 1, Nome WITH "A1", Saldo WITH 100
    DBF APPEND BLANK
    DBF REPLACE ID WITH 2, Nome WITH "A2", Saldo WITH 200
    DBF APPEND BLANK
    DBF REPLACE ID WITH 3, Nome WITH "A3", Saldo WITH 300

    ' Sorgente: ID 1,3 (2 non esiste nel sorgente → resta invariato)
    DBF CREATE pb FROM TYPE TUpd
    DBF USE pb ALIAS BB
    DBF APPEND BLANK
    DBF REPLACE ID WITH 1, Nome WITH "B1", Saldo WITH 10
    DBF APPEND BLANK
    DBF REPLACE ID WITH 3, Nome WITH "B3", Saldo WITH 30

    DBF SELECT AA
    DBF UPDATE ON ID FROM BB REPLACE Nome WITH BB.Nome, Saldo WITH Saldo + BB.Saldo
    DBF GO TOP
    PRINT "r=" + CSTR(ID) + "|" + Nome + "|" + CONVERSIONS.CSTR(Saldo)
    DBF SKIP
    PRINT "r=" + CSTR(ID) + "|" + Nome + "|" + CONVERSIONS.CSTR(Saldo)
    DBF SKIP
    PRINT "r=" + CSTR(ID) + "|" + Nome + "|" + CONVERSIONS.CSTR(Saldo)
    DBF CLOSE
    DBF SELECT BB
    DBF CLOSE
    KILL pa
    KILL pb
END SUB
