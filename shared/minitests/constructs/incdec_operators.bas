' MiniTest: operatori `++`/`--` (prefix/postfix) come STATEMENT e come ESPRESSIONE, su:
' variabile locale, campo di CLASSE (nudo e SELF), elemento di array 1-D e 2-D, INTEGER e
' DOUBLE. Semantica: prefix = NUOVO valore, postfix = VECCHIO valore.
' EXPECT: a= 6|b= 7|c= 7  i= 8|d= 9  i= 9|pre= 6  x= 6|post= 6  x= 7|bump= 8  d= 0.5|e= 11  m= 6|f= 11  a= 12|param= 6
SUB BumpParam(d AS INTEGER)
  d++                      ' parametro BYVAL
  PRINT "param="; d        ' 6
END SUB
CLASS C
  PUBLIC DIM x AS INTEGER
  PUBLIC DIM d AS DOUBLE
  PUBLIC SUB NEW()
    x = 5
    d = 1.5
  END SUB
  FUNCTION Pre() AS INTEGER
    Pre = ++x              ' prefix: x diventa 6, ritorna 6
  END FUNCTION
  FUNCTION Post() AS INTEGER
    Post = x++             ' postfix: ritorna 6, x diventa 7
  END FUNCTION
  SUB Bump()
    x++                    ' campo nudo
    SELF.d--               ' campo qualificato, DOUBLE
  END SUB
END CLASS
SUB Main()
  DIM i AS INTEGER
  i = 5
  i++                      ' postfix statement
  PRINT "a="; i            ' 6
  ++i                      ' prefix statement
  PRINT "b="; i            ' 7
  DIM x AS INTEGER
  x = i++                  ' postfix espressione: x=7, i=8
  PRINT "c="; x; " i="; i
  x = ++i                  ' prefix espressione: i=9, x=9
  PRINT "d="; x; " i="; i

  DIM c AS C = NEW C()
  PRINT "pre="; c.Pre(); " x="; c.x      ' 6 / 6
  PRINT "post="; c.Post(); " x="; c.x    ' 6 / 7
  c.Bump()
  PRINT "bump="; c.x; " d="; c.d         ' 8 / 0.5

  DIM a(3) AS INTEGER
  DIM m(2, 2) AS INTEGER
  a(1) = 10
  m(0, 0) = 7
  a(1)++                   ' elemento 1-D (statement)
  m(0, 0)--                ' elemento 2-D (statement)
  PRINT "e="; a(1); " m="; m(0, 0)       ' 11 / 6
  DIM y AS INTEGER
  y = a(1)++               ' elemento 1-D (espressione): y=11, a(1)=12
  PRINT "f="; y; " a="; a(1)

  BumpParam(5)             ' parametro BYVAL di una SUB
END SUB
