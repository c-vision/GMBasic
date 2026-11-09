REM FILE: StatsMeanComparison.bas
REM Confronto tra medie con test indipendente, appaiato, a un campione e ANOVA.

IMPORT STATS

SUB PrintResult(label$, result)
    PRINT label$; ": statistic="; result.Statistic; ", p="; result.PValue; ", df="; result.DegreesOfFreedom
END SUB

SUB Main()
    DIM control() = {12.1, 11.8, 12.5, 12.0, 11.6}
    DIM treatment() = {13.4, 12.9, 13.7, 13.1, 12.8}
    DIM afterTreatment() = {12.8, 12.4, 13.0, 12.7, 12.2}
    DIM groups(2, 4) AS DOUBLE
    groups = {{12.1, 11.8, 12.5, 12.0, 11.6}, {13.4, 12.9, 13.7, 13.1, 12.8}, {12.5, 12.7, 12.9, 12.6, 12.8}}

    PrintResult("T test indipendente", STATS.TTEST(control, treatment))
    PrintResult("T test appaiato", STATS.TTESTPAIRED(control, afterTreatment))
    PrintResult("T test contro media 12", STATS.TTESTONESAMPLE(control, 12.0))
    PrintResult("ANOVA a una via", STATS.ANOVA(groups))
END SUB
