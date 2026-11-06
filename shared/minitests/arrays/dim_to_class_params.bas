' Lower bound propagate ai parametri array di METODI e COSTRUTTORI di classe:
' chiamata diretta/CALL, costruttore (anche BASE.NEW), override virtuale e metodo
' su un elemento di array di istanze.
' EXPECT: ctor= 1 , 3 , 10|fields= 3 , 10|show= 1 , 3 , 30|show= 1 , 3 , 30|ctor= 1 , 3 , 10|over= 1 , 3 , 20|ctor= 1 , 3 , 10|show= 1 , 3 , 30
CLASS Viewer
  DIM n AS LONG
  DIM first AS DOUBLE

  PUBLIC SUB NEW(a() AS DOUBLE)
    n = UBOUND(a,1)
    first = a(1)
    PRINT "ctor="; LBOUND(a,1); ","; n; ","; first
  END SUB

  PUBLIC VIRTUAL SUB Show(a() AS DOUBLE)
    PRINT "show="; LBOUND(a,1); ","; UBOUND(a,1); ","; a(3)
  END SUB
END CLASS

CLASS Special INHERITS Viewer
  PUBLIC SUB NEW(a() AS DOUBLE)
    BASE.NEW(a)
  END SUB

  PUBLIC OVERRIDE SUB Show(a() AS DOUBLE)
    PRINT "over="; LBOUND(a,1); ","; UBOUND(a,1); ","; a(2)
  END SUB
END CLASS

SUB Main()
  DIM a(1 TO 3) AS DOUBLE
  a(1) = 10 : a(2) = 20 : a(3) = 30

  DIM v AS Viewer = NEW Viewer(a)
  PRINT "fields="; v.n; ","; v.first
  v.Show(a)
  CALL v.Show(a)

  DIM s AS Viewer = NEW Special(a)
  s.Show(a)

  DIM instances(0) AS Viewer
  instances(0) = NEW Viewer(a)
  instances(0).Show(a)
END SUB
