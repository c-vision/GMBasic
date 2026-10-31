' OperatorsDoEventsDemo — sample completo: TUTTI gli operatori (logici keyword e
' bitwise simboli), short-circuit, DOEVENTS in un loop grafico, FUNCTION Main con
' exit code.
DIM hits AS INTEGER

FUNCTION Bump() AS INTEGER
  hits = hits + 1
  RETURN 1
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM i AS INTEGER
  DIM a AS INTEGER
  DIM b AS INTEGER
  DIM drawn AS INTEGER
  a = 5 : b = 3 : drawn = 0

  SCREEN 12, 320, 200, 32
  FOR i = 1 TO 16
    IF (i > 3 AND i < 14) OR (i = 16) THEN
      IF (i && 1) = 0 THEN
        LINE (i * 10, 60)-(i * 10, 140), 10
        drawn = drawn + 1
      END IF
    END IF
    IF (i XOR 1) <> 0 THEN PSET (i * 10, 40), 7
    IF (NOT (i = 9)) AND ((i ^^ 2) <> 0) THEN PSET (i * 10, 150), 12
    DOEVENTS
  NEXT i
  SCREEN 0

  hits = 0
  DIM x AS INTEGER
  x = (0 AND Bump())
  DIM y AS INTEGER
  y = (1 OR Bump())
  DIM z AS INTEGER
  z = (1 AND Bump())

  PRINT "logical="; (a AND b); ","; (a OR b); ","; (a XOR b); ","; (NOT a)
  PRINT "bitwise="; (a && b); ","; (a || b); ","; (a ^^ b); ","; (! a)
  PRINT "shortcircuit-hits="; hits
  PRINT "drawn="; drawn
  PRINT "OperatorsDoEventsDemo ok"
  RETURN 0
END FUNCTION
