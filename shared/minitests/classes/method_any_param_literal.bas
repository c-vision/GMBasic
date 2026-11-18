' Metodo di classe con parametro ANY: letterale numerico/stringa come argomento (IL: `ldc.r8` nudo -> InvalidProgram) e local INTEGER.
' EXPECT: tag= 5  i= 2|tag=s i= 2|OK
CLASS Tagger
  SUB Show(tag AS ANY)
    DIM i AS INTEGER
    i = 2
    PRINT "tag="; tag; " i="; i
  END SUB
END CLASS

SUB Main()
  DIM t AS Tagger = NEW Tagger()
  t.Show(5)
  t.Show("s")
  PRINT "OK"
END SUB
