' MiniTest DBF — SELECT commuta l'area attiva prima di JOIN.
' A(2 rec), B(1 rec). SELECT A → JOIN A×B con FOR TA.K=TA.K (sempre true).
' Senza SELECT A il JOIN userebbe B come primary (1×2=2 uguale, ma con altri dati
' farebbe differenza). Con SELECT A: primary=A che ha 2 rec → 2×1=2 output.
' EXPECT: 2
IMPORT DBF

TYPE TA
    K AS STRING * 3
END TYPE
TYPE TB
    K AS STRING * 3
END TYPE

SUB Main()
    DBF CREATE "/tmp/gmt_sw_a.dbf" FROM TYPE TA
    DBF APPEND RECORD {K: "A1"}
    DBF APPEND RECORD {K: "A2"}
    DBF CLOSE

    DBF CREATE "/tmp/gmt_sw_b.dbf" FROM TYPE TB
    DBF APPEND RECORD {K: "B1"}
    DBF CLOSE

    DBF USE "/tmp/gmt_sw_a.dbf" ALIAS A
    DBF USE "/tmp/gmt_sw_b.dbf" ALIAS B
    DBF SELECT A
    DBF JOIN WITH B TO "/tmp/gmt_sw_out.dbf" FOR A.K = A.K
    DBF USE "/tmp/gmt_sw_out.dbf" ALIAS J
    PRINT DBF.LASTREC()
END SUB
