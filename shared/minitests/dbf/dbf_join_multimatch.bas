' MiniTest DBF — JOIN WITH multi-match (fan-out: 1 ordine → 2 output righe).
' Crea 1 ordine (IDCLI=C1) e 2 clienti con ID=C1 → 2 match.
' EXPECT: 2
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
    DBF CREATE "/tmp/gmb_mjm_o.dbf" FROM TYPE TORD
    DBF APPEND RECORD {ID: "O1", IDCLI: "C1"}
    DBF CLOSE

    DBF CREATE "/tmp/gmb_mjm_c.dbf" FROM TYPE TCLI
    DBF APPEND RECORD {ID: "C1", NOME: "ANNA"}
    DBF APPEND RECORD {ID: "C1", NOME: "BETTA"}
    DBF CLOSE

    DBF USE "/tmp/gmb_mjm_c.dbf" ALIAS C
    DBF USE "/tmp/gmb_mjm_o.dbf" ALIAS O
    DBF JOIN WITH C TO "/tmp/gmb_mjm_out.dbf" FOR O.IDCLI = C.ID
    DBF USE "/tmp/gmb_mjm_out.dbf" ALIAS J
    PRINT DBF.LASTREC()
END SUB
