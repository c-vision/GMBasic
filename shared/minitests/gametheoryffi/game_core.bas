' OPTIMIZE (FFI gametheory) — saddle point, valore misto, minimax, Nash, Shapley.
' EXPECT: saddle= 1 :2.0: 0 : 0 |value=2.0|minimax= 1 :2.0|nash= 0 :0.50|shapley= 0 :0.333
IMPORT OPTIMIZE

FUNCTION Coalition(mask AS DOUBLE) AS DOUBLE
  IF mask > 0 THEN RETURN 1
  RETURN 0
END FUNCTION

SUB Main()
  ' payoff 2x2 con saddle in (0,0): valore 2
  DIM A(3) AS DOUBLE
  A(0)=2 : A(1)=2
  A(2)=1 : A(3)=3
  DIM out(2) AS DOUBLE
  PRINT "saddle="; OPTIMIZE.GAMESADDLE(A, 2, 2, out); ":"; FORMAT(out(0), "#.0"); ":"; out(1); ":"; out(2);

  DIM rs(1) AS DOUBLE
  DIM cs(1) AS DOUBLE
  PRINT "|value="; FORMAT(OPTIMIZE.GAMEVALUE(A, 2, 2, rs, cs), "#.0");

  DIM mm(4) AS DOUBLE
  PRINT "|minimax="; OPTIMIZE.GAMEMINIMAX(A, 2, 2, mm); ":"; FORMAT(mm(0), "#.0");

  ' matching pennies: Nash misto (1/2,1/2)
  DIM AP(3) AS DOUBLE
  AP(0)=1 : AP(1)=-1
  AP(2)=-1 : AP(3)=1
  DIM BP(3) AS DOUBLE
  BP(0)=-1 : BP(1)=1
  BP(2)=1 : BP(3)=-1
  DIM nash(3) AS DOUBLE
  PRINT "|nash="; OPTIMIZE.NASH2P(AP, BP, 2, 2, nash); ":"; FORMAT(nash(0), "#.00");

  DIM phi(2) AS DOUBLE
  PRINT "|shapley="; OPTIMIZE.SHAPLEY(Coalition, 3, phi); ":"; FORMAT(phi(0), "#.000")
  PRINT ""
END SUB
