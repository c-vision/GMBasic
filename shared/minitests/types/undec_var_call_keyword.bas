' MiniTest: variabile NON dichiarata assegnata a una chiamata il cui nome coincide con una
' keyword riservata (ASK -> AiVectorAsk): deve vincere la USER-FUNCTION (tipo di ritorno).
' EXPECT: p= 1.5
FUNCTION ASK() AS DOUBLE
  ASK = 1.5
END FUNCTION
SUB Main()
  request_price = ASK()
  PRINT "p="; request_price
END SUB
