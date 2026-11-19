' EXPECT: exp= 0.135335|stud= 0.025139|chi= 0.05
IMPORT STATS
SUB Main()
  PRINT "exp="; MATH.ROUND(STATS.ExponentialSf(2, 1.0), 6)
  PRINT "stud="; MATH.ROUND(STATS.StudentTSf(1.959964, 1000), 6)
  PRINT "chi="; MATH.ROUND(STATS.ChiSquaredSf(3.841459, 1), 6)
END SUB
