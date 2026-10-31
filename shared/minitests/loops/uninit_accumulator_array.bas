' MiniTest: accumulatore DIM senza inizializzatore esplicito, in presenza di un array +
' doppio FOR. Bug: il backend IL (pass ILOptim SsaPromotion) leggeva il valore iniziale da
' uno slot locale RIUSATO (il contatore del FOR precedente) -> 39 invece di 36; PCode/VM-C
' corretti. Il fix emette lo store esplicito del default (0) per gli scalari numerici DIM.
' EXPECT: s= 36
SUB Main()
  DIM m(8) AS INTEGER
  DIM i AS INTEGER, j AS INTEGER, s AS INTEGER
  FOR i = 0 TO 2
    FOR j = 0 TO 2
      m(i * 3 + j) = i * 3 + j
    NEXT j
  NEXT i
  FOR i = 0 TO 2
    FOR j = 0 TO 2
      s = s + m(i * 3 + j)
    NEXT j
  NEXT i
  PRINT "s="; s
END SUB
