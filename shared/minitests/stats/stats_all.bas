' MiniTest NAMESPACE STATS — tutte le 83 funzioni della superficie.
'
' Copre: descrittive, correlazioni/regressione, test di ipotesi e distribuzioni.
' Derivato dai MiniTest per-funzione (stessi input e oracoli) + chiamate con parametri
' validi per le distribuzioni. Scopo: verificare la STESSA versione del namespace sui
' 4 backend, non un'applicazione.
' EXPECT: n= 5|min= 1|max= 5|mean= 3|var= 2.5|skew= 0|kurt=-1.2|f= 4.8|p= 0.070988|df= 6|chi2= 0.5|p= 0.992123|chi2= 0.793651|p= 0.372998|df= 1|r= 1|p= 0|rho= 1|p= 0|slope= 1.99|intercept= 0.05|r= 0.998652|p= 5.9E-05|se= 0.059722|k2= 3.992116|p= 0.13587|w= 0.986339|p= 0.990059|z= 1.040349|p= 0.298178|z=-1.70581|p= 0.088043|jb= 4.848241|p= 0.088556|d= 0.2|p= 1|u= 0|p= 0.020921|w= 6|p= 0.68583|z= 0.447214|p= 0.654721|t=-0.098533|p= 0.923933|t= 0.331042|p= 0.757228|t= 1.5|p= 0.208|oddsratio= 20|p= 0.034965|4.8|5|3.3466401061363|11.2|-1.97544642857143|3|0.088041393098|-0.058600903867|5|9|gcdf= 0.800852|gpdf= 0.149361|gppf= 1.678347|gisf= 1.678347|iqr= 2|mode= 1|moment= 2|normcdf= 0.5|normpdf= 0.398942|bern= 0.5|betaCdf= 0.5|betaPdf= 1.5|betaPpf= 0.5|binom= 0.246094|chiCdf= 0.393469|chiPdf= 0.606531|chiPpf= 1.17741|chi2Cdf= 0.393469|chi2Isf= 1.386294|chi2Pdf= 0.303265|chi2Ppf= 1.386294|expCdf= 0.632121|expIsf= 0.693147|expPdf= 0.367879|expPpf= 0.693147|fCdf= 0.534881|fIsf= 0.931933|fPdf= 0.49548|fPpf= 0.931933|gammaCdf= 0.800852|gammaIsf= 1.678347|gammaPdf= 0.149361|gammaPpf= 1.678347|geom= 0.125|hyper= 0.22593|logisticPdf= 0.25|logisticPpf= 0|lognormCdf= 0.5|lognormPdf= 0.398942|lognormPpf= 1|normCdf= 0.5|normIsf= 0|normPdf= 0.398942|normPpf= 0|paretoPdf= 2|paretoPpf= 1.414214|poisson= 0.270671|studCdf= 0.5|studIsf= 0|studPdf= 0.389108|studPpf= 0|triPdf= 1|triPpf= 0.5|uniPdf= 1|uniPpf= 0.5|weiCdf= 0.632121|weiPdf= 0.735759|weiPpf= 0.832555|skewnorm= 0.398942|negbin= 0.15625
IMPORT STATS
IMPORT MATH


SUB S_describe()
  DIM v = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM d = STATS.DESCRIBE(v)
  PRINT "n="; d.Nobs
  PRINT "min="; d.Min
  PRINT "max="; d.Max
  PRINT "mean="; MATH.ROUND(d.Mean, 6)
  PRINT "var="; MATH.ROUND(d.Variance, 6)
  PRINT "skew="; MATH.ROUND(d.Skewness, 6)
  PRINT "kurt="; MATH.ROUND(d.Kurtosis, 6)
END SUB


SUB S_anova()
  DIM groups(1) AS ARRAY OF DOUBLE
  groups(0) = {2.0, 3.0, 4.0, 5.0}
  groups(1) = {4.0, 5.0, 6.0, 7.0}
  DIM r = STATS.ANOVA(groups)
  PRINT "f="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
  PRINT "df="; r.DegreesOfFreedom
END SUB


