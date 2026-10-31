' EXPECT: n= 6|p3= 4|p5= 0
SUB Main()
 DIM data(3)
 data(0)=1:data(1)=2:data(2)=3:data(3)=4
 DIM p = ARRAYS.APAD(data, 6, 0.0)
 PRINT "n="; UBOUND(p)+1
 PRINT "p3="; p(3)
 PRINT "p5="; p(5)
END SUB
