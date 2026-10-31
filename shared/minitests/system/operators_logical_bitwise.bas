' MiniTest: operatori LOGICI (keyword: NOT/AND/OR/XOR, -1/0 e short-circuit)
' e BITWISE (!, &&, ||, ^^). Parità 4 backend.
' EXPECT: -1|-1|0|0|1|7|6|-2|0|0|1
DIM hits AS INTEGER
FUNCTION Bump() AS INTEGER
  hits = hits + 1
  RETURN 1
END FUNCTION
FUNCTION Main() AS INTEGER
  PRINT (5 AND 3)
  PRINT (5 OR 2)
  PRINT (5 XOR 3)
  PRINT (NOT 5)
  PRINT (5 && 3)
  PRINT (5 || 2)
  PRINT (5 ^^ 3)
  PRINT (! 1)
  DIM x AS INTEGER
  hits = 0 : x = (0 AND Bump()) : PRINT hits
  hits = 0 : x = (1 OR Bump()) : PRINT hits
  hits = 0 : x = (1 AND Bump()) : PRINT hits
  RETURN 0
END FUNCTION
