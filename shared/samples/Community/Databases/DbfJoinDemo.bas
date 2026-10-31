' Sample DBF — SELECT + JOIN WITH: commutazione area e nested-loop join su 4 backend.
' Crea ordini (3 rec) e clienti (2 rec). Usa DBF SELECT per rendere current=ordini,
' poi DBF JOIN WITH con condizione QUALIFICATA O.IDCLI = C.ID (3 match).
' Usa ogni comando/funzione: CREATE, APPEND RECORD, CLOSE, USE, SELECT, JOIN WITH, LASTREC.
' Parity4 PASS su 4 backend.
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
    DBF CREATE "/tmp/gmb_sj.dbf" FROM TYPE TORD
    DBF APPEND RECORD {ID: "O1", IDCLI: "C1"}
    DBF APPEND RECORD {ID: "O2", IDCLI: "C2"}
    DBF APPEND RECORD {ID: "O3", IDCLI: "C1"}
    DBF CLOSE

    DBF CREATE "/tmp/gmb_sc.dbf" FROM TYPE TCLI
    DBF APPEND RECORD {ID: "C1", NOME: "ANNA"}
    DBF APPEND RECORD {ID: "C2", NOME: "BOB"}
    DBF CLOSE

    DBF USE "/tmp/gmb_sc.dbf" ALIAS C
    DBF USE "/tmp/gmb_sj.dbf" ALIAS O
    DBF SELECT O
    DBF JOIN WITH C TO "/tmp/gmb_so.dbf" FOR O.IDCLI = C.ID
    DBF USE "/tmp/gmb_so.dbf" ALIAS J
    PRINT DBF.LASTREC()
END SUB