SUB S_chi2()
  DIM observed = {16.0, 18.0, 16.0, 14.0, 12.0, 12.0}
  DIM expected = {16.0, 16.0, 16.0, 16.0, 12.0, 12.0}
  DIM r = STATS.CHI2(observed, expected)
  PRINT "chi2="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_chi2con()
  DIM tb(1) AS ARRAY OF DOUBLE
  tb(0) = {10, 20}
  tb(1) = {30, 40}
  DIM r = STATS.CHI2CON(tb)
  PRINT "chi2="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
  PRINT "df="; r.DegreesOfFreedom
END SUB


SUB S_pearsonr()
  DIM x = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM y = {2.0, 4.0, 6.0, 8.0, 10.0}
  DIM r = STATS.PEARSONR(x, y)
  PRINT "r="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_spearmanr()
  DIM x = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM y = {2.0, 4.0, 6.0, 8.0, 10.0}
  DIM r = STATS.SPEARMANR(x, y)
  PRINT "rho="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_linregress()
  DIM x = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM y = {2.1, 3.9, 6.2, 7.8, 10.1}
  DIM r = STATS.LINREGRESS(x, y)
  PRINT "slope="; MATH.ROUND(r.Slope, 6)
  PRINT "intercept="; MATH.ROUND(r.Intercept, 6)
  PRINT "r="; MATH.ROUND(r.R, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
  PRINT "se="; MATH.ROUND(r.Stderr, 6)
END SUB


SUB S_normaltest()
  DIM z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0}
  DIM r = STATS.NORMALTEST(z)
  PRINT "k2="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_shapiro()
  DIM normal = {0.12, -0.31, 0.45, 1.02, -0.76, 0.33, 0.18, -0.22, 0.57, -0.41}
  DIM r = STATS.SHAPIRO(normal)
  PRINT "w="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_skewtest()
  DIM z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0}
  DIM r = STATS.SKEWTEST(z)
  PRINT "z="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_kurtosistest()
  DIM z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0}
  DIM r = STATS.KURTOSISTEST(z)
  PRINT "z="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_jarquebera()
  DIM z = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0}
  DIM r = STATS.JARQUEBERA(z)
  PRINT "jb="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_ks()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM y = {1.0, 3.0, 6.0, 8.0, 10.0}
  DIM r = STATS.KS(x, y)
  PRINT "d="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_mannwhitney()
  DIM lo = {1.0, 2.0, 3.0, 4.0}
  DIM hi = {5.0, 6.0, 7.0, 8.0}
  DIM r = STATS.MANNWHITNEY(lo, hi)
  PRINT "u="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_wilcoxon()
  DIM a = {1.0, 2.0, 3.0, 4.0, 5.0}
  DIM b = {2.0, 1.0, 4.0, 3.0, 6.0}
  DIM r = STATS.WILCOXON(a, b)
  PRINT "w="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_ztest()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM r = STATS.ZTEST(x, 5.0, 2.0)
  PRINT "z="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_ttest()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM y = {1.0, 3.0, 6.0, 8.0, 10.0}
  DIM r = STATS.TTEST(x, y)
  PRINT "t="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_ttestonesample()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM r = STATS.TTESTONESAMPLE(x, 5.0)
  PRINT "t="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_ttestpaired()
  DIM x = {2.0, 4.0, 5.0, 7.0, 9.0}
  DIM paired = {1.5, 3.5, 5.5, 6.5, 8.5}
  DIM r = STATS.TTESTPAIRED(x, paired)
  PRINT "t="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_fisher()
  DIM r = STATS.FISHER(8, 2, 1, 5)
  PRINT "oddsratio="; MATH.ROUND(r.Statistic, 6)
  PRINT "p="; MATH.ROUND(r.PValue, 6)
END SUB


SUB S_desc()
  DIM d(4) AS DOUBLE = {5, 2, 9, 1, 7}
  PRINT STATS.MEAN(d)
  PRINT STATS.MEDIAN(d)
  PRINT STATS.STDDEV(d)
  PRINT STATS.VARIANCE(d)
END SUB


SUB S_desc2()
  DIM d(4) AS DOUBLE = {5, 2, 9, 1, 7}
  PRINT STATS.KURTOSIS(d)
  PRINT STATS.MAD(d)
