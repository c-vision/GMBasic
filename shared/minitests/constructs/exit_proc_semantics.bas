' MiniTest: EXIT SUB / EXIT FUNCTION dentro loop, valore del return-slot,
' default numerico 0, default stringa "" e uscita senza eseguire il resto del corpo.
' EXPECT: s1|loop|f7-ok|f0-ok|s0-ok|s1-ok
SUB S()
  PRINT "s1"
  EXIT SUB
  PRINT "bad-sub"
END SUB

SUB S2()
  DIM i AS INTEGER
  FOR i = 1 TO 3
    IF i = 2 THEN EXIT SUB
    PRINT "loop"
  NEXT i
  PRINT "bad-loop"
END SUB

FUNCTION F2() AS INTEGER
  DIM i AS INTEGER
  FOR i = 1 TO 3
    IF i = 2 THEN
      F2 = 7
      EXIT FUNCTION
    END IF
  NEXT i
  F2 = 8
END FUNCTION

FUNCTION F0() AS INTEGER
  EXIT FUNCTION
  F0 = 9
END FUNCTION

FUNCTION S0() AS STRING
  EXIT FUNCTION
  S0 = "bad"
END FUNCTION

FUNCTION S1() AS STRING
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM r AS INTEGER
  S()
  S2()
  r = F2()
  IF r = 7 THEN PRINT "f7-ok"
  r = F0()
  IF r = 0 THEN PRINT "f0-ok"
  IF LEN(S0()) = 0 THEN PRINT "s0-ok"
  IF LEN(S1()) = 0 THEN PRINT "s1-ok"
  RETURN 0
END FUNCTION
