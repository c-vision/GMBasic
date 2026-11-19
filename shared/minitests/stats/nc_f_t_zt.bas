' EXPECT: fpdf= 0.359694|fcdf= 0.347511|fsf= 0.652489|tpdf= 0.318481|tcdf= 0.300408|tsf= 0.699592|ztpmf= 0.232879|ztcdf= 0.735897
IMPORT STATS
SUB Main()
  PRINT "fpdf="; MATH.ROUND(STATS.NcFPdf(1, 3, 10, 2), 6)
  PRINT "fcdf="; MATH.ROUND(STATS.NcFCdf(1, 3, 10, 2), 6)
  PRINT "fsf="; MATH.ROUND(STATS.NcFSf(1, 3, 10, 2), 6)
  PRINT "tpdf="; MATH.ROUND(STATS.NcTPdf(1.5, 5, 2), 6)
  PRINT "tcdf="; MATH.ROUND(STATS.NcTCdf(1.5, 5, 2), 6)
  PRINT "tsf="; MATH.ROUND(STATS.NcTSf(1.5, 5, 2), 6)
  PRINT "ztpmf="; MATH.ROUND(STATS.ZTPoissonPmf(3, 2.5), 6)
  PRINT "ztcdf="; MATH.ROUND(STATS.ZTPoissonCdf(3, 2.5), 6)
END SUB
