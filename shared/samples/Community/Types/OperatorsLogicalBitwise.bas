' Operatori logici (keyword) e bitwise (simboli).
' Logici: NOT/AND/OR/XOR → -1 vero / 0 falso, AND/OR short-circuit.
' Bitwise: ! (not), && (and), || (or), ^^ (xor).
FUNCTION Main() AS INTEGER
  PRINT "Logici:"
  PRINT "  5 AND 3  = "; (5 AND 3)
  PRINT "  5 OR 2   = "; (5 OR 2)
  PRINT "  5 XOR 3  = "; (5 XOR 3)
  PRINT "  NOT 5    = "; (NOT 5)
  PRINT "Bitwise:"
  PRINT "  5 && 3   = "; (5 && 3)
  PRINT "  5 || 2   = "; (5 || 2)
  PRINT "  5 ^^ 3   = "; (5 ^^ 3)
  PRINT "  !1       = "; (! 1)
  RETURN 0
END FUNCTION
