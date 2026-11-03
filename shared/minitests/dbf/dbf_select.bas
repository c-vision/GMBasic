' MiniTest DBF — DBF SELECT è un COMANDO che commuta l'area attiva.
' A(2 record) e B(1 record): dopo SELECT A LASTREC()=2, dopo SELECT B =1.
' Non dipende da JOIN (l'engine di JOIN è coperto da dbf_join*.bas): isola il comando
' SELECT, così il test resta valido anche quando il valutatore di espressioni C è in
' manutenzione.
' EXPECT: a= 2|alias-a=A|b= 1|alias-b=B
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
    PRINT "a="; DBF.LASTREC()
    PRINT "alias-a="; DBF.ALIAS()
    DBF SELECT B
    PRINT "b="; DBF.LASTREC()
    PRINT "alias-b="; DBF.ALIAS()
END SUB
