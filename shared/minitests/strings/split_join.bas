' EXPECT: n= 3|a,b,c
DIM parts() AS STRING
parts = SPLIT("a,b,c", ",")
PRINT "n="; UBOUND(parts) + 1
PRINT JOIN(parts(), ",")
