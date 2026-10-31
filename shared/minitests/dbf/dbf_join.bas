' MiniTest DBF — JOIN WITH qualified condition (4 backend).
' Self-contained: crea ordini (3 rec) e clienti (2 rec).
' JOIN WITH condizione QUALIFICATA O.IDCLI = C.ID → 3 match.
' EXPECT: 3
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
    DBF CREATE "/tmp/gmb_mt_j.dbf" FROM TYPE TORD
    DBF APPEND RECORD {ID: "O1", IDCLI: "C1"}
    DBF APPEND RECORD {ID: "O2", IDCLI: "C2"}
    DBF APPEND RECORD {ID: "O3", IDCLI: "C1"}
    DBF CLOSE

    DBF CREATE "/tmp/gmb_mt_c.dbf" FROM TYPE TCLI
    DBF APPEND RECORD {ID: "C1", NOME: "ANNA"}
    DBF APPEND RECORD {ID: "C2", NOME: "BOB"}
    DBF CLOSE

    DBF USE "/tmp/gmb_mt_c.dbf" ALIAS C
    DBF USE "/tmp/gmb_mt_j.dbf" ALIAS O
    DBF JOIN WITH C TO "/tmp/gmb_mt_out.dbf" FOR O.IDCLI = C.ID
    DBF USE "/tmp/gmb_mt_out.dbf" ALIAS J
    PRINT DBF.LASTREC()
END SUB
