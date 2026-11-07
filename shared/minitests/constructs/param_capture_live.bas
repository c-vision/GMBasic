' MiniTest — cattura di un PARAMETRO di procedura in una lambda: per RIFERIMENTO.
' La closure cattura la VARIABILE, non il valore: una riassegnazione del parametro dopo
' la creazione della lambda si vede (parità IL/PCode/VM-C/C99Native). Un parametro BYREF
' inoltrato a un'altra SUB continua a scrivere lo slot del chiamante.
' EXPECT: r1= 16|orig= 5|r2=ciao!|s=99|boxed= 3
SUB Bump(ByRef x AS INTEGER)
  x = x + 1
END SUB

FUNCTION Mk(p AS INTEGER) AS INTEGER
  DIM f = { || RETURN p }
  Bump(p)
  p = p + 10
  RETURN EVAL(f)
END FUNCTION

FUNCTION Msg(s AS STRING) AS STRING
  DIM f = { || RETURN s }
  s = s + "!"
  RETURN EVAL(f)
END FUNCTION

FUNCTION MkStr(t AS STRING) AS STRING
  DIM f = { || t = t + "9" : RETURN t }
  t = "9"
  RETURN EVAL(f)
END FUNCTION

SUB Main()
  DIM p AS INTEGER
  p = 5
  PRINT "r1="; Mk(p)          ' 16 (Bump porta p a 6, poi +10; la closure legge il box)
  PRINT "orig="; p            ' 5  (il parametro è BYVAL: il chiamante non cambia)
  PRINT "r2="; Msg("ciao")    ' ciao!
  PRINT "s="; MkStr("x")      ' 99 (mutazione dal corpo della closure, box condiviso)
  DIM q AS INTEGER
  q = 2
  DIM g = { || q = q + 1 : RETURN q }
  PRINT "boxed="; EVAL(g)     ' 3
END SUB
