' EXPECT: pred= 3  2  1|3way= 3  2  1
SUB Main()
  DIM a = {3, 1, 2}
  ASORT a, { |x, y| RETURN x > y }
  PRINT "pred="; a(0); a(1); a(2)
  DIM b = {3, 1, 2}
  ASORT b, { |x, y| RETURN y - x }
  PRINT "3way="; b(0); b(1); b(2)
END SUB
