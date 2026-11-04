' OPTIMIZE (FFI localsearch) — solve con callback, costruttive nearest/cheapest.
' EXPECT: ls= 1 :0.0:14.0|near= 1 : 0 : 3 |cheap= 1 : 0 : 1 
IMPORT OPTIMIZE

' massimizza Σ i·state(i) su una permutazione
FUNCTION Score(state() AS DOUBLE) AS DOUBLE
  s = 0
  FOR i = 0 TO UBOUND(state)
    s = s + i * state(i)
  NEXT i
  RETURN s
END FUNCTION

SUB Main()
  DIM init(3) AS LONG
  init(0)=3 : init(1)=2 : init(2)=1 : init(3)=0
  DIM out(2) AS DOUBLE
  r = OPTIMIZE.LSSOLVECFG(Score, init, 4, 0, 200, 3, 0, out)
  PRINT "ls="; r; ":"; FORMAT(out(0), "#.0"); ":"; FORMAT(out(1), "#.0");

  ' matrice distanza del quadrato
  DIM d(15) AS DOUBLE
  DIM px(3) AS DOUBLE
  DIM py(3) AS DOUBLE
  px(0)=0 : px(1)=1 : px(2)=1 : px(3)=0
  py(0)=0 : py(1)=0 : py(2)=1 : py(3)=1
  FOR i = 0 TO 3
    FOR j = 0 TO 3
      d(i*4+j) = SQR((px(i)-px(j))^2 + (py(i)-py(j))^2)
    NEXT j
  NEXT i
  DIM perm(3) AS LONG
  PRINT "|near="; OPTIMIZE.LSNEAREST(d, 4, 0, perm); ":"; perm(0); ":"; perm(3);
  PRINT "|cheap="; OPTIMIZE.LSCHEAPEST(d, 4, perm); ":"; perm(0); ":"; perm(3);
  PRINT ""
END SUB
