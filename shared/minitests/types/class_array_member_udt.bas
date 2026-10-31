' MiniTest: array-MEMBRO di CLASSE di UDT — accesso al CAMPO dell'elemento (`arr(i).campo`
' e `SELF.arr(i).campo`), scrittura + lettura. Copre il bug "array-membro di UDT dentro una
' classe": prima falliva su tutti i backend (IL IndexedTarget #error FieldAccessExpr,
' C99Native parser, VM-C elemento senza __class__/copia).
' EXPECT: bare= 60|self= 60
TYPE R
  campo AS INTEGER
  altro AS INTEGER
END TYPE
CLASS C
  PUBLIC DIM arr(3) AS R
  PUBLIC SUB NEW()
  END SUB
  SUB Fill()
    arr(0).campo = 10
    arr(1).campo = 20
    SELF.arr(2).campo = 30
  END SUB
  FUNCTION SumBare() AS INTEGER
    SumBare = arr(0).campo + arr(1).campo + arr(2).campo
  END FUNCTION
  FUNCTION SumSelf() AS INTEGER
    SumSelf = SELF.arr(0).campo + SELF.arr(1).campo + SELF.arr(2).campo
  END FUNCTION
END CLASS
SUB Main()
  DIM c AS C = NEW C()
  c.Fill()
  PRINT "bare="; c.SumBare()
  PRINT "self="; c.SumSelf()
END SUB
