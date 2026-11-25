' MiniTest: LEN di un NUMERO = numero di "cifre" della rappresentazione G17, senza il punto
' decimale e senza il segno meno (canone IlRuntime.Len: 123→3, 1.25→3, -1.5→2, 100→3).
' Prima: C99 non compilava (`gmb_str_t_len(double)`), VM/C dava 0, IL/PCode le cifre.
' EXPECT: a= 3|b= 3|c= 2|d= 3|s= 3
SUB Main()
  PRINT "a="; LEN(123)
  PRINT "b="; LEN(1.25)
  PRINT "c="; LEN(-1.5)
  PRINT "d="; LEN(100)
  PRINT "s="; LEN("123")
END SUB
