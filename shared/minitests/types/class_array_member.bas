' MiniTest: indicizzazione di un array-MEMBRO di CLASSE — bare `arr(i)` e `SELF.arr(i)`,
' scrittura e lettura. Copre il bug (open) Class-Array-Member-Indexing-Rotto: prima
' falliva su tutti i backend (IL "array non DIMensionato"/NRE, PCode/VM-C vuoto,
' C99BE exit 139 / C99Native driver fallito).
' EXPECT: bare= 10|self= 20|sum= 60
CLASS Cam
  PUBLIC DIM arr(3) AS INTEGER
  PUBLIC SUB NEW()
  END SUB
  SUB Fill()
    arr(0) = 10
    SELF.arr(1) = 20
    arr(2) = 30
  END SUB
  SUB Show()
    PRINT "bare="; arr(0)
    PRINT "self="; SELF.arr(1)
  END SUB
  FUNCTION Sum() AS INTEGER
    Sum = arr(0) + SELF.arr(1) + arr(2)
  END FUNCTION
END CLASS
SUB Main()
  DIM c AS Cam = NEW Cam()
  c.Fill()
  c.Show()
  PRINT "sum="; c.Sum()
END SUB
