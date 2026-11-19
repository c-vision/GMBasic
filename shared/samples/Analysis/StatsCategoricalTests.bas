IMPORT STATS
REM FILE: StatsCategoricalTests.bas
REM Test per frequenze osservate e tabelle di contingenza 2x2.


SUB Main()
    DIM observed() = {26.0, 19.0, 15.0}
    DIM expected() = {20.0, 20.0, 20.0}

    DIM chi2 = STATS.CHI2(observed, expected)
    PRINT "Chi-quadro: statistic="; chi2.Statistic; ", p="; chi2.PValue; ", df="; chi2.DegreesOfFreedom

    REM Tabella 2x2: 8 successi e 2 insuccessi contro 1 successo e 5 insuccessi.
    DIM fisher = STATS.FISHER(8, 2, 1, 5)
    PRINT "Fisher esatto: odds ratio="; fisher.Statistic; ", p="; fisher.PValue
END SUB
