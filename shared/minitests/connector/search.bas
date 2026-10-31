' EXPECT: qgmail= 673|qmail= 1456|qnone=[]
IMPORT AI
SUB Main()
  PRINT "qgmail="; LEN(AI.CONNECTORSEARCH("gmail"))
  PRINT "qmail="; LEN(AI.CONNECTORSEARCH("mail"))
  PRINT "qnone="; AI.CONNECTORSEARCH("zzz_nomatch")
END SUB
