' CLIPPER 5.3 — EVAL(bBlock [, args...]) su un code block
' EXPECT: e1= 6|e2= 10|e3= 42|e4= 3.14|e5=ABC|e6=X!|e7=x!|e8= 10|e9=x!|e10= 3.14|e11= 15|e12=X|e13=y|e14= 6|e15= 6|e16= 6|e17= 6
FUNCTION MkInc() AS FUNCTION(x) AS DOUBLE
  RETURN { |x| RETURN x + 1 }
END FUNCTION
SUB Main()
  DIM b1 = { |x| RETURN x + 1 }
  PRINT "e1="; EVAL(b1, 5)       ' 6
  PRINT "e2="; b1(9)             ' 10
  DIM b2 = { |a, b| RETURN a * b }
  PRINT "e3="; EVAL(b2, 6, 7)    ' 42
  DIM b3 = { || 3.14 }
  PRINT "e4="; EVAL(b3)          ' 3.14
  DIM b4 = &("{ |s| RETURN UPPER(s) }")   ' macro-compiled block
  PRINT "e5="; EVAL(b4, "abc")   ' ABC
  DIM b5 = { |s| RETURN UPPER(s) + "!" } ' concat: risultato stringa
  PRINT "e6="; EVAL(b5, "x")     ' X!
  DIM b6 = { |s| RETURN s + "!" }        ' param usato SOLO in concat
  PRINT "e7="; EVAL(b6, "x")     ' x!
  DIM b7 = { |a, b, c| RETURN a * b + c } ' arità 3
  PRINT "e8="; EVAL(b7, 2, 3, 4) ' 10
  PRINT "e9="; EVAL({ |s| RETURN s + "!" }, "x") ' x! (block INLINE in EVAL)
  PRINT "e10="; EVAL({ || 3.14 })                ' 3.14 (inline 0-arg)
  DIM k = 10
  PRINT "e11="; EVAL({ |x| RETURN x + k }, 5)    ' 15 (inline con cattura)
  DIM rs AS STRING = EVAL({ |s| RETURN UPPER(s) }, "x")
  PRINT "e12="; rs                               ' X (assegnazione stringa)
  EVAL({ |s| PRINT "e13="; s }, "y")             ' e13=y (statement inline)
  PRINT "e14="; EVAL(MkInc(), 5)                 ' 6 (target = chiamata che ritorna block)
  DIM bm = MkInc()
  PRINT "e15="; EVAL(bm, 5)                      ' 6 (firma inferita dalla FUNCTION)
  PRINT "e16="; EVAL({ |x| RETURN EVAL({ |y| RETURN y + 1 }, x) }, 5) ' 6 (EVAL annidato)
  DIM inner = { |y| RETURN y + 1 }
  DIM outer = { |x| RETURN EVAL(inner, x) }
  PRINT "e17="; EVAL(outer, 5)                   ' 6 (function-value catturato in lambda)
END SUB
