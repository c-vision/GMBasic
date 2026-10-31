' EXPECT: s=abcdef
SUB Main()
  DIM S$(2, 1)
  S$ = {{"a", "b"}, {"c", "d"}, {"e", "f"}}
  PRINT "s="; S$(0,0); S$(0,1); S$(1,0); S$(1,1); S$(2,0); S$(2,1)
END SUB
