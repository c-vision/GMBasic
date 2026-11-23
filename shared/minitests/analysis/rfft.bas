' MiniTest ANALYSIS.RFFT via FFI (namespace SIGNAL del pacchetto `signal`).
' Componenti non negative interleaved [re0, im0, …]: per x[i]=cos(2πi/8) (N=8)
' sono 2*(8\2+1)=10 valori, con X1 = 4 (re all'indice 2 dell'array interleaved).
' EXPECT: rfft= 10  |  4
IMPORT SIGNAL
SUB Main()
  DIM x(7) AS DOUBLE
  FOR i = 0 TO 7: x(i) = COS(2 * 3.141592653589793 * i / 8): NEXT
  DIM rf() = SIGNAL.RFFT(x)
  PRINT "rfft="; LEN(rf); " | "; MATH.ROUND(rf(2), 6)
END SUB