END SUB


SUB S_desc3()
  DIM d(4) AS DOUBLE = {5, 2, 9, 1, 7}
  DIM e(4) AS DOUBLE = {9, 4, 3, 6, 8}
  PRINT ROUND(STATS.SKEWNESS(d), 12)
  PRINT ROUND(STATS.CORRELATION(d, e), 12)
  PRINT STATS.PERCENTILE(d, 50)
  PRINT STATS.PERCENTILE(d, 100)
END SUB


SUB S_dist_extra()
  PRINT "gcdf="; MATH.ROUND(STATS.GammaCdf(2, 1, 3), 6)
  PRINT "gpdf="; MATH.ROUND(STATS.GammaPdf(2, 1, 3), 6)
  PRINT "gppf="; MATH.ROUND(STATS.GammaPpf(2, 1, 0.5), 6)
  PRINT "gisf="; MATH.ROUND(STATS.GammaIsf(2, 1, 0.5), 6)
  DIM a = {1, 2, 3, 4, 5}
  PRINT "iqr="; STATS.Iqr(a)
  PRINT "mode="; STATS.Mode(a)
  PRINT "moment="; STATS.Moment(a, 2)
END SUB


SUB S_dist_normal()
  PRINT "normcdf="; MATH.ROUND(STATS.NORMALCDF(0, 0, 1), 6)
  PRINT "normpdf="; MATH.ROUND(STATS.NORMALPDF(0, 0, 1), 6)
END SUB



