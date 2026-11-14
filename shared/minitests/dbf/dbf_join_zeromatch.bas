' MiniTest DBF — JOIN WITH zero match (condizione falsa).
' Crea ordini (3 rec) e clienti (1 rec con ID=XX che non matcha O.IDCLI).
' JOIN WITH produce 0 record.
' EXPECT: 0
IMPORT DBF

TYPE TORD
    ID AS STRING * 5
    IDCLI AS STRING * 5
END TYPE
TYPE TCLI
    ID AS STRING * 5
    NOME AS STRING * 10
END TYPE

SUB Main()
    DBF CREATE "/tmp/gmb_mjz_o.dbf" FROM TYPE TORD
    DBF APPEND RECORD {ID: "O1", IDCLI: "C1"}
    DBF APPEND RECORD {ID: "O2", IDCLI: "C2"}
    DBF APPEND RECORD {ID: "O3", IDCLI: "C1"}
    DBF CLOSE

    DBF CREATE "/tmp/gmb_mjz_c.dbf" FROM TYPE TCLI
    DBF APPEND RECORD {ID: "XX", NOME: "NOONE"}
    DBF CLOSE

    DBF USE "/tmp/gmb_mjz_c.dbf" ALIAS C
    DBF USE "/tmp/gmb_mjz_o.dbf" ALIAS O
    DBF JOIN WITH C TO "/tmp/gmb_mjz_out.dbf" FOR O.IDCLI = C.ID
    DBF USE "/tmp/gmb_mjz_out.dbf" ALIAS J
    PRINT DBF.LASTREC()
END SUB
