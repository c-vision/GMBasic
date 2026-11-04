' EXPECT: f00= 3|y00= 0
SUB Main()
  DIM x(3) AS DOUBLE = {0, 1, 2, 3}
  DIM f = NUMERICAL.Fft(x)
  PRINT "f00="; MATH.ROUND(f(0,0), 4)
  DIM y = NUMERICAL.Ifft(f)
  PRINT "y00="; MATH.ROUND(y(0,0), 4)
END SUB