SUB Distribuzioni()
  PRINT "bern="; MATH.ROUND(STATS.BERNOULLIPMF(1, 0.5), 6)
  PRINT "betaCdf="; MATH.ROUND(STATS.BETACDF(0.5, 2, 2), 6)
  PRINT "betaPdf="; MATH.ROUND(STATS.BETAPDF(0.5, 2, 2), 6)
  PRINT "betaPpf="; MATH.ROUND(STATS.BETAPPF(0.5, 2, 2), 6)
  PRINT "binom="; MATH.ROUND(STATS.BINOMIALPMF(5, 10, 0.5), 6)
  PRINT "chiCdf="; MATH.ROUND(STATS.CHICDF(1, 2), 6)
  PRINT "chiPdf="; MATH.ROUND(STATS.CHIPDF(1, 2), 6)
  PRINT "chiPpf="; MATH.ROUND(STATS.CHIPPF(0.5, 2), 6)
  PRINT "chi2Cdf="; MATH.ROUND(STATS.CHISQUAREDCDF(1, 2), 6)
  PRINT "chi2Isf="; MATH.ROUND(STATS.CHISQUAREDISF(0.5, 2), 6)
  PRINT "chi2Pdf="; MATH.ROUND(STATS.CHISQUAREDPDF(1, 2), 6)
  PRINT "chi2Ppf="; MATH.ROUND(STATS.CHISQUAREDPPF(0.5, 2), 6)
  PRINT "expCdf="; MATH.ROUND(STATS.EXPONENTIALCDF(1, 1), 6)
  PRINT "expIsf="; MATH.ROUND(STATS.EXPONENTIALISF(0.5, 1), 6)
  PRINT "expPdf="; MATH.ROUND(STATS.EXPONENTIALPDF(1, 1), 6)
  PRINT "expPpf="; MATH.ROUND(STATS.EXPONENTIALPPF(0.5, 1), 6)
  PRINT "fCdf="; MATH.ROUND(STATS.FCDF(1, 5, 10), 6)
  PRINT "fIsf="; MATH.ROUND(STATS.FISF(0.5, 5, 10), 6)
  PRINT "fPdf="; MATH.ROUND(STATS.FPDF(1, 5, 10), 6)
  PRINT "fPpf="; MATH.ROUND(STATS.FPPF(0.5, 5, 10), 6)
  PRINT "gammaCdf="; MATH.ROUND(STATS.GAMMACDF(2, 1, 3), 6)
  PRINT "gammaIsf="; MATH.ROUND(STATS.GAMMAISF(2, 1, 0.5), 6)
  PRINT "gammaPdf="; MATH.ROUND(STATS.GAMMAPDF(2, 1, 3), 6)
  PRINT "gammaPpf="; MATH.ROUND(STATS.GAMMAPPF(2, 1, 0.5), 6)
  PRINT "geom="; MATH.ROUND(STATS.GEOMETRICPMF(3, 0.5), 6)
  PRINT "hyper="; MATH.ROUND(STATS.HYPERGEOMETRICPMF(3, 50, 20, 10), 6)
  PRINT "logisticPdf="; MATH.ROUND(STATS.LOGISTICPDF(0, 0, 1), 6)
  PRINT "logisticPpf="; MATH.ROUND(STATS.LOGISTICPPF(0.5, 0, 1), 6)
  PRINT "lognormCdf="; MATH.ROUND(STATS.LOGNORMALCDF(1, 0, 1), 6)
  PRINT "lognormPdf="; MATH.ROUND(STATS.LOGNORMALPDF(1, 0, 1), 6)
  PRINT "lognormPpf="; MATH.ROUND(STATS.LOGNORMALPPF(0.5, 0, 1), 6)
  PRINT "normCdf="; MATH.ROUND(STATS.NORMALCDF(0, 0, 1), 6)
  PRINT "normIsf="; MATH.ROUND(STATS.NORMALISF(0.5, 0, 1), 6)
  PRINT "normPdf="; MATH.ROUND(STATS.NORMALPDF(0, 0, 1), 6)
  PRINT "normPpf="; MATH.ROUND(STATS.NORMALPPF(0.5, 0, 1), 6)
  PRINT "paretoPdf="; MATH.ROUND(STATS.PARETOPDF(1, 1, 2), 6)
  PRINT "paretoPpf="; MATH.ROUND(STATS.PARETOPPF(0.5, 1, 2), 6)
  PRINT "poisson="; MATH.ROUND(STATS.POISSONPMF(2, 2), 6)
  PRINT "studCdf="; MATH.ROUND(STATS.STUDENTTCDF(0, 10), 6)
  PRINT "studIsf="; MATH.ROUND(STATS.STUDENTTISF(0.5, 10), 6)
  PRINT "studPdf="; MATH.ROUND(STATS.STUDENTTPDF(0, 10), 6)
  PRINT "studPpf="; MATH.ROUND(STATS.STUDENTTPPF(0.5, 10), 6)
  PRINT "triPdf="; MATH.ROUND(STATS.TRIANGULARPDF(0.25, 0, 0.5, 1), 6)
  PRINT "triPpf="; MATH.ROUND(STATS.TRIANGULARPPF(0.5, 0, 0.5, 1), 6)
  PRINT "uniPdf="; MATH.ROUND(STATS.UNIFORMPDF(0.5, 0, 1), 6)
  PRINT "uniPpf="; MATH.ROUND(STATS.UNIFORMPPF(0.5, 0, 1), 6)
  PRINT "weiCdf="; MATH.ROUND(STATS.WEIBULLCDF(1, 2, 1), 6)
  PRINT "weiPdf="; MATH.ROUND(STATS.WEIBULLPDF(1, 2, 1), 6)
  PRINT "weiPpf="; MATH.ROUND(STATS.WEIBULLPPF(0.5, 2, 1), 6)
  PRINT "skewnorm="; MATH.ROUND(STATS.SKEWEDNORMALPDF(0, 0, 1, 0), 6)
  PRINT "negbin="; MATH.ROUND(STATS.NEGATIVEBINOMIALPMF(3, 3, 0.5), 6)
END SUB

SUB Main()
  S_describe()
  S_anova()
  S_chi2()
  S_chi2con()
  S_pearsonr()
  S_spearmanr()
  S_linregress()
  S_normaltest()
  S_shapiro()
  S_skewtest()
  S_kurtosistest()
  S_jarquebera()
  S_ks()
  S_mannwhitney()
  S_wilcoxon()
  S_ztest()
  S_ttest()
  S_ttestonesample()
  S_ttestpaired()
  S_fisher()
  S_desc()
  S_desc2()
  S_desc3()
  S_dist_extra()
  S_dist_normal()
  Distribuzioni()
END SUB
