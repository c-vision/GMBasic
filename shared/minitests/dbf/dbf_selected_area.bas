' MiniTest DBF — DBF SELECT è un COMANDO (cambia area), DBF.SELECTED() è una FUNZIONE
' (numero d'area 1-based). Copre: nessun argomento, alias esplicito, alias inesistente,
' SELECT numerico, CLOSE per alias di un'area NON corrente, CLOSE per numero e CLOSE
' dell'area corrente.
' EXPECT: b= 2|a= 1|alias-b= 2|unknown= 0|unknown-select= 1|alias-unknown=A|sel-num= 2|sel-lit= 1|sel-var-ident= 0|sel-missing= 0|close1= 2|close-current= 0
IMPORT DBF

TYPE TArea
    K AS STRING * 3
END TYPE

SUB Main()
    DIM n AS INTEGER
    DBF CREATE "/tmp/gmb_mt_sel_a.dbf" FROM TYPE TArea
    DBF CLOSE
    DBF CREATE "/tmp/gmb_mt_sel_b.dbf" FROM TYPE TArea
    DBF CLOSE

    DBF USE "/tmp/gmb_mt_sel_a.dbf" ALIAS A
    DBF USE "/tmp/gmb_mt_sel_b.dbf" ALIAS B

    PRINT "b="; DBF.SELECTED()
    DBF SELECT A
    PRINT "a="; DBF.SELECTED()
    PRINT "alias-b="; DBF.SELECTED("B")
    PRINT "unknown="; DBF.SELECTED("ZZZ")
    DBF SELECT ZZZ
    PRINT "unknown-select="; DBF.SELECTED()
    PRINT "alias-unknown="; DBF.ALIAS()
    DBF SELECT 2
    PRINT "sel-num="; DBF.SELECTED()
    PRINT "sel-lit="; DBF.SELECTED(1)
    n = 2
    PRINT "sel-var-ident="; DBF.SELECTED(n)
    PRINT "sel-missing="; DBF.SELECTED(9)
    DBF CLOSE 1
    PRINT "close1="; DBF.SELECTED()
    DBF CLOSE B
    PRINT "close-current="; DBF.SELECTED()
END SUB
